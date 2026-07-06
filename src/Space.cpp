#include "Space.hpp"
#include <cassert>

namespace sbx {

void Space::broad_phase_query(const AABB &aabb, uint32_t layer_mask, bool only_dynamic_type, void *ctx, BroadPhaseCallback callback) {
	if (!only_dynamic_type) {
#define MAX_C_PER_D 64
		int buf_x[MAX_C_PER_D];
		int buf_z[MAX_C_PER_D];

		// detect tiles
		int n_buf_x = sbx::torus_iter_chunks_1d(width(), 1, aabb.vmin.x, aabb.vmax.x, buf_x, MAX_C_PER_D);
		int n_buf_z = sbx::torus_iter_chunks_1d(height(), 1, aabb.vmin.z, aabb.vmax.z, buf_z, MAX_C_PER_D);

		for (int i = 0; i < n_buf_x; ++i) {
			for (int j = 0; j < n_buf_z; ++j) {
				int x = buf_x[i];
				int z = buf_z[j];
				Tile *tile = tilemap.get(x, z);
				Vector3 offset_xz(x + 0.5f, 0, z + 0.5f);
				for (int l = 0; l < (int)TileLayer::COUNT; ++l) {
					// TODO: add a bitmap to accelerate this
					BodyID *bid = tile_body_registry.getptr(tile->data[l]);
					if (bid) {
						Body *candidate = get_body(*bid);
						if (layer_mask & (1U << candidate->layer)) {
							AABB new_aabb = candidate->cube.aabb();
							new_aabb.move(offset_xz);
							if (aabb.intersects(new_aabb)) {
								callback(this, *bid, Vector3i(x, z, l), ctx);
							}
						}
					}
				}
			}
		}
#undef MAX_C_PER_D
	}

	// detect bodies
	for (int i = -1; i <= 1; i++) {
		for (int j = -1; j <= 1; j++) {
			Vector2i chunk_pos = chunker.map_2d(aabb.position_xz());
			BodyID p = chunks[chunker.torus_2d_to_1d(chunk_pos + Vector2i(i, j))];
			while (p) {
				Body *candidate = get_body(p);
				bool should_skip = only_dynamic_type && candidate->type != BodyType::DYNAMIC;
				if (!should_skip) {
					if (layer_mask & (1U << candidate->layer)) {
						if (aabb.intersects(candidate->cube.aabb())) {
							callback(this, p, Vector3i(0, 0, 0), ctx);
						}
					}
				}
				p = candidate->next;
			}
		}
	}
}

void Space::update_body_chunk(BodyID bid) {
	Body *body = get_body(bid);
	assert(body->type != BodyType::TILE);
	int chunk_index = chunker.map_1d(body->position_xz());
	if (body->chunk_index == chunk_index) {
		return;
	}
	remove_body_chunk(bid);
	body->chunk_index = chunk_index;
	BodyID head = chunks[chunk_index];
	body->next = head;
	if (head) {
		get_body(head)->prev = bid;
	}
	chunks[chunk_index] = bid;
}

void Space::remove_body_chunk(BodyID bid) {
	Body *body = get_body(bid);
	if (body->chunk_index < 0) {
		return;
	}
	if (!body->prev) {
		if (body->next) {
			chunks[body->chunk_index] = body->next;
		} else {
			chunks[body->chunk_index] = BodyID();
		}
	} else {
		get_body(body->prev)->next = body->next;
	}
	if (body->next) {
		get_body(body->next)->prev = body->prev;
	}
	body->prev = BodyID();
	body->next = BodyID();
	body->chunk_index = -1;
}

void Space::step(float delta) {
	assert(!locked);
	locked = true;
	begin_clear_curr_pairs();

	// find collision pairs
	for (int i = 0; i < nonstatic_bodies.size(); ++i) {
		auto hwnd = nonstatic_bodies.createHandleFromData(i);
		Body *a = hwnd.operator->();
		BodyID a_bid(a->type, hwnd.getID());
		assert(a->type != BodyType::STATIC);

		std::pair<Vector2i, BodyID> ctx_pair(Vector2i(width(), height()), a_bid);

		if (!a->is_moving()) {
			continue;
		}

		bool only_dynamic_type = a->type == BodyType::KINEMATIC;
		broad_phase_query(a->cube.aabb(), layer_masks[a->layer], only_dynamic_type, (void *)&ctx_pair, [](Space *space, BodyID candidate, Vector3i xzl, void *ctx) {
			std::pair<Vector2i, BodyID> *ctx_pair = (std::pair<Vector2i, BodyID> *)ctx;
			Vector2i torus_size = ctx_pair->first;
			BodyID a_bid = ctx_pair->second;

			Body *a = space->get_body(a_bid);
			Body *b = space->get_body(candidate);

			if (a == b) {
				return;
			}

			// narrow phase
			Vector3 offset_xz(xzl.x + 0.5f, 0, xzl.z + 0.5f);
			AABB a_core = a->cube.core;
			AABB b_core = b->cube.core;
			b_core.move(offset_xz);
			torus_normalize_two_aabb(torus_size.x, torus_size.y, &a_core, &b_core);

			UnitVector3 n;
			float max_sep = a_core.find_max_separation(b_core, &n);

			if (max_sep < 0) {
				space->add_curr_pair(a_bid, candidate, xzl, n.to_vec3(), max_sep);
				// print_line("max_sep: " + rtos(max_sep) + ", n: " + n.to_vec3());
			} else {
				AAFace a_face = a_core.get_face(n.flip());
				AAFace b_face = b_core.get_face(n);

				Vector3 n_alt = a_face.find_closest_distance(b_face);
				float n_alt_length = n_alt.length();
				max_sep = n_alt_length - a->cube.radius - b->cube.radius;

				if (max_sep < 0) {
					if (n_alt_length > FLOAT_EPS) {
						n_alt /= n_alt_length;
					} else {
						n_alt = a->position() - (b->position() + offset_xz);
						n_alt /= n_alt.length();
					}
					space->add_curr_pair(a_bid, candidate, xzl, n_alt, max_sep);
					// print_line("max_sep_alt: " + rtos(max_sep) + ", n_alt: " + n_alt);
				}
			}
		});
	}

	end_clear_curr_pairs();

	// resolve collisions
	for (const auto &[pair, info] : curr_pairs) {
		assert(info.tagged);

		Body *a = get_body(pair.a);
		Body *b = get_body(pair.b);

		if (a->is_trigger || b->is_trigger) {
			continue;
		}

		// a: incident body
		// b: reference body
		assert(info.max_sep < 0);
		assert(pair.a.type != BodyType::STATIC);

		Vector3 n = info.normal;
		const float e = 1.0f; // restitution

		// contact separation
		Vector3 correction = n * (-info.max_sep + PENETRATION_SLOP);
		correction *= PENETRATION_CORRECTION_PERCENTAGE;
		Vector3 v_bias = correction / delta;

		if (a->type == BodyType::DYNAMIC) {
			if (b->type != BodyType::DYNAMIC) {
				// DYNAMIC <-> (STATIC or KINEMATIC or TILE)
				a->instant_velocity += v_bias;
				// impulse-based collision resolution
				Vector3 v_rel = a->velocity;
				float v_rel_n = v_rel.dot(n);
				if (v_rel_n < 0) {
					Vector3 v_delta = -(1 + e) * v_rel_n * n;
					a->velocity += v_delta;
				}
			} else {
				// DYNAMIC <-> DYNAMIC
				float mass_ratio = a->mass / (a->mass + b->mass);
				a->instant_velocity += v_bias * (1 - mass_ratio);
				b->instant_velocity -= v_bias * mass_ratio;
				// impulse-based collision resolution
				Vector3 v_rel = a->velocity - b->velocity;
				float v_rel_n = v_rel.dot(n);
				if (v_rel_n < 0) {
					Vector3 v_delta = -(1 + e) * v_rel_n * n;
					a->velocity += v_delta * (1 - mass_ratio);
					b->velocity -= v_delta * mass_ratio;
				}
			}
		} else {
			// KINEMATIC <-> DYNAMIC
			assert(b->type == BodyType::DYNAMIC);
			b->instant_velocity -= v_bias;
			// impulse-based collision resolution
			Vector3 v_rel = a->velocity - b->velocity;
			float v_rel_n = v_rel.dot(n);
			if (v_rel_n < 0) {
				Vector3 v_delta = -(1 + e) * v_rel_n * n;
				b->velocity -= v_delta;
			}
		}
	}

	// move bodies by its velocity
	for (int i = 0; i < nonstatic_bodies.size(); ++i) {
		auto hwnd = nonstatic_bodies.createHandleFromData(i);
		Body *body = hwnd.operator->();
		BodyID bid(body->type, hwnd.getID());

		if (!body->is_moving()) {
			continue;
		}
		// body->velocity += this->gravity * delta;
		Vector3 total_vel = body->velocity + body->instant_velocity;
		body->instant_velocity.zero();
		body->cube.move(total_vel * delta);
		update_body_chunk(bid);
	}

	assert(locked);
	locked = false;
}

} // namespace sbx