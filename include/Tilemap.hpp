#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>

namespace sbx {

using TileID = uint16_t;

enum class TileLayer {
	BASE,
	FLOOR,
	SLIME,
	BLOCK,
	COUNT,
};

union Tile {
	TileID data[4];
	struct {
		TileID base;
		TileID floor;
		TileID slime;
		TileID block;
	};
};

struct Tilemap {
	int full_w;
	int full_h;
	int slice_x, slice_y, slice_w, slice_h;
	std::shared_ptr<Tile> tiles;

	Tilemap() = default;
	Tilemap(int width, int height) :
			full_w(width),
			full_h(height),
			slice_x(0),
			slice_y(0),
			slice_w(width),
			slice_h(height) {
		tiles = std::shared_ptr<Tile>(new Tile[width * height], std::default_delete<Tile[]>());
		std::memset(tiles.get(), 0, width * height * sizeof(Tile));
	}

	int width() const { return slice_w; }
	int height() const { return slice_h; }
	static int n_layers() { return sizeof(Tile) / sizeof(TileID); }

	bool is_valid_xy(int x, int y) const {
		return x >= 0 && y >= 0 && x < slice_w && y < slice_h;
	}

	bool is_valid_xyl(int x, int y, int layer) const {
		return is_valid_xy(x, y) && layer >= 0 && layer < Tilemap::n_layers();
	}

	Tile *get(int x, int y) const {
		assert(is_valid_xy(x, y));
		return tiles.get() + ((slice_y + y) * full_w + (slice_x + x));
	}

	Tilemap slice_reverted() {
		Tilemap res = *this;
		res.slice_x = 0;
		res.slice_y = 0;
		res.slice_w = full_w;
		res.slice_h = full_h;
		return res;
	}

	Tilemap sliced(int x, int y, int w, int h) {
		assert(x >= 0 && y >= 0 && w > 0 && h > 0);
		assert(x + w <= slice_w && y + h <= slice_h);
		Tilemap res = *this;
		res.slice_x += x;
		res.slice_y += y;
		res.slice_w = w;
		res.slice_h = h;
		return res;
	}
};

} // namespace sbx