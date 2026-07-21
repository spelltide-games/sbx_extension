#pragma once

#include "Space.hpp"
#include "godot_cpp/variant/variant.hpp"

using namespace godot;

namespace sbx {

Variant space_to_var(const Space *p);
void space_from_var(Space *p, Variant v);

Variant bodyid_to_var(const BodyID *p);
void bodyid_from_var(BodyID *p, Variant v);

} // namespace sbx
