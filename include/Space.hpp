#pragma once

#include "godot_cpp/templates/hash_map.hpp"
#include "godot_cpp/classes/array_mesh.hpp"
#include <cassert>

#include "Tilemap.hpp"
#include "Torus.hpp"
#include "index_vector.hpp"

namespace sbx {

void setup_space_module(const char *name);

using namespace godot;

enum class BodyType : uint8_t {
	TILE,
	STATIC, // zero mass, zero velocity, may be manually moved
	KINEMATIC, // zero mass, velocity set by user, moved by solver
	DYNAMIC, // positive mass, velocity determined by forces, moved by solver
};

struct TileBody {
	Cube cube;
	uint32_t layer;
	bool is_trigger;
};

struct Body;

struct BodyID {
	BodyType type;
	bool is_valid;
	siv::ID id;

	BodyID() :
			type(BodyType::STATIC), is_valid(false), id(0) {}
	BodyID(BodyType type, siv::ID id) :
			type(type), is_valid(true), id(id) {}
	operator bool() const { return is_valid; }

	uint64_t hash() const {
		if (!is_valid)
			return 0;
		return (id << 4) + (uint8_t)type + 1;
	}

	bool operator==(const BodyID &other) const {
		return type == other.type && is_valid == other.is_valid && id == other.id;
	}

	bool operator!=(const BodyID &other) const {
		return !(*this == other);
	}

	struct Hasher {
		static uint32_t hash(const BodyID &bid) {
			return (uint32_t)bid.hash();
		}
	};
};

struct Body {
	BodyType type;
	uint8_t layer;
	bool is_trigger;

	Cube cube;
	float mass;
	int chunk_index;

	BodyID prev;
	BodyID next;

	Vector3 velocity;
	Vector3 instant_velocity;

	Body(BodyType type, Vector3 aabb_extent, float radius01, float mass) :
			type(type),
			layer(0),
			is_trigger(false),
			cube(Vector3(0, 0, 0), aabb_extent, radius01),
			mass(mass),
			chunk_index(-1),
			prev(),
			next(),
			velocity(0, 0, 0),
			instant_velocity(0, 0, 0) {}

	Vector3 position() const { return cube.core.position(); }
	Vector2 position_xz() const { return cube.core.position_xz(); }

	bool is_moving() const {
		return velocity != Vector3(0, 0, 0) || instant_velocity != Vector3(0, 0, 0);
	}
};

struct CollisionPair {
	BodyID a; // non-static
	BodyID b;
	Vector3i xzl;

	CollisionPair(BodyID a, BodyID b, Vector3i xzl) {
		if (a.type > b.type) {
			this->a = a;
			this->b = b;
		} else if (a.type < b.type) {
			this->a = b;
			this->b = a;
		} else {
			assert(a.id != b.id);
			if (a.id > b.id) {
				this->a = a;
				this->b = b;
			} else {
				this->a = b;
				this->b = a;
			}
		}
		this->xzl = xzl;
	}

	bool operator==(const CollisionPair &other) const {
		return a == other.a && b == other.b && xzl == other.xzl;
	}

	bool operator!=(const CollisionPair &other) const {
		return !(*this == other);
	}

	struct Hasher {
		static uint32_t hash(const CollisionPair &pair) {
			uint32_t h1 = (uint32_t)pair.a.hash();
			uint32_t h2 = (uint32_t)pair.b.hash();
			h1 ^= h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
			return h1;
		}
	};

	struct Info {
		Vector3 normal;
		float max_sep;
		bool tagged;
	};
};

struct Space;
using BroadPhaseCallback = void (*)(Space *space, BodyID candidate, Vector3i xzl, void *ctx);

struct CollisionEvent {
	enum class Type : int {
		ADDED,
		REMOVED,
	};

	Type type;
	BodyID a;
	BodyID b;
	Vector3i xzl;
	Vector3 normal;

	CollisionEvent() = default;
	CollisionEvent(CollisionEvent::Type type, BodyID a, BodyID b, Vector3i xzl, Vector3 normal = Vector3(0, 0, 0)) :
			type(type), a(a), b(b), xzl(xzl), normal(normal) {}
};

using CollisionEventHandler = void (*)(const CollisionEvent &ev, void *ctx);

enum class BroadPhaseFlags : uint32_t {
	INCLUDE_TILES = 1 << 0,
	INCLUDE_BODIES = 1 << 1,
	ALL = 0xFFFFFFFF,
};

struct Space {
	Tilemap tilemap;
	Chunker chunker;

	Vector3 gravity;
	uint32_t layer_masks[32];

	BodyID *chunks;
	siv::Vector<Body> nonstatic_bodies;
	siv::Vector<Body> static_bodies;
	siv::Vector<Body> tile_bodies;
	HashMap<TileID, BodyID> tile_body_registry;

	HashMap<CollisionPair, CollisionPair::Info, CollisionPair::Hasher> curr_pairs;
	Vector<CollisionEvent> curr_events;

	Space(Tilemap tilemap, int chunk_size) :
			tilemap(tilemap), chunker(tilemap.width(), tilemap.height(), chunk_size) {
		this->gravity = Vector3(0, 0, 0);
		for (int i = 0; i < 32; i++) {
			layer_masks[i] = 0xFFFFFFFF;
		}
		this->chunks = new BodyID[chunker.n_chunks_x * chunker.n_chunks_y];
	}

	int width() const { return tilemap.width(); }
	int height() const { return tilemap.height(); }
	int body_count() const {
		return nonstatic_bodies.size() + static_bodies.size();
	}

	siv::Vector<Body> *get_body_vector(BodyType type) {
		switch (type) {
			case BodyType::STATIC:
				return &static_bodies;
			case BodyType::DYNAMIC:
				return &nonstatic_bodies;
			case BodyType::KINEMATIC:
				return &nonstatic_bodies;
			case BodyType::TILE:
				return &tile_bodies;
		}
		return nullptr;
	}

	Body *get_body(BodyID bid) {
		assert(bid.is_valid);
		siv::Vector<Body> *v = get_body_vector(bid.type);
		return &v->operator[](bid.id);
	}

	void teleport_body(BodyID bid, Vector3 position) {
		Body *body = get_body(bid);
		position.x = posmodf(position.x, width());
		position.z = posmodf(position.z, height());
		body->cube.core.set_position(position);
		if (bid.type != BodyType::TILE) {
			update_body_chunk(bid);
		}
	}

	void add_curr_pair(BodyID a, BodyID b, Vector3i xzl, Vector3 normal, float max_sep);
	BodyID create_body(BodyType type, Vector3 extent, float radius01);
	void register_tile_body(TileID tile_id, BodyID bid) {
		ERR_FAIL_COND(bid.type != BodyType::TILE);
		ERR_FAIL_COND(tile_body_registry.has(tile_id));
		tile_body_registry.insert(tile_id, bid);
	}
	void destroy_body(BodyID bid, CollisionEventHandler handler, void *handler_ctx);
	void remove_pairs_with_tile(Vector3i xzl, CollisionEventHandler handler, void *handler_ctx);

	void update_body_chunk(BodyID bid);
	void remove_body_chunk(BodyID bid);

	void draw_body(PackedVector3Array* p_array, BodyID bid, Vector3i xzl);
	void draw_chunk_bodies(Ref<ArrayMesh> mesh, bool include_tiles, int x, int y, int w, int h);

	// void point_cast(Vector3 point);
	// void ray_cast(Vector3 from, Vector3 to, float max_distance);
	// void circle_cast(Vector3 center, float radius);
	// void sphere_cast(Vector3 center, float radius);
	// void cube_cast(Vector3 vmin, Vector3 vmax);

	void broad_phase_query(AABB aabb, uint32_t layer_mask, uint32_t flags, void *ctx, BroadPhaseCallback callback);
	void step(float delta, CollisionEventHandler handler, void *handler_ctx);
};

} // namespace sbx