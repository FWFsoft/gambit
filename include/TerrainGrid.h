#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// The ground's shape, one cell per map tile: how high it is, whether it can
// be walked on, and which region of the planet it belongs to.
//
// Read from three hidden tile layers of the map (TiledMap fills it), so the
// generator writes them and Tiled can repaint them by hand:
//   "Height"  ground level 0..15 (tile id in the terrain_data tileset)
//   "Region"  region 0..15 (0 = no region: sea, void)
//   "Blocked" any tile = nobody walks here (sea, peaks, city blocks)
//   "Biome"   biome 0..15 (0 = none: sea, void); see Biome.h
// A map without these layers (the old arena) is flat and open everywhere.
//
// Elevation is gameplay-neutral (docs/design: "Map & Environment Design",
// elevation vs verticality): height only lifts what is drawn. Walking up a
// slope is free and attacks do not miss because of it. What stops movement
// is the Blocked layer.
//
// World space is the engine's: screen-aligned, y down, the map's centre at
// the origin. A tile's top corner sits at corner (tx, ty); its four corners
// are (tx, ty) top, (tx+1, ty) right, (tx+1, ty+1) bottom, (tx, ty+1) left.
class TerrainGrid {
 public:
  // World pixels one height level lifts the ground on screen.
  static constexpr float LEVEL_PIXELS = 16.0f;
  // The level drawn at zero lift (the crash site sits here).
  static constexpr int BASE_LEVEL = 4;
  static constexpr int LEVEL_COUNT = 16;
  static constexpr int REGION_COUNT = 16;
  static constexpr int BIOME_COUNT = 16;

  void reset(int width, int height, int tileWidth, int tileHeight) {
    assert(width > 0 && height > 0 && tileWidth > 0 && tileHeight > 0);
    w = width;
    h = height;
    halfTileW = tileWidth / 2.0f;
    quarterTileH = tileHeight / 4.0f;
    centreY = ((w - 1) / 2.0f + (h - 1) / 2.0f) * quarterTileH;
    levels.assign(static_cast<size_t>(w * h), BASE_LEVEL);
    regions.assign(static_cast<size_t>(w * h), 0);
    biomes.assign(static_cast<size_t>(w * h), 0);
    blocked.assign(static_cast<size_t>(w * h), 0);
    corners.clear();
    loaded = false;
  }

  // Called by the loader once every cell is set: builds the corner heights.
  void finish() {
    assert(w > 0 && h > 0);
    corners.assign(static_cast<size_t>((w + 1) * (h + 1)), 0.0f);
    for (int cy = 0; cy <= h; cy++) {
      for (int cx = 0; cx <= w; cx++) {
        float sum = 0.0f;
        int n = 0;
        for (int dy = -1; dy <= 0; dy++) {
          for (int dx = -1; dx <= 0; dx++) {
            int tx = cx + dx, ty = cy + dy;
            if (tx < 0 || ty < 0 || tx >= w || ty >= h) continue;
            sum += levels[ty * w + tx];
            n++;
          }
        }
        float level = n > 0 ? sum / n : 0.0f;
        corners[cy * (w + 1) + cx] = (level - BASE_LEVEL) * LEVEL_PIXELS;
      }
    }
    loaded = true;
  }

  bool active() const { return loaded; }
  int width() const { return w; }
  int height() const { return h; }

  void setCell(int tx, int ty, int level, int region, bool isBlocked,
               int biome = 0) {
    assert(inBounds(tx, ty));
    assert(level >= 0 && level < LEVEL_COUNT);
    assert(region >= 0 && region < REGION_COUNT);
    assert(biome >= 0 && biome < BIOME_COUNT);
    size_t i = static_cast<size_t>(ty * w + tx);
    levels[i] = static_cast<uint8_t>(level);
    regions[i] = static_cast<uint8_t>(region);
    biomes[i] = static_cast<uint8_t>(biome);
    blocked[i] = isBlocked ? 1 : 0;
  }

  bool inBounds(int tx, int ty) const {
    return tx >= 0 && ty >= 0 && tx < w && ty < h;
  }

  // Fractional corner coordinates of a world point.
  void worldToCorner(float wx, float wy, float& cx, float& cy) const {
    float diff = wx / halfTileW;                         // cx - cy
    float sum = (wy + centreY + quarterTileH) / quarterTileH;  // cx + cy
    cx = (sum + diff) * 0.5f;
    cy = (sum - diff) * 0.5f;
  }
  // World position of corner (cx, cy) on flat ground.
  void cornerToWorld(float cx, float cy, float& wx, float& wy) const {
    wx = (cx - cy) * halfTileW;
    wy = (cx + cy) * quarterTileH - quarterTileH - centreY;
  }
  bool tileAt(float wx, float wy, int& tx, int& ty) const {
    float cx, cy;
    worldToCorner(wx, wy, cx, cy);
    tx = static_cast<int>(std::floor(cx));
    ty = static_cast<int>(std::floor(cy));
    return inBounds(tx, ty);
  }

  // How far the ground here is lifted on screen (world pixels, up is
  // positive). Smooth: interpolated between the tile corners, exactly the
  // surface the ground mesh draws.
  float liftAt(float wx, float wy) const {
    if (!loaded) return 0.0f;
    float cx, cy;
    worldToCorner(wx, wy, cx, cy);
    cx = std::clamp(cx, 0.0f, static_cast<float>(w) - 0.001f);
    cy = std::clamp(cy, 0.0f, static_cast<float>(h) - 0.001f);
    int x0 = static_cast<int>(cx), y0 = static_cast<int>(cy);
    float fx = cx - x0, fy = cy - y0;
    float a = cornerLift(x0, y0), b = cornerLift(x0 + 1, y0);
    float c = cornerLift(x0, y0 + 1), d = cornerLift(x0 + 1, y0 + 1);
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
  }
  float cornerLift(int cx, int cy) const {
    assert(loaded);
    assert(cx >= 0 && cy >= 0 && cx <= w && cy <= h);
    return corners[cy * (w + 1) + cx];
  }

  // Off the map counts as blocked once a terrain is loaded.
  bool blockedAt(float wx, float wy) const {
    if (!loaded) return false;
    int tx, ty;
    if (!tileAt(wx, wy, tx, ty)) return true;
    return blocked[ty * w + tx] != 0;
  }
  // A body of `radius` standing here touches a blocked cell.
  bool blocksBody(float x, float y, float radius) const {
    if (!loaded) return false;
    const float r = radius * 0.7f;
    return blockedAt(x, y) || blockedAt(x + r, y) || blockedAt(x - r, y) ||
           blockedAt(x, y + r * 0.5f) || blockedAt(x, y - r * 0.5f);
  }
  // 0 = no region (sea, void, off the map).
  int regionAt(float wx, float wy) const {
    if (!loaded) return 0;
    int tx, ty;
    if (!tileAt(wx, wy, tx, ty)) return 0;
    return regions[ty * w + tx];
  }
  // 0 = no biome (sea, void, off the map, a map without a Biome layer).
  int biomeAt(float wx, float wy) const {
    if (!loaded) return 0;
    int tx, ty;
    if (!tileAt(wx, wy, tx, ty)) return 0;
    return biomes[ty * w + tx];
  }
  int biomeOf(int tx, int ty) const {
    assert(inBounds(tx, ty));
    return biomes[ty * w + tx];
  }
  int levelOf(int tx, int ty) const {
    assert(inBounds(tx, ty));
    return levels[ty * w + tx];
  }
  int regionOf(int tx, int ty) const {
    assert(inBounds(tx, ty));
    return regions[ty * w + tx];
  }
  bool blockedTile(int tx, int ty) const {
    assert(inBounds(tx, ty));
    return blocked[ty * w + tx] != 0;
  }

  // The nearest open spot to (x, y) for a body of `radius`, searching out to
  // `maxDist`. Returns false (and leaves x, y) if there is none.
  bool nearestOpen(float& x, float& y, float radius, float maxDist) const {
    if (!blocksBody(x, y, radius)) return true;
    for (float d = 12.0f; d <= maxDist; d += 12.0f) {
      for (int i = 0; i < 16; i++) {
        float a = i * 0.39269908f;  // 2 pi / 16
        float px = x + std::cos(a) * d, py = y + std::sin(a) * d;
        if (!blocksBody(px, py, radius)) {
          x = px;
          y = py;
          return true;
        }
      }
    }
    return false;
  }

 private:
  int w = 0, h = 0;
  float halfTileW = 32.0f, quarterTileH = 16.0f, centreY = 0.0f;
  std::vector<uint8_t> levels;
  std::vector<uint8_t> regions;
  std::vector<uint8_t> biomes;
  std::vector<uint8_t> blocked;
  std::vector<float> corners;
  bool loaded = false;
};

// Set dressing placed by the map generator (Tiled object layer "Props",
// point objects): a rock, a plant, a landmark, a city block. Drawn by the
// client from the prop catalog (assets/props/catalog.json); a prop whose
// sprite is not made yet is drawn as a placeholder. Solid props also have a
// Blocked cell or a terrain rectangle under them: this list is only looks.
struct MapProp {
  std::string kind;   // catalog key, e.g. "rock", "egg_sac", "city_block"
  float x = 0.0f, y = 0.0f;
  float scale = 1.0f;
  int variant = 0;    // which look of the kind (seeded by the generator)
  int region = 0;     // the region it stands in (for genus-dressed kinds)
  float width = 0.0f, depth = 0.0f, tall = 0.0f;  // block-like kinds
};
