#include "Space.hpp"
#include "pocketpy.h"

namespace sbx {

static py_Type _tp_BodyID;
py_Type get_BodyID_type() {
	if (_tp_BodyID == 0) {
		_tp_BodyID = py_gettype("sbxcpp", py_name("BodyID"));
	}
	return _tp_BodyID;
}

static void gd_newvec3(py_Ref r, Vector3 v) {
	py_newvec3(r, c11_vec3{ { v.x, v.y, v.z } });
}

static Vector3 gd_tovec3(py_Ref r) {
	c11_vec3 v = py_tovec3(r);
	return Vector3(v.x, v.y, v.z);
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

#define BIND_INT_PROPERTY(__name, __getter)                 \
	py_bindmethod(t, #__name, [](int argc, py_Ref argv) {   \
		PY_CHECK_ARGC(1);                                   \
		PY_CHECK_ARG_TYPE(0, tp_object);                    \
		Tilemap *self = (Tilemap *)py_touserdata(&argv[0]); \
		py_newint(py_retval(), self->__getter);             \
		return true;                                        \
	});

	BIND_INT_PROPERTY(width, width())
	BIND_INT_PROPERTY(height, height())
	BIND_INT_PROPERTY(n_layers, n_layers())

	BIND_INT_PROPERTY(slice_x, slice_x)
	BIND_INT_PROPERTY(slice_y, slice_y)
	BIND_INT_PROPERTY(slice_w, slice_w)
	BIND_INT_PROPERTY(slice_h, slice_h)

#undef BIND_INT_PROPERTY

	py_bindmethod(t, "set", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(5);
		PY_CHECK_ARG_TYPE(0, tp_object);
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
		PY_CHECK_ARG_TYPE(0, tp_object);
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

	py_bindmethod(t, "setv", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(4);
		PY_CHECK_ARG_TYPE(0, tp_object);
		PY_CHECK_ARG_TYPE(1, tp_int);
		PY_CHECK_ARG_TYPE(2, tp_int);
		PY_CHECK_ARG_TYPE(3, tp_vec4i);
		Tilemap *self = (Tilemap *)py_touserdata(&argv[0]);
		py_i64 x = cpy312__int_mod(py_toint(&argv[1]), self->width());
		py_i64 y = cpy312__int_mod(py_toint(&argv[2]), self->height());
		c11_vec4i v = py_tovec4i(&argv[3]);
		Tile *tile = self->get(x, y);
		tile->data[0] = v.data[0];
		tile->data[1] = v.data[1];
		tile->data[2] = v.data[2];
		tile->data[3] = v.data[3];
		py_newnone(py_retval());
		return true;
	});

	py_bindmethod(t, "getv", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(3);
		PY_CHECK_ARG_TYPE(0, tp_object);
		PY_CHECK_ARG_TYPE(1, tp_int);
		PY_CHECK_ARG_TYPE(2, tp_int);
		Tilemap *self = (Tilemap *)py_touserdata(&argv[0]);
		py_i64 x = cpy312__int_mod(py_toint(&argv[1]), self->width());
		py_i64 y = cpy312__int_mod(py_toint(&argv[2]), self->height());
		Tile *tile = self->get(x, y);
		c11_vec4i v;
		v.data[0] = tile->data[0];
		v.data[1] = tile->data[1];
		v.data[2] = tile->data[2];
		v.data[3] = tile->data[3];
		py_newvec4i(py_retval(), v);
		return true;
	});

	py_bindmethod(t, "sliced", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(5);
		PY_CHECK_ARG_TYPE(0, tp_object);
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
		py_Type tilemap_t = py_gettype("sbxcpp", py_name("Tilemap"));
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
}

static void setup_Space(py_GlobalRef mod) {
	py_Type t = py_newtype("Space", tp_object, mod, [](void *ud) {
		Space *self = (Space *)ud;
		self->~Space();
	});

	py_bindmethod(t, "__new__", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(3);
		py_Type space_t = py_totype(argv);
		py_Type tilemap_t = py_gettype("sbxcpp", py_name("Tilemap"));
		if (!py_checkinstance(&argv[1], tilemap_t)) {
			return false;
		}
		PY_CHECK_ARG_TYPE(2, tp_int);
		Tilemap *tilemap = (Tilemap *)py_touserdata(&argv[1]);
		int chunk_size = py_toint(&argv[2]);
		void *ud = py_newobject(py_retval(), space_t, 0, sizeof(Space));
		new (ud) Space(*tilemap, chunk_size);
		return true;
	});

	// tilemap
	py_bindproperty(t, "tilemap", [](int argc, py_Ref argv) {
		Space *self = (Space *)py_touserdata(&argv[0]);
		py_Type tilemap_t = py_gettype("sbxcpp", py_name("Tilemap"));
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
		PY_CHECK_ARG_TYPE(3, tp_float);
		int type_int = py_toint(&argv[1]);
		Vector3 extent = gd_tovec3(&argv[2]);
		float radius = py_tofloat(&argv[3]);
		if (type_int < 0 || type_int > 3) {
			return ValueError("invalid body type");
		}
		BodyID bid = self->create_body((BodyType)type_int, extent, radius);
		py_newtrivial(py_retval(), get_BodyID_type(), &bid, sizeof(BodyID));
		return true;
	});

	py_bindmethod(t, "destroy_body", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(2);
		Space *self = (Space *)py_touserdata(&argv[0]);
		PY_CHECK_ARG_TYPE(1, get_BodyID_type());
		BodyID *bid = (BodyID *)py_totrivial(&argv[1]);
		self->destroy_body(*bid);
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
		PY_CHECK_ARGC(3);
		Space *self = (Space *)py_touserdata(&argv[0]);
		PY_CHECK_ARG_TYPE(1, tp_vec3i);
		c11_vec3i c11_xzl = py_tovec3i(&argv[1]);
		py_Ref handler = &argv[2];
		Vector3i xzl(c11_xzl.x, c11_xzl.y, c11_xzl.z);
		auto it = self->curr_pairs_by_xzl.find(xzl);
		py_Type bid_t = get_BodyID_type();
		if (it) {
			CollisionPair pair = it->value;
			py_push(handler);
			py_newint(py_pushtmp(), (int)CollisionEvent::Type::REMOVED);
			py_newtrivial(py_pushtmp(), bid_t, &pair.a, sizeof(BodyID));
			py_newtrivial(py_pushtmp(), bid_t, &pair.b, sizeof(BodyID));
			py_newvec3i(py_pushtmp(), c11_xzl);
			py_newnone(py_pushtmp());
			if (!py_vectorcall(5, 0)) {
				return false;
			}
			self->curr_pairs.erase(pair);
			self->curr_pairs_by_xzl.remove(it);
			py_newbool(py_retval(), true);
		} else {
			py_newbool(py_retval(), false);
		}
		return true;
	});

	py_bindmethod(t, "step", [](int argc, py_Ref argv) {
		PY_CHECK_ARGC(3);
		Space *self = (Space *)py_touserdata(&argv[0]);
		PY_CHECK_ARG_TYPE(1, tp_float);
		py_Ref handler = &argv[2];
		float delta = py_tofloat(&argv[1]);
		self->step(delta);
		py_Type bid_t = get_BodyID_type();
		for (int i = 0; i < self->curr_events.size(); ++i) {
			CollisionEvent ev = self->curr_events[i];
			py_push(handler);
			py_newint(py_pushtmp(), (int)ev.type);
			py_newtrivial(py_pushtmp(), bid_t, &ev.a, sizeof(BodyID));
			py_newtrivial(py_pushtmp(), bid_t, &ev.b, sizeof(BodyID));
			py_newvec3i(py_pushtmp(), c11_vec3i{ { ev.xzl.x, ev.xzl.y, ev.xzl.z } });
			if (ev.type == CollisionEvent::Type::ADDED) {
				py_newvec3(py_pushtmp(), c11_vec3{ { ev.normal.x, ev.normal.y, ev.normal.z } });
			} else {
				py_newnone(py_pushtmp());
			}
			if (!py_vectorcall(5, 0)) {
				return false;
			}
		}
		py_newnone(py_retval());
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

	BIND_BODY_GETTER(position, position(), gd_newvec3)
	BIND_BODY_GETTER(extent, cube.core.extent(), gd_newvec3)
	BIND_BODY_GETTER(size, cube.core.size(), gd_newvec3)
	BIND_BODY_GETTER(radius, cube.radius, py_newfloat)
	BIND_BODY_GETTER(chunk_index, chunk_index, py_newint)
}

void setup_space_module(const char *name) {
	py_GlobalRef mod = py_newmodule(name);
	setup_Tilemap(mod);
	setup_BodyID(mod);
	setup_Space(mod);
}

} //namespace sbx