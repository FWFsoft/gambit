#pragma once

#include <string>

namespace Config {
namespace TestMode {

// Test mode configuration for visual testing
struct TestConfig {
  bool enabled = false;

  // Output paths
  std::string screenshotPath = "test_output/frame_latest.png";
  std::string inputCommandPath = "test_input.txt";
  std::string stateOutputPath = "test_output/state.json";

  // Screenshot capture settings
  int screenshotInterval =
      60;  // Capture every N frames (60 = every 1.0s at 60fps)

  // --record N: also save every Nth frame as test_output/rec_00001.png, ...
  // (0 = off). Stitch them with ffmpeg to make clips.
  int recordInterval = 0;
  int recordedFrames = 0;

  // Current frame counter
  int frameCount = 0;
};

// Global test config instance
inline TestConfig testConfig;

}  // namespace TestMode
}  // namespace Config
