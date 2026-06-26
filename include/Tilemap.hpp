#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>

namespace sbx {

using TileID = uint16_t;

struct Tilemap {
	int width;
	int height;
	int n_layers;
	int slice_x, slice_y, slice_w, slice_h;
	std::shared_ptr<TileID> tiles;

	Tilemap(int width, int height, int n_layers) :
			width(width),
			height(height),
			n_layers(n_layers),
			slice_x(0),
			slice_y(0),
			slice_w(width),
			slice_h(height) {
		tiles = std::shared_ptr<TileID>(new TileID[width * height * n_layers], std::default_delete<TileID[]>());
		std::memset(tiles.get(), 0, width * height * n_layers * sizeof(TileID));
	}

	TileID *addr(int layer, int x, int y) const {
		if (layer < 0 || layer >= n_layers) {
			return nullptr;
		}
		if (x < 0 || x >= slice_w || y < 0 || y >= slice_h) {
			return nullptr;
		}
		return addr_nocheck(layer, x, y);
	}

	TileID *addr_nocheck(int layer, int x, int y) const {
		return tiles.get() + (layer * width * height + ((slice_y + y) * width + (slice_x + x)));
	}

	Tilemap slice_reverted() {
		Tilemap res = *this;
		res.slice_x = 0;
		res.slice_y = 0;
		res.slice_w = width;
		res.slice_h = height;
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