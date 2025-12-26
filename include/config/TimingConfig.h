#pragma once

// Timing and frame rate configuration
// Engine-level timing constants

namespace Config {
namespace Timing {

// Target frame rate
constexpr int TARGET_FPS = 60;
constexpr float TARGET_DELTA_MS = 16.67f;  // 1000ms / 60fps
constexpr float TARGET_DELTA_SECONDS = TARGET_DELTA_MS / 1000.0f;

// Frame time limits
constexpr float MAX_FRAME_TIME_MS =
    33.0f;  // Minimum 30 FPS (slowest acceptable)

// Input history
constexpr int INPUT_HISTORY_SIZE = 60;  // 1 second of inputs at 60 FPS

// Network snapshots
constexpr int MAX_SNAPSHOTS = 3;  // ~50ms interpolation buffer

// Logging frequency
constexpr int LOG_FRAME_INTERVAL = 60;  // Log every second (60 frames)
constexpr int LOG_SLOW_FRAME_INTERVAL =
    300;  // Log every 5 seconds (300 frames)

}  // namespace Timing
}  // namespace Config
