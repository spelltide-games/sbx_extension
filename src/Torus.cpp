#include "Torus.hpp"
#include <cassert>

extern "C" {
double dmath_floor(double);
}

namespace sbx {

int torus_iter_chunks_1d(int size, int chunk_size, float dmin, float dmax, int *out, int out_size) {
	int n = size / chunk_size;
	assert(size % chunk_size == 0);

	float d0 = posmodf(dmin, size);
	float d1 = posmodf(dmax, size);

	int c0 = (int)dmath_floor(d0 / chunk_size);
	int c1 = (int)dmath_floor(d1 / chunk_size);

	int k = 0;

	if (dmax - dmin >= size) {
		// full dimension
		for (int i = 0; i < n; ++i) {
			out[k++] = i;
			if (k >= out_size)
				return k;
		}
	} else if (c0 < c1) {
		for (int i = c0; i <= c1; ++i) {
			out[k++] = i;
			if (k >= out_size)
				return k;
		}
	} else {
		// wrap-around
		for (int i = c0; i < n; ++i) {
			out[k++] = i;
			if (k >= out_size)
				return k;
		}
		for (int i = 0; i <= c1; ++i) {
			out[k++] = i;
			if (k >= out_size)
				return k;
		}
	}
	return k;
}

void torus_normalize_two_aabb(int width, int height, AABB *p_aabb_a, AABB *p_aabb_b) {
	const float w = (float)width;
	const float h = (float)height;

	Vector3 rel = p_aabb_b->position() - p_aabb_a->position();
	rel.x = posmodf(rel.x + 0.5 * w, w) - 0.5 * w;
	rel.z = posmodf(rel.z + 0.5 * h, h) - 0.5 * h;

	p_aabb_a->set_position(Vector3(0, 0, 0));
	p_aabb_b->set_position(rel);
}

bool torus_aabb_intersects(AABB a, AABB b, int width, int height) {
	torus_normalize_two_aabb(width, height, &a, &b);
	return a._intersects(b);
}

double torus_distance(Vector3 *p_pos, Vector3 ref_pos, int width, int height) {
	Vector3 pos = *p_pos;
	Vector2 candidates[5] = {
		Vector2(pos.x, pos.z),
		Vector2(pos.x + width, pos.z),
		Vector2(pos.x - width, pos.z),
		Vector2(pos.x, pos.z + height),
		Vector2(pos.x, pos.z - height),
	};
	Vector2 ref_pos2d(ref_pos.x, ref_pos.z);
	double min_dist = DOUBLE_MAX;
	for (int i = 0; i < 5; i++) {
		double dist = (candidates[i] - ref_pos2d).length();
		if (dist < min_dist) {
			min_dist = dist;
			pos.x = candidates[i].x;
			pos.z = candidates[i].y;
		}
	}
	*p_pos = pos;
	return min_dist;
}

float AABB::find_max_separation(const AABB &other, Vector3 *p_reference_normal) const {
	float sep_x1 = other.vmin.x - vmax.x;
	float sep_x2 = vmin.x - other.vmax.x;
	float sep_y1 = other.vmin.y - vmax.y;
	float sep_y2 = vmin.y - other.vmax.y;
	float sep_z1 = other.vmin.z - vmax.z;
	float sep_z2 = vmin.z - other.vmax.z;

	Vector3 max_sep, sign;
	max_sep.x = sep_x2 > sep_x1 ? sep_x2 : sep_x1;
	max_sep.y = sep_y2 > sep_y1 ? sep_y2 : sep_y1;
	max_sep.z = sep_z2 > sep_z1 ? sep_z2 : sep_z1;
	sign.x = sep_x2 > sep_x1 ? 1 : -1;
	sign.y = sep_y2 > sep_y1 ? 1 : -1;
	sign.z = sep_z2 > sep_z1 ? 1 : -1;

	Vector3 normal(0, 0, 0);
	int max_axis = max_sep.max_axis_index();
	if (max_sep[max_axis] < FLOAT_EPS) {
		normal[max_axis] = sign[max_axis];
		*p_reference_normal = normal;
		return max_sep[max_axis];
	} else {
		for (int axis = 0; axis < 3; axis++) {
			if (max_sep[axis] >= FLOAT_EPS) {
				normal[axis] = max_sep[axis] * sign[axis];
			}
		}
		float length = normal.length();
		normal /= length;
		*p_reference_normal = normal;
		return length;
	}
}

} // namespace sbx