#include "CubePhysics.hpp"
#include "LevelDB.hpp"
#include "LockstepGo.hpp"
#include "LockstepGoNetwork.hpp"
#include "MessagePack.hpp"
#include <godot_cpp/core/defs.hpp>

namespace pkpy {

using namespace godot;

void setup_sbx_godot_classes() {
	ClassDB::register_class<cube_physics::CubePhysicsSpace>();
	ClassDB::register_class<cube_physics::CubePhysicsBody>();
	ClassDB::register_class<MessagePack>();
}

void setup_sbx_python_modules() {
	setup_leveldb_module();
	setup_lockstepgo_module();
}

} // namespace pkpy