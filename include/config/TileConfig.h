#pragma once

// Tile and map configuration
// Settings for isometric tile rendering

namespace Config {
namespace Tile {

// Isometric tile dimensions
constexpr int WIDTH = 64;   // Tile width in pixels
constexpr int HEIGHT = 32;  // Tile height in pixels

// Tile batch rendering
constexpr int BATCH_SIZE = 30;        // Max tiles per batch (30x30)
constexpr int VERTICES_PER_TILE = 6;  // 2 triangles = 6 vertices
constexpr int FLOATS_PER_VERTEX = 4;  // x, y, u, v

// Total buffer size for batch rendering
constexpr int BATCH_BUFFER_SIZE =
    BATCH_SIZE * BATCH_SIZE * VERTICES_PER_TILE * FLOATS_PER_VERTEX;

}  // namespace Tile
}  // namespace Config
