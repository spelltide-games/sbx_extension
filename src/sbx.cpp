#include "sbx.hpp"
#include "Space.hpp"
#include "SpaceDebugDraw.hpp"
#include "LevelDB.hpp"
#include "LockstepGoNetwork.hpp"
#include "MessagePack.hpp"
#include "godot_cpp/core/class_db.hpp"
#include <godot_cpp/core/defs.hpp>

#include "pocketpy.h"

namespace sbx {

using namespace godot;

void setup_sbx_godot_classes() {
	ClassDB::register_class<MessagePack>();
	ClassDB::register_class<SpaceDebugDraw>();
}

void setup_sbx_python_modules() {
	py_newmodule("sbxcpp");
	setup_leveldb_module("sbxcpp.leveldb");
	setup_lockstep_module("sbxcpp.lockstep");
	setup_space_module("sbxcpp.space");
}

} // namespace sbx