#pragma once

// Animation configuration
// Game-specific animation frame layouts and timings

namespace Config {
namespace Animation {

// Frame duration (milliseconds)
constexpr float IDLE_FRAME_DURATION = 1000.0f;  // 1 second per idle frame
constexpr float WALK_FRAME_DURATION = 100.0f;   // 100ms per walk frame (10 FPS)

// Sprite sheet layout for player animations (256x256 PNG, 32x32 frames)
// Row 0 (y=0): Idle animation
// Row 1 (y=32): Walk North (4 frames)
// Row 2 (y=64): Walk Northeast (4 frames)
// Row 3 (y=96): Walk East (4 frames)
// Row 4 (y=128): Walk Southeast (4 frames)
// Row 5 (y=160): Walk South (4 frames)
// Row 6 (y=192): Walk Southwest (4 frames)
// Row 7 (y=224): Walk West (4 frames)
// Special: Walk Northwest (y=0, x=128)
constexpr int ROW_NORTH = 32;
constexpr int ROW_NORTHEAST = 64;
constexpr int ROW_EAST = 96;
constexpr int ROW_SOUTHEAST = 128;
constexpr int ROW_SOUTH = 160;
constexpr int ROW_SOUTHWEST = 192;
constexpr int ROW_WEST = 224;
constexpr int ROW_NORTHWEST = 0;  // Special case: uses COL_WALK_START

// Column offsets
constexpr int COL_IDLE = 0;
constexpr int COL_WALK_START = 128;  // Walk animations start at column 128

}  // namespace Animation
}  // namespace Config
