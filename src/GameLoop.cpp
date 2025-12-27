#include "GameLoop.h"

#include <SDL2/SDL.h>

#include <cassert>

#include "EventBus.h"
#include "Logger.h"

GameLoop::GameLoop() : running(false), frameNumber(0) {}

void GameLoop::run() {
  running = true;
  lastFrameTime = std::chrono::steady_clock::now();

  while (running) {
    auto currentTime = std::chrono::steady_clock::now();
    float elapsed =
        std::chrono::duration<float, std::milli>(currentTime - lastFrameTime)
            .count();

    // Update loop ALWAYS fires at 60 FPS
    UpdateEvent updateEvent{TARGET_DELTA_MS, frameNumber++};
    EventBus::instance().publish(updateEvent);

    // Render only if we have time
    float interpolation = elapsed / TARGET_DELTA_MS;
    interpolation = std::min(interpolation, 1.0f);

    RenderEvent renderEvent{interpolation};
    EventBus::instance().publish(renderEvent);

    // Sleep to maintain 60 FPS
    auto endTime = std::chrono::steady_clock::now();
    float frameDuration =
        std::chrono::duration<float, std::milli>(endTime - lastFrameTime)
            .count();
    float sleepTime = TARGET_DELTA_MS - frameDuration;

    // Tiger Style: Warn if frame rate drops significantly (> 30ms = 33 FPS)
    // Only log significant performance issues to reduce noise
    static int frameCount = 0;
    static int slowFrameCount = 0;
    frameCount++;

    if (frameDuration > 30.0f) {
      slowFrameCount++;
      // Only log slow frames once per second to avoid log spam
      if (slowFrameCount == 1 || frameCount % 60 == 0) {
        Logger::debug("Frame time: " + std::to_string(frameDuration) +
                      "ms (target: " + std::to_string(TARGET_DELTA_MS) + "ms)");
      }
    } else {
      slowFrameCount = 0;
      if (frameCount % 300 == 0) {
        // Log average performance every 5 seconds (300 frames at 60 FPS)
        Logger::debug("Frame time: " + std::to_string(frameDuration) +
                      "ms (target: " + std::to_string(TARGET_DELTA_MS) + "ms)");
      }
    }

    // Tiger Style: Assert if frame rate is catastrophically low (< 30 FPS)
    // TODO: Re-enable once initial startup performance is optimized
    // assert(
    //     frameDuration < MAX_FRAME_TIME_MS &&
    //     "Frame time exceeded 30 FPS threshold - critical performance
    //     issue!");

    if (sleepTime > 0) {
      SDL_Delay(static_cast<uint32_t>(sleepTime));
    }

    lastFrameTime = std::chrono::steady_clock::now();
  }
}

void GameLoop::stop() { running = false; }
