#include "godot_cpp/variant/variant.hpp"
#include <cassert>

#include "Space.hpp"

namespace sbx {

template <typename T>
Variant to_var(const T *p) {
	return *p;
}

template <typename T>
void from_var(T *p, Variant v) {
	*p = v;
}

// HashMap<K, V, H>
template <typename K, typename V, typename H>
Variant to_var(const HashMap<K, V, H> *p) {
	Array a;
	for (const auto &[k, v] : *p) {
		a.append(to_var(&k));
		a.append(to_var(&v));
	}
	return a;
}

template <typename K, typename V, typename H>
void from_var(HashMap<K, V, H> *p, Variant v) {
	Array a = v;
	p->clear();
	assert(a.size() % 2 == 0);
	for (int i = 0; i < a.size(); i += 2) {
		K k;
		V v;
		from_var(&k, a[i]);
		from_var(&v, a[i + 1]);
		p->insert(k, v);
	}
}

// Vector<T>
template <typename T>
Variant to_var(const std::vector<T> *p) {
	Array a;
	a.resize(p->size());
	for (int i = 0; i < p->size(); ++i) {
		a[i] = to_var(&p->operator[](i));
	}
	return a;
}

template <typename T>
void from_var(std::vector<T> *p, Variant v) {
	Array a = v;
	p->resize(a.size());
	for (int i = 0; i < a.size(); ++i) {
		T *p_elem = &p->operator[](i);
		from_var(p_elem, a[i]);
	}
}

// Tilemap
template <>
Variant to_var(const Tilemap *p) {
	Array a;
	a.append(p->full_w);
	a.append(p->full_h);
	a.append(p->slice_x);
	a.append(p->slice_y);
	a.append(p->slice_w);
	a.append(p->slice_h);
	PackedByteArray b_tiles;
	b_tiles.resize(p->full_w * p->full_h * sizeof(Tile));
	std::memcpy(b_tiles.ptrw(), p->tiles.get(), b_tiles.size());
	a.append(b_tiles);
	return a;
}

template <>
void from_var(Tilemap *p, Variant v) {
	Array a = v;
	assert(a.size() == 7);
	p->full_w = a[0];
	p->full_h = a[1];
	p->slice_x = a[2];
	p->slice_y = a[3];
	p->slice_w = a[4];
	p->slice_h = a[5];
	PackedByteArray b_tiles = a[6];
	size_t expected_size = p->full_w * p->full_h * sizeof(Tile);
	assert(b_tiles.size() == expected_size);
	p->tiles = std::shared_ptr<Tile>(new Tile[p->full_w * p->full_h], std::default_delete<Tile[]>());
	std::memcpy(p->tiles.get(), b_tiles.ptr(), b_tiles.size());
}

// Chunker
template <>
Variant to_var(const Chunker *p) {
	Array a;
	a.append(p->width);
	a.append(p->height);
	a.append(p->chunk_size);
	a.append(p->n_chunks_x);
	a.append(p->n_chunks_y);
	return a;
}

template <>
void from_var(Chunker *p, Variant v) {
	Array a = v;
	assert(a.size() == 5);
	p->width = a[0];
	p->height = a[1];
	p->chunk_size = a[2];
	p->n_chunks_x = a[3];
	p->n_chunks_y = a[4];
}

// BodyID
template <>
Variant to_var(const BodyID *p) {
	if (p->is_valid) {
		return Array::make((int)p->type, p->id);
	}
	return {};
}

template <>
void from_var(BodyID *p, Variant v) {
	if (v.get_type() == Variant::NIL) {
		p->type = BodyType::STATIC;
		p->is_valid = false;
		p->id = 0;
	} else {
		Array a = v;
		assert(a.size() == 2);
		p->type = (BodyType)(int)a[0];
		p->is_valid = true;
		p->id = (siv::ID)a[1];
	}
}

// CollisionPair
template <>
Variant to_var(const CollisionPair *p) {
	Array a;
	a.append(to_var(&p->a));
	a.append(to_var(&p->b));
	a.append(p->xzl);
	return a;
}

template <>
void from_var(CollisionPair *p, Variant v) {
	Array a = v;
	assert(a.size() == 3);
	from_var(&p->a, a[0]);
	from_var(&p->b, a[1]);
	p->xzl = a[2];
}

// CollisionPair::Info
template <>
Variant to_var(const CollisionPair::Info *p) {
	Array a;
	a.append(p->normal);
	a.append(p->max_sep);
	a.append(p->tagged);
	return a;
}

template <>
void from_var(CollisionPair::Info *p, Variant v) {
	Array a = v;
	assert(a.size() == 3);
	p->normal = a[0];
	p->max_sep = a[1];
	p->tagged = a[2];
}

// CollisionEvent
template <>
Variant to_var(const CollisionEvent *p) {
	Array a;
	a.append((int)p->type);
	a.append(to_var(&p->a));
	a.append(to_var(&p->b));
	a.append(p->xzl);
	a.append(p->normal);
	a.append(p->max_sep);
	return a;
}

template <>
void from_var(CollisionEvent *p, Variant v) {
	Array a = v;
	assert(a.size() == 6);
	p->type = (CollisionEvent::Type)(int)a[0];
	from_var(&p->a, a[1]);
	from_var(&p->b, a[2]);
	p->xzl = a[3];
	p->normal = a[4];
	p->max_sep = a[5];
}

// uint32_t[32]
template <>
Variant to_var(const uint32_t p[32]) {
	PackedByteArray b;
	b.resize(32 * sizeof(uint32_t));
	std::memcpy(b.ptrw(), p, b.size());
	return b;
}

template <>
void from_var(uint32_t p[32], Variant v) {
	PackedByteArray b = v;
	assert(b.size() == 32 * sizeof(uint32_t));
	std::memcpy(p, b.ptr(), b.size());
}

// BodyIDChunks
template <>
Variant to_var(const BodyIDChunks *p) {
	Array a;
	for (int i = 0; i < p->length; ++i) {
		a.append(to_var(&p->data[i]));
	}
	return a;
}

template <>
void from_var(BodyIDChunks *p, Variant v) {
	Array a = v;
	assert(p->data == nullptr);
	p->data = new BodyID[a.size()];
	p->length = a.size();
	for (int i = 0; i < p->length; ++i) {
		from_var(&p->data[i], a[i]);
	}
}

// Cube
template <>
Variant to_var(const Cube *p) {
	Array a;
	a.append(p->core.vmin);
	a.append(p->core.vmax);
	a.append(p->radius);
	return a;
}

template <>
void from_var(Cube *p, Variant v) {
	Array a = v;
	assert(a.size() == 3);
	p->core.vmin = a[0];
	p->core.vmax = a[1];
	p->radius = a[2];
}

// Body
template <>
Variant to_var(const Body *p) {
	Array a;
	a.append((int)p->type);
	a.append(p->layer);
	a.append(p->is_trigger);

	a.append(to_var(&p->cube));
	a.append(p->mass);
	a.append(p->friction);
	a.append(p->restitution);
	a.append(p->chunk_index);

	a.append(to_var(&p->prev));
	a.append(to_var(&p->next));

	a.append(p->velocity);
	a.append(p->instant_velocity);
	return a;
}

template <>
void from_var(Body *p, Variant v) {
	Array a = v;
	assert(a.size() == 12);
	p->type = (BodyType)(int)a[0];
	p->layer = a[1];
	p->is_trigger = a[2];

	from_var(&p->cube, a[3]);
	p->mass = a[4];
	p->friction = a[5];
	p->restitution = a[6];
	p->chunk_index = a[7];

	from_var(&p->prev, a[8]);
	from_var(&p->next, a[9]);

	p->velocity = a[10];
	p->instant_velocity = a[11];
}

// siv::Vector<Body>
template <>
Variant to_var(const siv::Vector<Body> *p) {
	Array a;
	Array a_m_data, a_m_metadata, a_m_indexes;
	for (int i = 0; i < p->m_data.size(); ++i) {
		a_m_data.append(to_var(&p->m_data[i]));
	}
	for (int i = 0; i < p->m_metadata.size(); ++i) {
		a_m_metadata.append(p->m_metadata[i].rid);
		a_m_metadata.append(p->m_metadata[i].validity_id);
	}
	for (int i = 0; i < p->m_indexes.size(); ++i) {
		a_m_indexes.append(p->m_indexes[i]);
	}
	a.append(a_m_data);
	a.append(a_m_metadata);
	a.append(a_m_indexes);
	return a;
}

template <>
void from_var(siv::Vector<Body> *p, Variant v) {
	Array a = v;
	assert(a.size() == 3);
	Array a_m_data = a[0];
	Array a_m_metadata = a[1];
	Array a_m_indexes = a[2];
	p->m_data.resize(a_m_data.size());
	for (int i = 0; i < a_m_data.size(); ++i) {
		from_var(&p->m_data[i], a_m_data[i]);
	}
	assert(a_m_metadata.size() % 2 == 0);
	p->m_metadata.resize(a_m_metadata.size() / 2);
	for (int i = 0; i < a_m_metadata.size(); i += 2) {
		p->m_metadata[i >> 1].rid = (siv::ID)a_m_metadata[i];
		p->m_metadata[i >> 1].validity_id = (siv::ID)a_m_metadata[i + 1];
	}
	p->m_indexes.resize(a_m_indexes.size());
	for (int i = 0; i < a_m_indexes.size(); ++i) {
		p->m_indexes[i] = (siv::ID)a_m_indexes[i];
	}
}

// Space
Variant space_to_var(const Space *p) {
	Dictionary d;
	d["tilemap"] = to_var(&p->tilemap);
	d["chunker"] = to_var(&p->chunker);

	d["gravity"] = p->gravity;
	d["layer_masks"] = to_var(p->layer_masks);

	d["chunks"] = to_var(&p->chunks);
	d["nonstatic_bodies"] = to_var(&p->nonstatic_bodies);
	d["static_bodies"] = to_var(&p->static_bodies);
	d["tile_bodies"] = to_var(&p->tile_bodies);
	d["tile_body_registry"] = to_var(&p->tile_body_registry);

	d["curr_pairs"] = to_var(&p->curr_pairs);
	d["curr_events"] = to_var(&p->curr_events);

	return d;
}

void space_from_var(Space *p, Variant v) {
	Dictionary d = v;
	from_var(&p->tilemap, d["tilemap"]);
	from_var(&p->chunker, d["chunker"]);

	p->gravity = d["gravity"];
	from_var(p->layer_masks, d["layer_masks"]);

	from_var(&p->chunks, d["chunks"]);
	from_var(&p->nonstatic_bodies, d["nonstatic_bodies"]);
	from_var(&p->static_bodies, d["static_bodies"]);
	from_var(&p->tile_bodies, d["tile_bodies"]);
	from_var(&p->tile_body_registry, d["tile_body_registry"]);

	from_var(&p->curr_pairs, d["curr_pairs"]);
	from_var(&p->curr_events, d["curr_events"]);
}

} // namespace sbx