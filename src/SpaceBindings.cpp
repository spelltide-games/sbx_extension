#include "Space.hpp"
#include "pocketpy.h"

namespace sbx {

void setup_space_module(const char *name) {
	py_GlobalRef mod = py_newmodule(name);
	py_Type t = py_newtype("Space", tp_object, mod, [](void *ud) {
		Space *self = (Space *)ud;
		self->~Space();
	});
}

} //namespace sbx