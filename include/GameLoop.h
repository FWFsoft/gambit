#pragma once

#include <chrono>
#include <functional>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

class GameLoop {
 public:
  GameLoop();

  void run();
  void tick();  // Single iteration — called by run() or emscripten main loop
  void stop();

  bool isRunning() const { return running; }

 private:
  bool running;
  uint64_t frameNumber;
  float accumulator = 0.0f;
  std::chrono::steady_clock::time_point lastFrameTime;

  static constexpr float TARGET_DELTA_MS = 16.67f;  // 60 FPS
  static constexpr float MAX_FRAME_TIME_MS =
      33.0f;  // 30 FPS minimum (Tiger Style threshold)

#ifdef __EMSCRIPTEN__
  static void emscriptenMainLoop(void* arg);
#endif
};
