#include "Space.hpp"
#include "ToVariant.hpp"
#include "pkpy.hpp"


namespace sbx {

static py_Type _tp_BodyID;
static py_Type _tp_Tile;

py_Type get_BodyID_type() {
	if (_tp_BodyID == 0) {
		_tp_BodyID = py_gettype("sbxcpp.space", py_name("BodyID"));
	}
	return _tp_BodyID;
}

py_Type get_Tile_type() {
	if (_tp_Tile == 0) {
		_tp_Tile = py_gettype("sbxcpp.space", py_name("Tile"));
	}
	return _tp_Tile;
}

static void gd_newvec3(py_Ref r, Vector3 v) {
	py_newvec3(r, c11_vec3{ { v.x, v.y, v.z } });
}

static Vector3 gd_tovec3(py_Ref r) {
	c11_vec3 v = py_tovec3(r);
	return Vector3(v.x, v.y, v.z);
}

static void gd_newtile(py_Ref r, Tile t) {
	py_newtrivial(r, get_Tile_type(), &t, sizeof(Tile));
}

static Tile gd_totile(py_Ref r) {
	Tile *t = (Tile *)py_totrivial(r);
	return *t;
}

static void setup_Tile(py_GlobalRef mod) {
	py_Type t = py_newtype("Tile", tp_object, mod, NULL);
	py_tpsetfinal(t);
	py_bindmethod(t, "__new__", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(1 + 4);
		PY_CHECK_ARG_TYPE(1, tp_int);
		PY_CHECK_ARG_TYPE(2, tp_int);
		PY_CHECK_ARG_TYPE(3, tp_int);
		PY_CHECK_ARG_TYPE(4, tp_int);
		py_Type tile_t = py_totype(argv);
		Tile tile;
		tile.data[0] = (TileID)py_toint(&argv[1]);
		tile.data[1] = (TileID)py_toint(&argv[2]);
		tile.data[2] = (TileID)py_toint(&argv[3]);
		tile.data[3] = (TileID)py_toint(&argv[4]);
		gd_newtile(py_retval(), tile);
		return true;
	});

	py_bindmethod(t, "__repr__", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(1);
		Tile *self = (Tile *)py_totrivial(argv);
		String repr = String("Tile({0}, {1}, {2}, {3})").format(Array::make((int)self->data[0], (int)self->data[1], (int)self->data[2], (int)self->data[3]));
		pkpy::py_newstring(py_retval(), repr);
		return true;
	});

	py_bindmethod(t, "__len__", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(1);
		py_newint(py_retval(), Tilemap::n_layers());
		return true;
	});

	py_bindmethod(t, "with_l", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(3);
		PY_CHECK_ARG_TYPE(1, tp_int);
		PY_CHECK_ARG_TYPE(2, tp_int);
		Tile tile = gd_totile(&argv[0]);
		int layer = py_toint(&argv[1]);
		TileID tile_id = (TileID)py_toint(&argv[2]);
		if (layer < 0 || layer >= Tilemap::n_layers()) {
			return IndexError("layer index out of range");
		}
		tile.data[layer] = tile_id;
		gd_newtile(py_retval(), tile);
		return true;
	});

	py_bindmethod(t, "__getitem__", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(2);
		PY_CHECK_ARG_TYPE(1, tp_int);
		Tile tile = gd_totile(&argv[0]);
		int layer = py_toint(&argv[1]);
		if (layer < 0 || layer >= Tilemap::n_layers()) {
			return IndexError("layer index out of range");
		}
		py_newint(py_retval(), tile.data[layer]);
		return true;
	});
}

static void setup_Tilemap(py_GlobalRef mod) {
	py_Type t = py_newtype("Tilemap", tp_object, mod, [](void *ud) {
		Tilemap *self = (Tilemap *)ud;
		self->~Tilemap();
	});

	py_bindmethod(t, "__new__", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(3);
		PY_CHECK_ARG_TYPE(1, tp_int);
		PY_CHECK_ARG_TYPE(2, tp_int);
		int width = py_toint(&argv[1]);
		int height = py_toint(&argv[2]);
		py_Type tilemap_t = py_totype(argv);
		void *ud = py_newobject(py_retval(), tilemap_t, 0, sizeof(Tilemap));
		new (ud) Tilemap(width, height);
		return true;
	});

#define BIND_INT_PROPERTY(__name, __getter) \
	py_bindproperty(t, #__name, [](int argc, py_Ref argv) {   \
		PY_CHECK_ARGC(1);                                   \
		Tilemap *self = (Tilemap *)py_touserdata(&argv[0]); \
		py_newint(py_retval(), self->__getter);             \
		return true; }, NULL);

	BIND_INT_PROPERTY(width, width())
	BIND_INT_PROPERTY(height, height())
	BIND_INT_PROPERTY(n_layers, Tilemap::n_layers())

	BIND_INT_PROPERTY(slice_x, slice_x)
	BIND_INT_PROPERTY(slice_y, slice_y)
	BIND_INT_PROPERTY(slice_w, slice_w)
	BIND_INT_PROPERTY(slice_h, slice_h)

#undef BIND_INT_PROPERTY

	py_bindmethod(t, "set", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(5);
		PY_CHECK_ARG_TYPE(1, tp_int);
		PY_CHECK_ARG_TYPE(2, tp_int);
		PY_CHECK_ARG_TYPE(3, tp_int);
		PY_CHECK_ARG_TYPE(4, tp_int);
		Tilemap *self = (Tilemap *)py_touserdata(&argv[0]);
		py_i64 x = cpy312__int_mod(py_toint(&argv[1]), self->width());
		py_i64 y = cpy312__int_mod(py_toint(&argv[2]), self->height());
		int layer = py_toint(&argv[3]);
		if (layer < 0 || layer >= self->n_layers()) {
			return IndexError("layer index out of range");
		}
		TileID value = (TileID)py_toint(&argv[4]);
		self->get(x, y)->data[layer] = value;
		py_newnone(py_retval());
		return true;
	});

	py_bindmethod(t, "get", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(4);
		PY_CHECK_ARG_TYPE(1, tp_int);
		PY_CHECK_ARG_TYPE(2, tp_int);
		PY_CHECK_ARG_TYPE(3, tp_int);
		Tilemap *self = (Tilemap *)py_touserdata(&argv[0]);
		py_i64 x = cpy312__int_mod(py_toint(&argv[1]), self->width());
		py_i64 y = cpy312__int_mod(py_toint(&argv[2]), self->height());
		int layer = py_toint(&argv[3]);
		if (layer < 0 || layer >= self->n_layers()) {
			return IndexError("layer index out of range");
		}
		py_newint(py_retval(), self->get(x, y)->data[layer]);
		return true;
	});

	py_bindmethod(t, "set_tile", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(4);
		PY_CHECK_ARG_TYPE(1, tp_int);
		PY_CHECK_ARG_TYPE(2, tp_int);
		PY_CHECK_ARG_TYPE(3, get_Tile_type());
		Tilemap *self = (Tilemap *)py_touserdata(&argv[0]);
		py_i64 x = cpy312__int_mod(py_toint(&argv[1]), self->width());
		py_i64 y = cpy312__int_mod(py_toint(&argv[2]), self->height());
		*self->get(x, y) = gd_totile(&argv[3]);
		py_newnone(py_retval());
		return true;
	});

	py_bindmethod(t, "get_tile", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(3);
		PY_CHECK_ARG_TYPE(1, tp_int);
		PY_CHECK_ARG_TYPE(2, tp_int);
		Tilemap *self = (Tilemap *)py_touserdata(&argv[0]);
		py_i64 x = cpy312__int_mod(py_toint(&argv[1]), self->width());
		py_i64 y = cpy312__int_mod(py_toint(&argv[2]), self->height());
		gd_newtile(py_retval(), *self->get(x, y));
		return true;
	});

	py_bindmethod(t, "sliced", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(5);
		PY_CHECK_ARG_TYPE(1, tp_int);
		PY_CHECK_ARG_TYPE(2, tp_int);
		PY_CHECK_ARG_TYPE(3, tp_int);
		PY_CHECK_ARG_TYPE(4, tp_int);
		Tilemap *self = (Tilemap *)py_touserdata(&argv[0]);
		int x = py_toint(&argv[1]);
		int y = py_toint(&argv[2]);
		int w = py_toint(&argv[3]);
		int h = py_toint(&argv[4]);
		if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > self->width() || y + h > self->height()) {
			return ValueError("invalid slice parameters");
		}
		py_Type tilemap_t = py_gettype("sbxcpp.space", py_name("Tilemap"));
		void *ud = py_newobject(py_retval(), tilemap_t, 0, sizeof(Tilemap));
		new (ud) Tilemap(self->sliced(x, y, w, h));
		return true;
	});
}

static void setup_BodyID(py_GlobalRef mod) {
	py_Type t = py_newtype("BodyID", tp_object, mod, NULL);
	py_tpsetfinal(t);
	py_bindproperty(t, "type", [](int argc, py_Ref argv) {
		BodyID *self = (BodyID *)py_totrivial(argv);
		py_newint(py_retval(), (int)self->type);
		return true; }, NULL);
	py_bindproperty(t, "is_valid", [](int argc, py_Ref argv) {
		BodyID *self = (BodyID *)py_totrivial(argv);
		py_newbool(py_retval(), self->is_valid);
		return true; }, NULL);
	py_bindproperty(t, "id", [](int argc, py_Ref argv) {
		BodyID *self = (BodyID *)py_totrivial(argv);
		py_newint(py_retval(), self->id);
		return true; }, NULL);

	py_bindmethod(t, "__hash__", [](int argc, py_Ref argv) {
		BodyID *self = (BodyID *)py_totrivial(argv);
		py_newint(py_retval(), (py_i64)self->hash());
		return true;
	});

	py_bindmethod(t, "__eq__", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(2);
		BodyID *self = (BodyID *)py_totrivial(&argv[0]);
		PY_CHECK_ARG_TYPE(1, get_BodyID_type());
		BodyID *other = (BodyID *)py_totrivial(&argv[1]);
		py_newbool(py_retval(), *self == *other);
		return true;
	});

	py_bindmethod(t, "__ne__", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(2);
		BodyID *self = (BodyID *)py_totrivial(&argv[0]);
		PY_CHECK_ARG_TYPE(1, get_BodyID_type());
		BodyID *other = (BodyID *)py_totrivial(&argv[1]);
		py_newbool(py_retval(), *self != *other);
		return true;
	});

	py_bindmethod(t, "__repr__", [](int argc, py_Ref argv) {
		BodyID *self = (BodyID *)py_totrivial(argv);
		String repr;
		if (self->is_valid) {
			repr = String("BodyID(type={0}, id={1})").format(Array::make((int)self->type, self->id));
		} else {
			repr = String("BodyID(INVALID)");
		}
		pkpy::py_newstring(py_retval(), repr);
		return true;
	});

	py_bindmethod(t, "to_var", [](int argc, py_Ref argv) {
		BodyID *self = (BodyID *)py_totrivial(argv);
		Variant v = bodyid_to_var(self);
		pkpy::py_newvariant(py_retval(), &v);
		return true;
	});

	py_bindstaticmethod(t, "from_var", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(1);
		Variant v = pkpy::py_tovariant(&argv[0]);
		BodyID bid;
		bodyid_from_var(&bid, v);
		py_newtrivial(py_retval(), get_BodyID_type(), &bid, sizeof(BodyID));
		return true;
	});
}

static void collision_event_handler(const CollisionEvent &ev, void *space) {
	py_Ref callbacks = py_getslot((py_Ref)space, 0);
	py_StackRef p0 = py_peek(0);
	py_push(callbacks);
	static py_Name on_pair_add = py_name("on_pair_add");
	static py_Name on_pair_remove = py_name("on_pair_remove");
	py_Name name = (ev.type == CollisionEvent::Type::ADDED) ? on_pair_add : on_pair_remove;
	if (!py_pushmethod(name)) {
		AttributeError(callbacks, name);
		pkpy::log_python_error_and_clearexc(p0);
		return;
	}
	py_newtrivial(py_pushtmp(), get_BodyID_type(), (void *)&ev.a, sizeof(BodyID));
	py_newtrivial(py_pushtmp(), get_BodyID_type(), (void *)&ev.b, sizeof(BodyID));
	py_newvec3i(py_pushtmp(), c11_vec3i{ { ev.xzl.x, ev.xzl.y, ev.xzl.z } });
	if (ev.type == CollisionEvent::Type::ADDED) {
		py_newvec3(py_pushtmp(), c11_vec3{ { ev.normal.x, ev.normal.y, ev.normal.z } });
		py_newfloat(py_pushtmp(), ev.max_sep);
		if (py_vectorcall(5, 0))
			return;
	} else {
		if (py_vectorcall(3, 0))
			return;
	}
	print_error("collision_event_handler: exception occurred in callback");
	pkpy::log_python_error_and_clearexc(p0);
}

static void setup_Space(py_GlobalRef mod) {
	py_Type t = py_newtype("Space", tp_object, mod, [](void *ud) {
		Space *self = (Space *)ud;
		self->~Space();
	});

	py_bindmethod(t, "__new__", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(4);
		py_Type space_t = py_totype(argv);
		py_Type tilemap_t = py_gettype("sbxcpp.space", py_name("Tilemap"));
		if (!py_checkinstance(&argv[1], tilemap_t)) {
			return false;
		}
		PY_CHECK_ARG_TYPE(2, tp_int);
		py_Ref callbacks = &argv[3];
		Tilemap *tilemap = (Tilemap *)py_touserdata(&argv[1]);
		int chunk_size = py_toint(&argv[2]);
		void *ud = py_newobject(py_retval(), space_t, 1, sizeof(Space));
		py_setslot(py_retval(), 0, callbacks);
		new (ud) Space(*tilemap, chunk_size);
		return true;
	});

	py_bindproperty(t, "gravity", [](int argc, py_Ref argv) {
		Space *self = (Space *)py_touserdata(&argv[0]);
		gd_newvec3(py_retval(), self->gravity);
		return true; }, [](int argc, py_Ref argv) {
			Space *self = (Space *)py_touserdata(&argv[0]);
			PY_CHECK_ARGC(2);
			PY_CHECK_ARG_TYPE(1, tp_vec3);
			self->gravity = gd_tovec3(&argv[1]);
			py_newnone(py_retval());
			return true; });

	// tilemap
	py_bindproperty(t, "tilemap", [](int argc, py_Ref argv) {
		Space *self = (Space *)py_touserdata(&argv[0]);
		py_Type tilemap_t = py_gettype("sbxcpp.space", py_name("Tilemap"));
		void *ud = py_newobject(py_retval(), tilemap_t, 0, sizeof(Tilemap));
		new (ud) Tilemap(self->tilemap);
		return true; }, NULL);

	// chunk_size
	py_bindproperty(t, "chunk_size", [](int argc, py_Ref argv) {
		Space *self = (Space *)py_touserdata(&argv[0]);
		py_newint(py_retval(), self->chunker.chunk_size);
		return true; }, NULL);

	py_bindmethod(t, "create_body", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(4);
		Space *self = (Space *)py_touserdata(&argv[0]);
		PY_CHECK_ARG_TYPE(1, tp_int);
		PY_CHECK_ARG_TYPE(2, tp_vec3);
		float radius01;
		if (!py_castfloat32(&argv[3], &radius01)) {
			return false;
		}
		int type_int = py_toint(&argv[1]);
		Vector3 aabb_extent = gd_tovec3(&argv[2]);
		if (type_int < 0 || type_int > 3) {
			return ValueError("invalid body type");
		}
		BodyID bid = self->create_body((BodyType)type_int, aabb_extent, radius01);
		py_newtrivial(py_retval(), get_BodyID_type(), &bid, sizeof(BodyID));
		return true;
	});

	py_bindmethod(t, "register_tile_body", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(3);
		Space *self = (Space *)py_touserdata(&argv[0]);
		PY_CHECK_ARG_TYPE(1, tp_int);
		PY_CHECK_ARG_TYPE(2, get_BodyID_type());
		TileID tile_id = (TileID)py_toint(&argv[1]);
		BodyID *bid = (BodyID *)py_totrivial(&argv[2]);
		self->register_tile_body(tile_id, *bid);
		py_newnone(py_retval());
		return true;
	});

	py_bindmethod(t, "destroy_body", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(2);
		Space *self = (Space *)py_touserdata(&argv[0]);
		PY_CHECK_ARG_TYPE(1, get_BodyID_type());
		BodyID *bid = (BodyID *)py_totrivial(&argv[1]);
		self->destroy_body(*bid, collision_event_handler, argv);
		py_newnone(py_retval());
		return true;
	});

	py_bindmethod(t, "teleport_body", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(3);
		Space *self = (Space *)py_touserdata(&argv[0]);
		PY_CHECK_ARG_TYPE(1, get_BodyID_type());
		PY_CHECK_ARG_TYPE(2, tp_vec3);
		BodyID *bid = (BodyID *)py_totrivial(&argv[1]);
		Vector3 position = gd_tovec3(&argv[2]);
		self->teleport_body(*bid, position);
		py_newnone(py_retval());
		return true;
	});

	py_bindmethod(t, "remove_pairs_with_tile", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(2);
		Space *self = (Space *)py_touserdata(&argv[0]);
		PY_CHECK_ARG_TYPE(1, tp_vec3i);
		c11_vec3i c11_xzl = py_tovec3i(&argv[1]);
		Vector3i xzl(c11_xzl.x, c11_xzl.y, c11_xzl.z);
		self->remove_pairs_with_tile(xzl, collision_event_handler, argv);
		py_newnone(py_retval());
		return true;
	});

	py_bindmethod(t, "step", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(2);
		Space *self = (Space *)py_touserdata(&argv[0]);
		PY_CHECK_ARG_TYPE(1, tp_float);
		float delta = py_tofloat(&argv[1]);
		self->step(delta, collision_event_handler, argv);
		py_newnone(py_retval());
		return true;
	});

	py_bindmethod(t, "closest_mirror", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(3);
		Space *self = (Space *)py_touserdata(&argv[0]);
		PY_CHECK_ARG_TYPE(1, tp_vec3);
		PY_CHECK_ARG_TYPE(2, tp_vec3);
		Vector3 pos = gd_tovec3(&argv[1]);
		Vector3 ref_pos = gd_tovec3(&argv[2]);
		torus_closest_mirror(&pos, ref_pos, self->width(), self->height());
		gd_newvec3(py_retval(), pos);
		return true;
	});

	// body properties
#define BIND_BODY_GETTER(__name, __getter, __init)                    \
	py_bindmethod(t, "body_get_" #__name, [](int argc, py_Ref argv) { \
		PY_CHECK_ARGC(2);                                             \
		Space *self = (Space *)py_touserdata(&argv[0]);               \
		PY_CHECK_ARG_TYPE(1, get_BodyID_type());                      \
		BodyID *bid = (BodyID *)py_totrivial(&argv[1]);               \
		Body *body = self->get_body(*bid);                            \
		__init(py_retval(), body->__getter);                          \
		return true;                                                  \
	});

#define BIND_BODY_SETTER(__name, __setter, __type, __cvt)             \
	py_bindmethod(t, "body_set_" #__name, [](int argc, py_Ref argv) { \
		PY_CHECK_ARGC(3);                                             \
		Space *self = (Space *)py_touserdata(&argv[0]);               \
		PY_CHECK_ARG_TYPE(1, get_BodyID_type());                      \
		PY_CHECK_ARG_TYPE(2, __type);                                 \
		BodyID *bid = (BodyID *)py_totrivial(&argv[1]);               \
		Body *body = self->get_body(*bid);                            \
		body->__setter = __cvt(&argv[2]);                             \
		py_newnone(py_retval());                                      \
		return true;                                                  \
	});

	BIND_BODY_GETTER(layer, layer, py_newint)
	BIND_BODY_SETTER(layer, layer, tp_int, py_toint)
	BIND_BODY_GETTER(is_trigger, is_trigger, py_newbool)
	BIND_BODY_SETTER(is_trigger, is_trigger, tp_bool, py_tobool)
	BIND_BODY_GETTER(mass, mass, py_newfloat)
	BIND_BODY_SETTER(mass, mass, tp_float, py_tofloat)
	BIND_BODY_GETTER(velocity, velocity, gd_newvec3)
	BIND_BODY_SETTER(velocity, velocity, tp_vec3, gd_tovec3)
	BIND_BODY_GETTER(instant_velocity, instant_velocity, gd_newvec3)
	BIND_BODY_SETTER(instant_velocity, instant_velocity, tp_vec3, gd_tovec3)
	BIND_BODY_GETTER(friction, friction, py_newfloat)
	BIND_BODY_SETTER(friction, friction, tp_float, py_tofloat)
	BIND_BODY_GETTER(restitution, restitution, py_newfloat)
	BIND_BODY_SETTER(restitution, restitution, tp_float, py_tofloat)

	BIND_BODY_GETTER(position, position(), gd_newvec3)
	BIND_BODY_GETTER(aabb_extent, cube.aabb().extent(), gd_newvec3)
	BIND_BODY_GETTER(core_extent, cube.core.extent(), gd_newvec3)
	BIND_BODY_GETTER(radius, cube.radius, py_newfloat)
	BIND_BODY_GETTER(radius01, cube.radius01(), py_newfloat)
	BIND_BODY_GETTER(chunk_index, chunk_index, py_newint)

#undef BIND_BODY_GETTER
#undef BIND_BODY_SETTER

	py_bindmethod(t, "body_get_chunk_pos", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(2);
		Space *self = (Space *)py_touserdata(&argv[0]);
		PY_CHECK_ARG_TYPE(1, get_BodyID_type());
		BodyID *bid = (BodyID *)py_totrivial(&argv[1]);
		Body *body = self->get_body(*bid);
		int idx = body->chunk_index;
		if (idx < 0) {
			py_newvec2i(py_retval(), c11_vec2i{ { -1, -1 } });
			return true;
		}
		int cx = idx % self->chunker.n_chunks_x;
		int cz = idx / self->chunker.n_chunks_x;
		py_newvec2i(py_retval(), c11_vec2i{ { cx, cz } });
		return true;
	});

	py_bindmethod(t, "get_ptr", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(1);
		Space *self = (Space *)py_touserdata(&argv[0]);
		py_newint(py_retval(), (intptr_t)self);
		return true;
	});

	py_bindmethod(t, "to_var", [](int argc, py_Ref argv) {
		Space *self = (Space *)py_touserdata(&argv[0]);
		Variant v = space_to_var(self);
		pkpy::py_newvariant(py_retval(), &v);
		return true;
	});

	py_bindstaticmethod(t, "from_var", [](int argc, py_Ref argv) {
		py_Type space_t = py_gettype("sbxcpp.space", py_name("Space"));
		PY_CHECK_ARGC(2);
		Variant v = pkpy::py_tovariant(&argv[0]);
		py_Ref callbacks = &argv[1];
		void *ud = py_newobject(py_retval(), space_t, 1, sizeof(Space));
		new (ud) Space();
		space_from_var((Space *)ud, v);
		py_setslot(py_retval(), 0, callbacks);
		return true;
	});
}

void setup_space_module(const char *name) {
	py_GlobalRef mod = py_newmodule(name);
	setup_Tile(mod);
	setup_Tilemap(mod);
	setup_BodyID(mod);
	setup_Space(mod);
}

} //namespace sbx