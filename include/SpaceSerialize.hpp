#pragma once

#include "Space.hpp"
#include <godot_cpp/variant/packed_byte_array.hpp>

namespace sbx {

PackedByteArray serialize_space(Space &space);

bool deserialize_space(Space &space, const PackedByteArray &bytes);

} // namespace sbx
