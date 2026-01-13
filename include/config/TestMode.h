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
      30;  // Capture every N frames (30 = every 0.5s at 60fps)

  // Current frame counter
  int frameCount = 0;
};

// Global test config instance
inline TestConfig testConfig;

}  // namespace TestMode
}  // namespace Config
