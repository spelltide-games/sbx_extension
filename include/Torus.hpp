#pragma once

#include "godot_cpp/variant/vector2.hpp"
#include "godot_cpp/variant/vector2i.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include <cassert>

extern "C" {
double dmath_fmod(double, double);
double dmath_floor(double);
}

namespace sbx {

using namespace godot;

const float FLOAT_MAX = 1e9f;
const float FLOAT_EPS = 0.001953125f; // 1/512
const float LINE_IS_POINT_THRESHOLD = 0.015625f; // 1/64
const float PENETRATION_CORRECTION_PERCENTAGE = 0.2f;
const float PENETRATION_SLOP = FLOAT_EPS;

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

	bool operator==(const UnitVector3 &other) const {
		return axis == other.axis && sign == other.sign;
	}

	bool operator!=(const UnitVector3 &other) const {
		return axis != other.axis || sign != other.sign;
	}
};

Vector3 project_point_on_line(Vector3 point, Line line);

static inline int posmod(int x, int m) {
	assert(m > 0);
	int r = x % m;
	return r < 0 ? r + m : r;
}

static inline double posmodf(double x, double m) {
	double r = dmath_fmod(x, m);
	return r >= 0 ? r : r + m;
}

struct AAFace {
	UnitVector3 normal;
	Vector3 vmin;
	Vector3 vmax;

	void get_ccw_points(Vector3 p_points[4]) const;
	void get_parallel_edges(int axis, Line p_edges[2]) const;
	Vector3 find_closest_distance(const AAFace &other) const;
	String repr() const;
};

struct AABB {
	Vector3 vmin;
	Vector3 vmax;

	AABB() :
			vmin(Vector3(0, 0, 0)), vmax(Vector3(0, 0, 0)) {}
	AABB(Vector3 vmin, Vector3 vmax) :
			vmin(vmin), vmax(vmax) {}

	Vector3 position() const { return (vmin + vmax) * 0.5f; }
	Vector2 position_xz() const { return Vector2((vmin.x + vmax.x) * 0.5f, (vmin.z + vmax.z) * 0.5f); }
	Vector3 extent() const { return size() * 0.5f; }
	Vector3 size() const { return vmax - vmin; }
	Vector2 size_xz() const { return Vector2(vmax.x - vmin.x, vmax.z - vmin.z); }

	bool intersects(const AABB &other) const {
		return (other.vmin.x < vmax.x) && (vmin.x < other.vmax.x) &&
				(other.vmin.y < vmax.y) && (vmin.y < other.vmax.y) &&
				(other.vmin.z < vmax.z) && (vmin.z < other.vmax.z);
	}

	void move(Vector3 offset) {
		vmin += offset;
		vmax += offset;
	}

	void set_position(Vector3 position) {
		Vector3 size = this->size();
		vmin = position - size * 0.5f;
		vmax = position + size * 0.5f;
	}

	AAFace get_face(UnitVector3 normal) const;
	float find_max_separation(const AABB &other, UnitVector3 *p_reference_normal) const;
};

struct Cube {
	AABB core;
	float radius;

	Cube() :
			core(), radius(0) {}

	Cube(Vector3 position, Vector3 aabb_extent, float radius01) {
		this->radius = radius01 * aabb_extent[aabb_extent.min_axis_index()];
		Vector3 offset = (aabb_extent - Vector3(1, 1, 1) * this->radius);
		this->core.vmin = position - offset;
		this->core.vmax = position + offset;
	}

	float radius01() const {
		Vector3 aabb_extent = aabb().extent();
		return radius / aabb_extent[aabb_extent.min_axis_index()];
	}

	AABB aabb() const {
		if (radius == 0)
			return core;
		return AABB(core.vmin - Vector3(radius, radius, radius), core.vmax + Vector3(radius, radius, radius));
	}

	void torus_move(Vector3 delta, int width, int height) {
		Vector3 pos = core.position() + delta;
		pos.x = posmodf(pos.x, width);
		pos.z = posmodf(pos.z, height);
		core.set_position(pos);
	}
};

struct Chunker {
	int width;
	int height;
	int chunk_size;
	int n_chunks_x;
	int n_chunks_y;

	Chunker(int width, int height, int chunk_size) :
			width(width), height(height), chunk_size(chunk_size) {
		n_chunks_x = (width + chunk_size - 1) / chunk_size;
		n_chunks_y = (height + chunk_size - 1) / chunk_size;
	}

	void get_slice(Vector2i chunk_pos, int *p_x, int *p_y, int *p_w, int *p_h) const {
		*p_x = chunk_pos.x * chunk_size;
		*p_y = chunk_pos.y * chunk_size;
		*p_w = chunk_size;
		*p_h = chunk_size;
	}

	Vector2i map_2d(float x, float y) const {
		assert(x >= 0 && x < width);
		assert(y >= 0 && y < height);

		int cx = static_cast<int>(x) / chunk_size;
		int cy = static_cast<int>(y) / chunk_size;
		return Vector2i(cx, cy);
	}

	int torus_2d_to_1d(Vector2i pos) const {
		int x = posmod(pos.x, n_chunks_x);
		int y = posmod(pos.y, n_chunks_y);
		return y * n_chunks_x + x;
	}

	int map_1d(float x, float y) const {
		Vector2i pos = map_2d(x, y);
		return pos.y * n_chunks_x + pos.x;
	}

	Vector2i map_2d(Vector2 pos) const {
		return map_2d(pos.x, pos.y);
	}

	int map_1d(Vector2 pos) const {
		return map_1d(pos.x, pos.y);
	}
};

int torus_iter_chunks_1d(int size, int chunk_size, double dmin, double dmax, int *out, int out_size);
void torus_normalize_two_aabb(int width, int height, AABB *p_aabb_a, AABB *p_aabb_b);

} // namespace sbx
