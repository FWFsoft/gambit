#pragma once

// Animation configuration
// Game-specific animation frame layouts and timings

namespace Config {
namespace Animation {

// Frame duration (milliseconds)
constexpr float IDLE_FRAME_DURATION = 1000.0f;  // 1 second per idle frame
constexpr float WALK_FRAME_DURATION = 100.0f;   // 100ms per walk frame (10 FPS)

// Sprite sheet layout for player animations
// Each direction uses one row of 32x32 frames
constexpr int ROW_SOUTH = 0;
constexpr int ROW_SOUTHWEST = 32;
constexpr int ROW_WEST = 64;
constexpr int ROW_NORTHWEST = 96;
constexpr int ROW_NORTH = 128;
constexpr int ROW_NORTHEAST = 160;
constexpr int ROW_EAST = 192;
constexpr int ROW_SOUTHEAST = 224;

// Column offsets
constexpr int COL_IDLE = 0;
constexpr int COL_WALK_START = 128;  // Walk animations start at column 128

}  // namespace Animation
}  // namespace Config
