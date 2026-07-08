#pragma once

#include "godot_cpp/variant/variant.hpp"
#include "pocketpy.h"

namespace pkpy {

void py_newvariant(py_OutRef out, const godot::Variant *val);
godot::Variant py_tovariant(py_Ref val);
void log_python_error_and_clearexc(py_StackRef p0);

} // namespace pkpy