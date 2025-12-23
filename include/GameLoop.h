#pragma once

#include <chrono>
#include <functional>

class GameLoop {
 public:
  GameLoop();

  void run();
  void stop();

  bool isRunning() const { return running; }

 private:
  bool running;
  uint64_t frameNumber;
  std::chrono::steady_clock::time_point lastFrameTime;

  static constexpr float TARGET_DELTA_MS = 16.67f;  // 60 FPS
  static constexpr float MAX_FRAME_TIME_MS =
      33.0f;  // 30 FPS minimum (Tiger Style threshold)
};
