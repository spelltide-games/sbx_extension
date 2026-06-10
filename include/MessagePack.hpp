#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace pkpy {

using namespace godot;

class MessagePack : public Object {
	GDCLASS(MessagePack, Object)

public:
	static Variant loads(const PackedByteArray &data);
	static PackedByteArray dumps(Variant value);

protected:
	static void _bind_methods();
};

} // namespace godot