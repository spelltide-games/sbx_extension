#pragma once

#include "godot_cpp/variant/vector2i.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include <cassert>

namespace sbx {

using namespace godot;

const float FLOAT_MAX = 1e9f;
const float FLOAT_EPS = 0.001953125f; // 1/512
const float LINE_IS_POINT_THRESHOLD = 0.015625f; // 1/64
const float PENETRATION_CORRECTION_PERCENTAGE = 0.2f;
const float PENETRATION_SLOP = FLOAT_EPS;
const Vector2i INVALID_CHUNK_POS = Vector2i(INT_MAX, INT_MAX);

using Line = Vector3[2];

struct UnitVector3 {
	int axis;
	int sign;

	UnitVector3 flip() const {
		return UnitVector3{ axis, -sign };
	}

	Vector3 to_vec3() const {
		Vector3 v(0, 0, 0);
		v[axis] = (float)sign;
		return v;
	}
};

static inline Vector3 project_point_on_line(Vector3 point, Line line) {
	Vector3 d = line[1] - line[0];
	if (fabs(d.x) < LINE_IS_POINT_THRESHOLD && fabs(d.y) < LINE_IS_POINT_THRESHOLD && fabs(d.z) < LINE_IS_POINT_THRESHOLD) {
		return line[0];
	}
	Vector3 v = point - line[0];
	float t = v.dot(d) / d.dot(d);
	t = CLAMP(t, 0.0f, 1.0f);
	return line[0] + t * d;
}

static inline bool aabb_intersects(Vector3 vmin, Vector3 vmax, Vector3 other_vmin, Vector3 other_vmax) {
	return (other_vmin.x < vmax.x) && (vmin.x < other_vmax.x) &&
			(other_vmin.y < vmax.y) && (vmin.y < other_vmax.y) &&
			(other_vmin.z < vmax.z) && (vmin.z < other_vmax.z);
}

struct AAFace {
	UnitVector3 normal;
	Vector3 vmin;
	Vector3 vmax;

	void get_ccw_points(Vector3 p_points[4]) const;
	void get_parallel_edges(int axis, Line p_edges[2]) const;
	Vector3 find_closest_distance(const AAFace &other) const;
};

struct AABB {
	Vector3 vmin;
	Vector3 vmax;

	AABB() :
			vmin(Vector3(0, 0, 0)), vmax(Vector3(0, 0, 0)) {}
	AABB(Vector3 vmin, Vector3 vmax) :
			vmin(vmin), vmax(vmax) {}

	Vector3 position() const { return (vmin + vmax) * 0.5f; }
	Vector3 size() const { return vmax - vmin; }

	bool intersects(const AABB &other) const {
		return aabb_intersects(vmin, vmax, other.vmin, other.vmax);
	}

	AAFace get_face(UnitVector3 normal) const;
	void move_both_to_origin(AABB *p_other);
	float find_max_separation(const AABB &other, UnitVector3 *p_reference_normal) const;
};

struct Cube {
	AABB core;
	float radius;

	AABB aabb() const {
		if (radius == 0)
			return core;
		return AABB(core.vmin - Vector3(radius, radius, radius), core.vmax + Vector3(radius, radius, radius));
	}

	void move(Vector3 delta) {
		core.vmin += delta;
		core.vmax += delta;
	}

	void apply(Vector3 position, Vector3 extent, float radius01) {
		this->radius = radius01 * extent[extent.min_axis_index()];
		Vector3 offset = (extent - Vector3(1, 1, 1) * this->radius);
		this->core.vmin = position - offset;
		this->core.vmax = position + offset;
	}
};

int torus_iter_chunks_1d(int size, int chunk_size, double dmin, double dmax, int *out, int out_size);

} // namespace sbx
