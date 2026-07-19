#include "SpaceSerialize.hpp"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace sbx {

static const int SERIALIZE_VERSION = 1;

static Array encode_body_vector(siv::Vector<Body> &vec) {
	Array out;
	for (size_t idx = 0; idx < vec.size(); idx++) {
		Body &b = vec.getData()[idx];
		siv::ID id = vec.createHandleFromData(idx).getID();

		Array e;
		e.push_back((int)b.type);
		e.push_back((int)b.layer);
		e.push_back(b.is_trigger);
		e.push_back((int64_t)id);
		e.push_back(b.position());
		e.push_back(b.cube.aabb().extent());
		e.push_back(b.cube.radius01());
		e.push_back(b.mass);
		e.push_back(b.friction);
		e.push_back(b.restitution);
		e.push_back(b.velocity);
		e.push_back(b.instant_velocity);
		out.push_back(e);
	}
	return out;
}

static void decode_body_vector(Space &space, const Array &arr) {
	for (int i = 0; i < arr.size(); i++) {
		Array e = arr[i];
		BodyType type = (BodyType)(int)e[0];
		int layer = e[1];
		bool is_trigger = e[2];
		Vector3 position = e[4];
		Vector3 aabb_extent = e[5];
		float radius01 = e[6];
		float mass = e[7];
		float friction = e[8];
		float restitution = e[9];
		Vector3 velocity = e[10];
		Vector3 instant_velocity = e[11];

		BodyID bid = space.create_body(type, aabb_extent, radius01);
		Body *body = space.get_body(bid);
		body->layer = layer;
		body->is_trigger = is_trigger;
		body->mass = mass;
		body->friction = friction;
		body->restitution = restitution;
		body->velocity = velocity;
		body->instant_velocity = instant_velocity;
		space.teleport_body(bid, position);
	}
}

PackedByteArray serialize_space(Space &space) {
	Array root;
	root.push_back(SERIALIZE_VERSION);
	root.push_back(space.gravity);

	Tilemap &tm = space.tilemap;
	root.push_back(tm.full_w);
	root.push_back(tm.full_h);
	root.push_back(tm.slice_x);
	root.push_back(tm.slice_y);
	root.push_back(tm.slice_w);
	root.push_back(tm.slice_h);

	size_t tile_bytes = (size_t)tm.full_w * tm.full_h * sizeof(Tile);
	PackedByteArray tiles;
	tiles.resize(tile_bytes);
	memcpy(tiles.ptrw(), tm.tiles.get(), tile_bytes);
	root.push_back(tiles);

	root.push_back(encode_body_vector(space.nonstatic_bodies));
	root.push_back(encode_body_vector(space.static_bodies));
	root.push_back(encode_body_vector(space.tile_bodies));

	Array registry;
	for (const auto &kv : space.tile_body_registry) {
		Array r;
		r.push_back((int)kv.key);
		r.push_back((int)kv.value.type);
		r.push_back((int64_t)kv.value.id);
		registry.push_back(r);
	}
	root.push_back(registry);

	return UtilityFunctions::var_to_bytes(root);
}

bool deserialize_space(Space &space, const PackedByteArray &bytes) {
	Array root = UtilityFunctions::bytes_to_var(bytes);
	int version = root[0];
	if (version != SERIALIZE_VERSION) {
		return false;
	}

	space.gravity = root[1];

	Tilemap &tm = space.tilemap;
	int full_w = root[2];
	int full_h = root[3];
	if (full_w != tm.full_w || full_h != tm.full_h) {
		return false;
	}
	tm.slice_x = root[4];
	tm.slice_y = root[5];
	tm.slice_w = root[6];
	tm.slice_h = root[7];

	PackedByteArray tiles = root[8];
	size_t tile_bytes = (size_t)full_w * full_h * sizeof(Tile);
	if ((size_t)tiles.size() != tile_bytes) {
		return false;
	}
	memcpy(tm.tiles.get(), tiles.ptr(), tile_bytes);

	decode_body_vector(space, root[9]);
	decode_body_vector(space, root[10]);
	decode_body_vector(space, root[11]);

	Array registry = root[12];
	for (int i = 0; i < registry.size(); i++) {
		Array r = registry[i];
		TileID tile_id = (TileID)(int)r[0];
		BodyType type = (BodyType)(int)r[1];
		siv::ID id = (siv::ID)(int64_t)r[2];
		space.register_tile_body(tile_id, BodyID(type, id));
	}

	return true;
}

} // namespace sbx
