#pragma once

#include "Space.hpp"
#include "godot_cpp/classes/array_mesh.hpp"
#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/classes/shader_material.hpp"

using namespace godot;

namespace sbx {

class SpaceDebugDraw : public MeshInstance3D {
	GDCLASS(SpaceDebugDraw, MeshInstance3D)

	Ref<ArrayMesh> mesh;
	Color tile_color = Color(0.0, 1, 0.0, 0.5);
	Color body_color = Color(1, 1, 1, 0.8);

	Ref<ShaderMaterial> create_with_color(Color color) {
		Ref<ShaderMaterial> mat;
		mat.instantiate();
		String shader_code = R"(
shader_type spatial;
render_mode unshaded;

void fragment() {
	ALBEDO = vec3({0}, {1}, {2});
}
)";
		Array args;
		args.append(color.r * color.a);
		args.append(color.g * color.a);
		args.append(color.b * color.a);
		shader_code = shader_code.format(args);
		Ref<Shader> shader;
		shader.instantiate();
		shader->set_code(shader_code);
		mat->set_shader(shader);
		return mat;
	}

	Color get_tile_color() const {
		return tile_color;
	}

	void set_tile_color(Color color) {
		if (mesh.is_valid() && mesh->get_surface_count() == 2) {
			set_surface_override_material(0, create_with_color(color));
		}
		tile_color = color;
	}

	Color get_body_color() const {
		return body_color;
	}

	void set_body_color(Color color) {
		if (mesh.is_valid() && mesh->get_surface_count() == 2) {
			set_surface_override_material(1, create_with_color(color));
		}
		body_color = color;
	}

	void rebuild(int64_t p_space, int x, int y, int w, int h) {
		if (!mesh.is_valid()) {
			mesh.instantiate();
		}
		mesh->clear_surfaces();
		Space *space = reinterpret_cast<Space *>(p_space);
		space->draw_chunk_bodies(mesh, x, y, w, h);
		set_mesh(mesh);
		set_surface_override_material(0, create_with_color(tile_color));
		set_surface_override_material(1, create_with_color(body_color));
	}

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("rebuild", "p_space", "x", "y", "w", "h"), &SpaceDebugDraw::rebuild);
		ClassDB::bind_method(D_METHOD("set_tile_color", "color"), &SpaceDebugDraw::set_tile_color);
		ClassDB::bind_method(D_METHOD("get_tile_color"), &SpaceDebugDraw::get_tile_color);
		ClassDB::bind_method(D_METHOD("set_body_color", "color"), &SpaceDebugDraw::set_body_color);
		ClassDB::bind_method(D_METHOD("get_body_color"), &SpaceDebugDraw::get_body_color);
		ADD_PROPERTY(PropertyInfo(Variant::COLOR, "tile_color"), "set_tile_color", "get_tile_color");
		ADD_PROPERTY(PropertyInfo(Variant::COLOR, "body_color"), "set_body_color", "get_body_color");
	}
};

} // namespace sbx