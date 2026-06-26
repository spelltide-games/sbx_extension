#include "Space.hpp"
#include <cassert>

namespace sbx {

static void broad_phase_query(Space *space, const AABB &aabb, uint32_t layer_mask, void *ctx, void (*callback)(Space *space, UnionBody candidate, void *ctx)) {
#define MAX_C_PER_D 64
	int buf_x[MAX_C_PER_D];
	int buf_z[MAX_C_PER_D];

	// detect tiles
	int n_buf_x = sbx::torus_iter_chunks_1d(space->width(), 1, aabb.vmin.x, aabb.vmax.x, buf_x, MAX_C_PER_D);
	int n_buf_z = sbx::torus_iter_chunks_1d(space->height(), 1, aabb.vmin.z, aabb.vmax.z, buf_z, MAX_C_PER_D);

	const Tilemap &tilemap = space->tilemap;
	for (int i = 0; i < n_buf_x; ++i) {
		for (int j = 0; j < n_buf_z; ++j) {
			for (int k = 0; k < tilemap.n_layers; ++k) {
				UnionBody candidate = space->body_from_tile(k, buf_x[i], buf_z[j]);
				if (layer_mask & (1U << candidate.layer())) {
					if (aabb.intersects(candidate.cube().aabb())) {
						callback(space, candidate, ctx);
					}
				}
			}
		}
	}
#undef MAX_C_PER_D

	// detect bodies
	for (int i = -1; i <= 1; i++) {
		for (int j = -1; j <= 1; j++) {
			Vector2i chunk_pos = space->to_chunk(aabb.position()) + Vector2i(i, j);
			Body **p_candidates = space->chunks.getptr(chunk_pos);
			if (p_candidates) {
				Body *candidate = *p_candidates;
				while (candidate) {
					if (layer_mask & (1U << candidate->layer)) {
						if (aabb.intersects(candidate->cube.aabb())) {
							callback(space, UnionBody(candidate), ctx);
						}
					}
					candidate = candidate->next;
				}
			}
		}
	}
}

void Space::step(float delta) {
	// find collision pairs
	for (Body *a : nonstatic_bodies) {
		assert(a->type != BodyType::STATIC);

		if (!a->is_moving()) {
			continue;
		}

		broad_phase_query(this, a->cube.aabb(), layer_masks[a->layer], (void *)a, [](Space *space, UnionBody candidate, void *ctx) {
			Body *a = (Body *)ctx;
			UnionBody b = candidate;

			if (b.equals(a)) {
				return;
			}

			// narrow phase
			AABB a_core = a->cube.core;
			AABB b_core = b.cube().core;
			a_core.move_both_to_origin(&b_core); // TODO: torus

			UnitVector3 n;
			float max_sep = a_core.find_max_separation(b_core, &n);

			if (max_sep < 0) {
				space->add_curr_pair(a, b, n.to_vec3(), max_sep);
				// print_line("max_sep: " + rtos(max_sep) + ", n: " + n.to_vec3());
			} else {
				AAFace a_face = a_core.get_face(n.flip());
				AAFace b_face = b_core.get_face(n);

				Vector3 n_alt = a_face.find_closest_distance(b_face);
				float n_alt_length = n_alt.length();
				max_sep = n_alt_length - a->cube.radius - b.cube().radius;

				if (max_sep < 0) {
					if (n_alt_length > FLOAT_EPS) {
						n_alt /= n_alt_length;
					} else {
						n_alt = a->position() - b.position();
						n_alt /= n_alt.length();
					}
					space->add_curr_pair(a, b, n_alt, max_sep);
					// print_line("max_sep_alt: " + rtos(max_sep) + ", n_alt: " + n_alt);
				}
			}
		});
	}

	// resolve collisions
	for (const auto &[pair, info] : curr_pairs) {
		if (pair.is_trigger())
			continue;

		// a: incident body
		// b: reference body
		assert(info.max_sep < 0);

		Vector3 n = info.normal;
		const float e = 1.0f; // restitution

		// contact separation
		Vector3 correction = n * (-info.max_sep + PENETRATION_SLOP);
		correction *= PENETRATION_CORRECTION_PERCENTAGE;
		Vector3 v_bias = correction / delta;

		assert(pair.a->type != BodyType::STATIC);

		if (pair.a->type == BodyType::DYNAMIC) {
			if (pair.b.type() != BodyType::DYNAMIC) {
				// DYNAMIC <-> (STATIC or KINEMATIC)
				pair.a->instant_velocity += v_bias;
				// impulse-based collision resolution
				Vector3 v_rel = pair.a->velocity;
				float v_rel_n = v_rel.dot(n);
				if (v_rel_n < 0) {
					Vector3 v_delta = -(1 + e) * v_rel_n * n;
					pair.a->velocity += v_delta;
				}
			} else {
				// DYNAMIC <-> DYNAMIC
				Body *b_body = pair.b.obj.body;
				float mass_ratio = pair.a->mass / (pair.a->mass + b_body->mass);
				pair.a->instant_velocity += v_bias * (1 - mass_ratio);
				b_body->instant_velocity -= v_bias * mass_ratio;
				// impulse-based collision resolution
				Vector3 v_rel = pair.a->velocity - b_body->velocity;
				float v_rel_n = v_rel.dot(n);
				if (v_rel_n < 0) {
					Vector3 v_delta = -(1 + e) * v_rel_n * n;
					pair.a->velocity += v_delta * (1 - mass_ratio);
					b_body->velocity -= v_delta * mass_ratio;
				}
			}
		} else {
			switch (pair.b.type()) {
				case BodyType::STATIC:
					// KINEMATIC <-> STATIC
					break;
				case BodyType::KINEMATIC:
					// KINEMATIC <-> KINEMATIC
					break;
				case BodyType::DYNAMIC:
					// KINEMATIC <-> DYNAMIC
					Body *b_body = pair.b.obj.body;
					b_body->instant_velocity -= v_bias;
					// impulse-based collision resolution
					Vector3 v_rel = pair.a->velocity - b_body->velocity;
					float v_rel_n = v_rel.dot(n);
					if (v_rel_n < 0) {
						Vector3 v_delta = -(1 + e) * v_rel_n * n;
						b_body->velocity -= v_delta;
					}
					break;
			}
		}
	}

	// send signals
	if (pair_added) {
		for (const auto &[pair, info] : curr_pairs) {
			if (!prev_pairs.has(pair)) {
				pair_added(this, pair.a, pair.b, info.normal);
			}
		}
	}
	if (pair_removed) {
		for (const auto &[pair, info] : prev_pairs) {
			if (!curr_pairs.has(pair)) {
				pair_removed(this, pair.a, pair.b);
			}
		}
	}
	prev_pairs = std::move(curr_pairs);
	curr_pairs.clear();

	// move bodies by its velocity
	for (Body *body : nonstatic_bodies) {
		if (!body->is_moving()) {
			continue;
		}
		// body->velocity += this->gravity * delta;
		Vector3 total_vel = body->velocity + body->instant_velocity;
		body->instant_velocity.zero();
		body->cube.move(total_vel * delta);
		update_body_chunk(body);
	}
}

} // namespace sbx