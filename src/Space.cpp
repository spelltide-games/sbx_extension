#include "Space.hpp"
#include <cassert>

namespace sbx {

void Space::broad_phase_query(const AABB &aabb, uint32_t layer_mask, uint32_t flags, void *ctx, BroadPhaseCallback callback) {
	if (flags & (uint32_t)BroadPhaseFlags::INCLUDE_TILES) {
#define MAX_C_PER_D 64
		int buf_x[MAX_C_PER_D];
		int buf_z[MAX_C_PER_D];

		// detect tiles
		int n_buf_x = sbx::torus_iter_chunks_1d(width(), 1, aabb.vmin.x, aabb.vmax.x, buf_x, MAX_C_PER_D);
		int n_buf_z = sbx::torus_iter_chunks_1d(height(), 1, aabb.vmin.z, aabb.vmax.z, buf_z, MAX_C_PER_D);

		assert(n_buf_x <= MAX_C_PER_D);
		assert(n_buf_z <= MAX_C_PER_D);

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

	if (flags & (uint32_t)BroadPhaseFlags::INCLUDE_BODIES) {
		// detect bodies
		for (int i = -1; i <= 1; i++) {
			for (int j = -1; j <= 1; j++) {
				Vector2i chunk_pos = chunker.map_2d(aabb.position_xz());
				BodyID p = chunks[chunker.torus_2d_to_1d(chunk_pos + Vector2i(i, j))];
				while (p) {
					Body *candidate = get_body(p);
					if (layer_mask & (1U << candidate->layer)) {
						if (aabb.intersects(candidate->cube.aabb())) {
							callback(this, p, Vector3i(0, 0, 0), ctx);
						}
					}
					p = candidate->next;
				}
			}
		}
	}
}

void Space::update_body_chunk(BodyID bid) {
	ERR_FAIL_COND(bid.type == BodyType::TILE);
	Body *body = get_body(bid);
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

void Space::add_curr_pair(BodyID a, BodyID b, Vector3i xzl, Vector3 normal, float max_sep) {
	CollisionPair pair(a, b, xzl);
	CollisionPair::Info *p_info = curr_pairs.getptr(pair);
	CollisionPair::Info new_info{ normal, max_sep, true };
	if (p_info) {
		*p_info = new_info;
	} else {
		curr_pairs.insert(pair, new_info);
		curr_events.push_back(CollisionEvent(CollisionEvent::Type::ADDED, a, b, xzl, normal));
	}
}

BodyID Space::create_body(BodyType type, Vector3 aabb_extent, float radius01) {
	switch (type) {
		case BodyType::DYNAMIC: {
			siv::ID id = nonstatic_bodies.emplace_back(BodyType::DYNAMIC, aabb_extent, radius01, 1.0f);
			BodyID bid(BodyType::DYNAMIC, id);
			update_body_chunk(bid);
			return bid;
		}
		case BodyType::KINEMATIC: {
			siv::ID id = nonstatic_bodies.emplace_back(BodyType::KINEMATIC, aabb_extent, radius01, INFINITY);
			BodyID bid(BodyType::KINEMATIC, id);
			update_body_chunk(bid);
			return bid;
		}
		case BodyType::STATIC: {
			siv::ID id = static_bodies.emplace_back(BodyType::STATIC, aabb_extent, radius01, INFINITY);
			BodyID bid(BodyType::STATIC, id);
			update_body_chunk(bid);
			return bid;
		}
		case BodyType::TILE: {
			siv::ID id = tile_bodies.emplace_back(BodyType::TILE, aabb_extent, radius01, INFINITY);
			BodyID bid(BodyType::TILE, id);
			return bid;
		}
	}
	return BodyID();
}

void Space::destroy_body(BodyID bid, CollisionEventHandler handler, void *handler_ctx) {
	ERR_FAIL_COND(bid.type == BodyType::TILE);
	// remove from curr_pairs
	auto it = curr_pairs.begin();
	while (it) {
		auto next = it;
		++next;
		if (it->key.a == bid || it->key.b == bid) {
			assert(it->value.tagged);
			CollisionEvent ev(CollisionEvent::Type::REMOVED, it->key.a, it->key.b, it->key.xzl);
			handler(ev, handler_ctx);
			curr_pairs.remove(it);
		}
		it = next;
	}
	remove_body_chunk(bid);
	get_body_vector(bid.type)->erase(bid.id);
}

void Space::remove_pairs_with_tile(Vector3i xzl, CollisionEventHandler handler, void *handler_ctx) {
	// remove from curr_pairs
	auto it = curr_pairs.begin();
	while (it) {
		auto next = it;
		++next;
		if (it->key.xzl == xzl) {
			assert(it->key.b.type == BodyType::TILE);
			assert(it->value.tagged);
			CollisionEvent ev(CollisionEvent::Type::REMOVED, it->key.a, it->key.b, it->key.xzl);
			handler(ev, handler_ctx);
			curr_pairs.remove(it);
		}
		it = next;
	}
}

void Space::step(float delta, CollisionEventHandler handler, void *handler_ctx) {
	// begin pairs
	curr_events.clear();
	for (auto &kv : curr_pairs) {
		kv.value.tagged = false;
	}

	// find collision pairs
	for (int i = 0; i < nonstatic_bodies.size(); ++i) {
		auto hwnd = nonstatic_bodies.createHandleFromData(i);
		Body *a = hwnd.operator->();
		BodyID a_bid(a->type, hwnd.getID());
		assert(a->type != BodyType::STATIC);

		std::pair<Vector2i, BodyID> ctx_pair(Vector2i(width(), height()), a_bid);

		// this causes bug for curr_pairs
		// if (!a->is_moving()) {
		// 	continue;
		// }

		uint32_t flags = (uint32_t)BroadPhaseFlags::ALL;
		broad_phase_query(a->cube.aabb(), layer_masks[a->layer], flags, (void *)&ctx_pair, [](Space *space, BodyID candidate, Vector3i xzl, void *ctx) {
			std::pair<Vector2i, BodyID> *ctx_pair = (std::pair<Vector2i, BodyID> *)ctx;
			Vector2i torus_size = ctx_pair->first;
			BodyID a_bid = ctx_pair->second;

			Body *a = space->get_body(a_bid);
			Body *b = space->get_body(candidate);

			if (a == b) {
				return;
			}

			// narrow phase
			AABB a_core = a->cube.core;
			AABB b_core = b->cube.core;
			if (b->type == BodyType::TILE) {
				b_core.move(Vector3(xzl.x + 0.5f, 0, xzl.y + 0.5f));
			}
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
						n_alt = a_core.position() - b_core.position();
						n_alt /= n_alt.length();
					}
					space->add_curr_pair(a_bid, candidate, xzl, n_alt, max_sep);
					// print_line("max_sep_alt: " + rtos(max_sep) + ", n_alt: " + n_alt);
				}
			}
		});
	}

	// end pairs
	auto it = curr_pairs.begin();
	while (it) {
		auto next = it;
		++next;
		if (!it->value.tagged) {
			CollisionEvent ev(CollisionEvent::Type::REMOVED, it->key.a, it->key.b, it->key.xzl);
			curr_events.push_back(ev);
			curr_pairs.remove(it);
		}
		it = next;
	}

	// resolve collisions
	for (const auto &[pair, info] : curr_pairs) {
		assert(info.tagged);

		Body *a = get_body(pair.a);
		Body *b = get_body(pair.b);

		if (a->is_trigger || b->is_trigger) {
			continue;
		}

		if (a->type == BodyType::KINEMATIC && b->type != BodyType::DYNAMIC) {
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
		body->cube.torus_move(total_vel * delta, width(), height());
		update_body_chunk(bid);
	}

	// dispatch events
	for (int i = 0; i < curr_events.size(); ++i) {
		const CollisionEvent &ev = curr_events[i];
		handler(ev, handler_ctx);
	}
}

static void f_draw(Ref<ArrayMesh> mesh, const PackedVector3Array &points, Color color) {
	Array surface;
	surface.resize(Mesh::ARRAY_MAX);
	surface[Mesh::ARRAY_VERTEX] = points;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINE_STRIP, surface);
}

void Space::draw_body(PackedVector3Array *p_array, BodyID bid, Vector3i xzl) {
	Body *body = get_body(bid);
	AABB core = body->cube.core;
	if (body->type == BodyType::TILE) {
		core.move(Vector3(xzl.x + 0.5f, 0, xzl.y + 0.5f));
	}

	auto add_as_lines = [](PackedVector3Array *p_array, PackedVector3Array points) {
		for (int i = 0; i < points.size(); ++i) {
			Vector3 p0 = points[i];
			Vector3 p1 = points[(i + 1) % points.size()];
			p_array->append(p0);
			p_array->append(p1);
		}
	};

	auto rr = [](const AABB &core, float radius, float t, int axis) {
		const int corner_res = 8;
		PackedVector3Array points;
		int u;
		int v;
		if (axis == 0) { // 法线为X轴
			u = 1;
			v = 2;
		} else if (axis == 1) { // 法线为Y轴
			u = 0;
			v = 2;
		} else { // 法线为Z轴
			u = 0;
			v = 1;
		}

		float min_val = core.vmin[axis] - radius;
		float max_val = core.vmax[axis] + radius;
		float curr_val = Math::lerp(min_val, max_val, t);

		auto make_vec3 = [=](float val_axis, float val_u, float val_v) {
			Vector3 vec(0, 0, 0);
			vec[axis] = val_axis;
			vec[u] = val_u;
			vec[v] = val_v;
			return vec;
		};

		if (radius == 0.0f) {
			points.append(make_vec3(curr_val, core.vmax[u], core.vmax[v]));
			points.append(make_vec3(curr_val, core.vmax[u], core.vmin[v]));
			points.append(make_vec3(curr_val, core.vmin[u], core.vmin[v]));
			points.append(make_vec3(curr_val, core.vmin[u], core.vmax[v]));
			return points;
		}

		// 圆角区域的深度 dy
		float dy = 0.0;
		if (curr_val < core.vmin[axis]) {
			dy = core.vmin[axis] - curr_val;
		} else if (curr_val > core.vmax[axis]) {
			dy = curr_val - core.vmax[axis];
		}

		float r = Math::sqrt(Math::max<float>(0.0, radius * radius - dy * dy));

		const float PI = 3.14159265358979323846;

		// 第一象限 (+u, +v)
		for (int j = 0; j <= corner_res; j++) {
			float ang = float(j) / corner_res * (PI / 2.0);
			points.append(make_vec3(curr_val, core.vmax[u] + r * Math::sin(ang), core.vmax[v] + r * Math::cos(ang)));
		}

		// 第二象限 (+u, -v)
		for (int j = 0; j <= corner_res; j++) {
			float ang = PI / 2.0 + float(j) / corner_res * (PI / 2.0);
			points.append(make_vec3(curr_val, core.vmax[u] + r * Math::sin(ang), core.vmin[v] + r * Math::cos(ang)));
		}

		// 第三象限 (-u, -v)
		for (int j = 0; j <= corner_res; j++) {
			float ang = PI + float(j) / corner_res * (PI / 2.0);
			points.append(make_vec3(curr_val, core.vmin[u] + r * Math::sin(ang), core.vmin[v] + r * Math::cos(ang)));
		}

		// 第四象限 (-u, +v)
		for (int j = 0; j <= corner_res; j++) {
			float ang = 3.0 * PI / 2.0 + float(j) / corner_res * (PI / 2.0);
			points.append(make_vec3(curr_val, core.vmin[u] + r * Math::sin(ang), core.vmax[v] + r * Math::cos(ang)));
		}

		return points;
	};

	Vector3 aabb_size = body->cube.aabb().extent() * 2;
	for (int axis = 0; axis <= 2; axis++) {
		float t = body->cube.radius / aabb_size[axis];
		add_as_lines(p_array, rr(core, body->cube.radius, t, axis));
		add_as_lines(p_array, rr(core, body->cube.radius, 1 - t, axis));
	}
}

void Space::draw_chunk_bodies(Ref<ArrayMesh> mesh, bool include_tiles, int x, int y, int w, int h) {
	Vector2i chunk_pos(x, y);
	Vector2i max_chunk_pos = chunk_pos + Vector2i(w, h);
	max_chunk_pos.x = Math::min(max_chunk_pos.x, chunker.n_chunks_x);
	max_chunk_pos.y = Math::min(max_chunk_pos.y, chunker.n_chunks_y);
	static PackedVector3Array lines_tile;
	static PackedVector3Array lines_body;
	lines_tile.clear();
	lines_body.clear();
	for (int cx = chunk_pos.x; cx < max_chunk_pos.x; ++cx) {
		for (int cz = chunk_pos.y; cz < max_chunk_pos.y; ++cz) {
			// bodies
			int chunk_index = chunker.torus_2d_to_1d(Vector2i(cx, cz));
			BodyID p = chunks[chunk_index];
			while (p) {
				draw_body(&lines_body, p, Vector3i(0, 0, 0));
				p = get_body(p)->next;
			}
			// tiles
			if (include_tiles) {
				int x_, y_, w_, h_;
				chunker.get_slice(Vector2i(cx, cz), &x_, &y_, &w_, &h_);
				for (int i = x_; i < x_ + w_; ++i) {
					for (int j = y_; j < y_ + h_; ++j) {
						Tile *tile = tilemap.get(i, j);
						for (int l = 0; l < (int)TileLayer::COUNT; ++l) {
							BodyID *bid = tile_body_registry.getptr(tile->data[l]);
							if (bid) {
								draw_body(&lines_tile, *bid, Vector3i(i, j, l));
							}
						}
					}
				}
			}
		}
	}

	Array surface;
	surface.resize(Mesh::ARRAY_MAX);
	if (include_tiles) {
		surface[Mesh::ARRAY_VERTEX] = lines_tile;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, surface);
		surface[Mesh::ARRAY_VERTEX] = lines_body;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, surface);
	} else {
		assert(mesh->get_surface_count() == 1);
		surface[Mesh::ARRAY_VERTEX] = lines_body;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, surface);
	}
}

} // namespace sbx