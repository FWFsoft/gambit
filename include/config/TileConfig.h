#pragma once

// Tile and map configuration
// Settings for isometric tile rendering

namespace Config {
namespace Tile {

// Isometric tile dimensions
constexpr int WIDTH = 64;   // Tile width in pixels
constexpr int HEIGHT = 32;  // Tile height in pixels

// Tile batch rendering
constexpr int VERTICES_PER_TILE = 6;  // 2 triangles = 6 vertices
constexpr int FLOATS_PER_VERTEX = 4;  // x, y, u, v

// NOTE: Buffer size is now dynamically allocated based on actual map
// dimensions. No fixed BATCH_SIZE or BATCH_BUFFER_SIZE needed - TileRenderer
// auto-resizes.

}  // namespace Tile
}  // namespace Config
