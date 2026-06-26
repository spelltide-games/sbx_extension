#pragma once

#include "godot_cpp/templates/hash_map.hpp"
#include "godot_cpp/templates/hash_set.hpp"
#include <cassert>

#include "Tilemap.hpp"
#include "Torus.hpp"

namespace sbx {

using namespace godot;

enum class BodyType : uint8_t {
	STATIC,
	KINEMATIC,
	DYNAMIC,
};

struct TileBody {
	Cube cube;
	uint32_t layer;
	bool is_trigger;
};

struct Body {
	Body *prev;
	Body *next;
	void *ctx;

	Cube cube;
	Vector3 velocity;
	Vector3 instant_velocity;
	Vector2i chunk_pos;

	BodyType type;
	bool is_trigger;

	uint32_t layer;
	float mass;

	Body(void *ctx, BodyType type, bool is_trigger, uint32_t layer, float mass) :
			prev(nullptr), next(nullptr), ctx(ctx), cube(), velocity(Vector3(0, 0, 0)), instant_velocity(Vector3(0, 0, 0)), chunk_pos(INVALID_CHUNK_POS), type(type), is_trigger(is_trigger), layer(layer), mass(mass) {
	}

	Vector3 position() const { return cube.core.position(); }

	bool is_moving() const {
		return velocity != Vector3(0, 0, 0) || instant_velocity != Vector3(0, 0, 0);
	}
};

struct UnionBody {
	union {
		struct {
			bool is_tile;
			Body *body;
		} obj;
		struct {
			bool is_tile;
			int layer, x, z;
			TileBody *body;
		} tile;
	};

	UnionBody(Body *body) {
		obj.is_tile = false;
		obj.body = body;
	}

	UnionBody(int layer, int x, int z, TileBody *body) {
		tile.is_tile = true;
		tile.layer = layer;
		tile.x = x;
		tile.z = z;
		tile.body = body;
	}

	bool is_tile() const {
		return obj.is_tile;
	}

	uint32_t layer() const {
		if (is_tile()) {
			return tile.layer;
		} else {
			return obj.body->layer;
		}
	}

	bool is_trigger() const {
		if (is_tile()) {
			return tile.body->is_trigger;
		} else {
			return obj.body->is_trigger;
		}
	}

	Cube cube() const {
		if (is_tile()) {
			return tile.body->cube;
		} else {
			return obj.body->cube;
		}
	}

	Vector3 position() const {
		if (is_tile()) {
			return Vector3(tile.x + 0.5f, 0, tile.z + 0.5f);
		} else {
			return obj.body->cube.core.position();
		}
	}

	BodyType type() const {
		if (is_tile()) {
			return BodyType::STATIC;
		} else {
			return obj.body->type;
		}
	}

	bool equals(Body *other) const {
		if (is_tile()) {
			return false;
		} else {
			return obj.body == other;
		}
	}

	bool equals(UnionBody other) const {
		if (is_tile() && other.is_tile()) {
			return tile.layer == other.tile.layer && tile.x == other.tile.x && tile.z == other.tile.z;
		} else if (!is_tile() && !other.is_tile()) {
			return obj.body == other.obj.body;
		} else {
			return false;
		}
	}

	uint64_t hash() const {
		if (is_tile()) {
			// 4 bit layer, 30 bit x, 30 bit z
			uint64_t h = 0;
			h |= (static_cast<uint64_t>(tile.layer) & 0xF) << 60;
			h |= (static_cast<uint64_t>(tile.x) & 0x3FFFFFFF) << 30;
			h |= (static_cast<uint64_t>(tile.z) & 0x3FFFFFFF);
			return h;
		} else {
			return reinterpret_cast<uint64_t>(obj.body);
		}
	}
};

struct CollisionPair {
	Body *a; // non-static
	UnionBody b;

	CollisionPair(Body *a, UnionBody b) :
			a(a), b(b) {}

	bool operator==(const CollisionPair &other) const {
		if (a == other.a && b.equals(other.b)) {
			return true;
		}
		if (other.b.equals(a) && b.equals(other.a)) {
			return true;
		}
		return false;
	}

	bool operator!=(const CollisionPair &other) const {
		return !(*this == other);
	}

	bool is_trigger() const {
		return a->is_trigger || b.is_trigger();
	}

	struct Hasher {
		static uint32_t hash(const CollisionPair &pair) {
			uint64_t a_addr = reinterpret_cast<uint64_t>(pair.a);
			return static_cast<uint32_t>(a_addr ^ pair.b.hash());
		}
	};

	struct Info {
		Vector3 normal;
		float max_sep;
	};
};

struct Space {
	Tilemap tilemap;
	int chunk_size;

	Vector3 gravity;
	int body_count;
	uint32_t layer_masks[32];

	HashMap<Vector2i, Body *> chunks;
	HashMap<TileID, TileBody *> tile_bodies;

	HashSet<Body *> nonstatic_bodies;

	HashMap<CollisionPair, CollisionPair::Info, CollisionPair::Hasher> curr_pairs;
	HashMap<CollisionPair, CollisionPair::Info, CollisionPair::Hasher> prev_pairs;

	void (*pair_added)(Space *space, Body *a, UnionBody b, Vector3 normal);
	void (*pair_removed)(Space *space, Body *a, UnionBody b);

	UnionBody body_from_tile(int layer, int x, int z) {
		TileID *p_tile = tilemap.addr_nocheck(layer, x, z);
		return UnionBody(layer, x, z, tile_bodies[*p_tile]);
	}

	Space(Tilemap tilemap, int chunk_size) :
			tilemap(tilemap), chunk_size(chunk_size) {
		this->body_count = 0;
		this->gravity = Vector3(0, 0, 0);
		for (int i = 0; i < 32; i++) {
			layer_masks[i] = 0xFFFFFFFF;
		}
		this->pair_added = nullptr;
		this->pair_removed = nullptr;
	}

	int width() const { return tilemap.width; }
	int height() const { return tilemap.height; }

	void add_curr_pair(Body *a, UnionBody b, Vector3 normal, float max_sep) {
		CollisionPair pair(a, b);
		if (!curr_pairs.has(pair)) {
			curr_pairs.insert(pair, CollisionPair::Info{ normal, max_sep });
		}
	}

	Body *create_body(Vector3 position, Vector3 extent, float radius01, void *ctx, BodyType type, bool is_trigger, uint32_t layer, float mass) {
		Body *body = new Body(ctx, type, is_trigger, layer, mass);
		body->cube.apply(position, extent, radius01);
		if (type != BodyType::STATIC) {
			nonstatic_bodies.insert(body);
		}
		update_body_chunk(body);
		body_count++;
		return body;
	}

	void destroy_body(Body *body) {
		remove_body_chunk(body);
		if (body->type != BodyType::STATIC) {
			nonstatic_bodies.erase(body);
		}
		body_count--;
		delete body;
	}

	void destroy_tile_body(int layer, int x, int z) {
		// ...
	}

	void update_body_chunk(Body *body) {
		Vector2i chunk_pos = to_chunk(body->position());
		if (body->chunk_pos == chunk_pos) {
			return;
		}
		remove_body_chunk(body);
		body->chunk_pos = chunk_pos;
		Body *head = chunks.has(chunk_pos) ? chunks[chunk_pos] : nullptr;
		body->next = head;
		if (head) {
			head->prev = body;
		}
		chunks[chunk_pos] = body;
	}

	void remove_body_chunk(Body *body) {
		if (body->chunk_pos == INVALID_CHUNK_POS) {
			return;
		}
		if (!body->prev) {
			if (body->next) {
				chunks[body->chunk_pos] = body->next;
			} else {
				chunks.erase(body->chunk_pos);
			}
		} else {
			body->prev->next = body->next;
		}
		if (body->next) {
			body->next->prev = body->prev;
		}
		body->prev = nullptr;
		body->next = nullptr;
		body->chunk_pos = INVALID_CHUNK_POS;
	}

	Vector2i to_chunk(Vector3 position) const {
		return Vector2i(
				static_cast<int>(floor(position.x / chunk_size)),
				static_cast<int>(floor(position.z / chunk_size)));
	}

	// void point_cast(Vector3 point);
	// void ray_cast(Vector3 from, Vector3 to, float max_distance);
	// void circle_cast(Vector3 center, float radius);
	// void sphere_cast(Vector3 center, float radius);
	// void cube_cast(Vector3 vmin, Vector3 vmax);

	void step(float delta);

	~Space() {
		for (auto &[chunk_pos, body] : chunks) {
			Body *curr = body;
			while (curr) {
				Body *next = curr->next;
				delete curr;
				curr = next;
			}
		}
	}
};

} // namespace sbx