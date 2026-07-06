#include "Torus.hpp"
#include <cassert>

namespace sbx {

Vector3 project_point_on_line(Vector3 point, Line line) {
	Vector3 d = line[1] - line[0];
	if (fabs(d.x) < LINE_IS_POINT_THRESHOLD && fabs(d.y) < LINE_IS_POINT_THRESHOLD && fabs(d.z) < LINE_IS_POINT_THRESHOLD) {
		return line[0];
	}
	Vector3 v = point - line[0];
	float t = v.dot(d) / d.dot(d);
	t = CLAMP(t, 0.0f, 1.0f);
	return line[0] + t * d;
}

int torus_iter_chunks_1d(int size, int chunk_size, double dmin, double dmax, int *out, int out_size) {
	int n = size / chunk_size;
	assert(size % chunk_size == 0);

	double d0 = posmodf(dmin, size);
	double d1 = posmodf(dmax, size);

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
	const double w = (double)width;
	const double h = (double)height;

	Vector2 rel_xz = p_aabb_b->position_xz() - p_aabb_a->position_xz();
	rel_xz.x = posmodf(rel_xz.x + 0.5 * w, w) - 0.5 * w;
	rel_xz.y = posmodf(rel_xz.y + 0.5 * h, h) - 0.5 * h;

	p_aabb_a->set_position(Vector3(0, 0, 0));
	p_aabb_b->set_position(Vector3(rel_xz.x, 0, rel_xz.y));
}

void AAFace::get_ccw_points(Vector3 p_points[4]) const {
	static const int LUT[6][4] = {
		{ 0b000, 0b100, 0b110, 0b010 }, /* axis=0 sign=-1  (-X) */
		{ 0b001, 0b011, 0b111, 0b101 }, /* axis=0 sign=+1  (+X) */
		{ 0b000, 0b001, 0b101, 0b100 }, /* axis=1 sign=-1  (-Y) */
		{ 0b010, 0b110, 0b111, 0b011 }, /* axis=1 sign=+1  (+Y) */
		{ 0b000, 0b010, 0b011, 0b001 }, /* axis=2 sign=-1  (-Z) */
		{ 0b100, 0b101, 0b111, 0b110 }, /* axis=2 sign=+1  (+Z) */
	};
	int row = normal.axis * 2 + (normal.sign > 0 ? 1 : 0);
	for (int i = 0; i < 4; i++) {
		int mask = LUT[row][i];
		p_points[i] = Vector3(
				((mask >> 0) & 1) ? vmax.x : vmin.x,
				((mask >> 1) & 1) ? vmax.y : vmin.y,
				((mask >> 2) & 1) ? vmax.z : vmin.z);
	}
}

void AAFace::get_parallel_edges(int axis, Line p_edges[2]) const {
	assert(axis != normal.axis);
	int t = 3 - normal.axis - axis;
	p_edges[0][0] = vmin;
	p_edges[0][1] = vmin;
	p_edges[0][1][axis] = vmax[axis];
	p_edges[1][0] = vmin;
	p_edges[1][0][t] = vmax[t];
	p_edges[1][1] = p_edges[1][0];
	p_edges[1][1][axis] = vmax[axis];
}

Vector3 AAFace::find_closest_distance(const AAFace &other) const {
	float min_dist_squared = FLOAT_MAX;
	Vector3 min_dir;

	// a -----
	//		^
	//		| normal
	//   b -----
	for (int axis = 0; axis < 3; axis++) {
		if (axis == normal.axis)
			continue;

		Line edges_a[2], edges_b[2];
		get_parallel_edges(axis, edges_a);
		other.get_parallel_edges(axis, edges_b);

		for (int i = 0; i < 2; i++) {
			for (int j = 0; j < 2; j++) {
				for (int k = 0; k < 2; k++) {
					Vector3 vert_a = edges_a[i][k];
					Vector3 proj_b = project_point_on_line(vert_a, edges_b[j]);
					Vector3 dir = vert_a - proj_b;
					if (dir.length_squared() < min_dist_squared) {
						min_dist_squared = dir.length_squared();
						min_dir = dir;
						// print_line("vert_a: " + vert_a + ", proj_b: " + proj_b);
					}
				}
			}
		}
	}
	return min_dir;
}

AAFace AABB::get_face(UnitVector3 normal) const {
	AAFace face;
	face.normal = normal;
	face.vmin = vmin;
	face.vmax = vmax;
	int axis = normal.axis;
	if (normal.sign > 0) {
		face.vmin[axis] = vmax[axis];
	} else {
		face.vmax[axis] = vmin[axis];
	}
	return face;
}

float AABB::find_max_separation(const AABB &other, UnitVector3 *p_reference_normal) const {
	float sep_x1 = other.vmin.x - vmax.x;
	float sep_x2 = vmin.x - other.vmax.x;
	float sep_y1 = other.vmin.y - vmax.y;
	float sep_y2 = vmin.y - other.vmax.y;
	float sep_z1 = other.vmin.z - vmax.z;
	float sep_z2 = vmin.z - other.vmax.z;

	float max_sep = sep_x1;
	*p_reference_normal = UnitVector3{ 0, -1 };
	if (sep_x2 > max_sep) {
		max_sep = sep_x2;
		*p_reference_normal = UnitVector3{ 0, 1 };
	}
	if (sep_y1 > max_sep) {
		max_sep = sep_y1;
		*p_reference_normal = UnitVector3{ 1, -1 };
	}
	if (sep_y2 > max_sep) {
		max_sep = sep_y2;
		*p_reference_normal = UnitVector3{ 1, 1 };
	}
	if (sep_z1 > max_sep) {
		max_sep = sep_z1;
		*p_reference_normal = UnitVector3{ 2, -1 };
	}
	if (sep_z2 > max_sep) {
		max_sep = sep_z2;
		*p_reference_normal = UnitVector3{ 2, 1 };
	}
	return max_sep;
}

} // namespace sbx