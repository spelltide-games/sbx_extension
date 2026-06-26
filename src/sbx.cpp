#include "sbx.hpp"
#include "Space.hpp"
#include "LevelDB.hpp"
#include "LockstepGoNetwork.hpp"
#include "MessagePack.hpp"
#include "godot_cpp/core/class_db.hpp"
#include <godot_cpp/core/defs.hpp>

namespace sbx {

using namespace godot;

void setup_sbx_godot_classes() {
	ClassDB::register_class<MessagePack>();
}

void setup_sbx_python_modules() {
	setup_leveldb_module();
	setup_lockstepgo_module();
}

} // namespace sbx