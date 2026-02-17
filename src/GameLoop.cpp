#include "GameLoop.h"

#include <SDL2/SDL.h>

#include <cassert>

#include "EventBus.h"
#include "Logger.h"

GameLoop::GameLoop() : running(false), frameNumber(0) {}

void GameLoop::tick() {
  auto currentTime = std::chrono::steady_clock::now();
  float elapsed =
      std::chrono::duration<float, std::milli>(currentTime - lastFrameTime)
          .count();

#ifdef __EMSCRIPTEN__
  // In WASM, requestAnimationFrame may call us faster than 60fps on
  // high-refresh displays. Accumulate time and only update at 60fps.
  accumulator += elapsed;
  lastFrameTime = currentTime;

  // Cap accumulator to prevent lag spiral (max 1 catch-up frame)
  if (accumulator > TARGET_DELTA_MS * 2.0f) {
    accumulator = TARGET_DELTA_MS * 2.0f;
  }

  while (accumulator >= TARGET_DELTA_MS) {
    UpdateEvent updateEvent{TARGET_DELTA_MS, frameNumber++};
    EventBus::instance().publish(updateEvent);
    accumulator -= TARGET_DELTA_MS;
  }

  // Always render (browser controls vsync via requestAnimationFrame)
  float interpolation = accumulator / TARGET_DELTA_MS;
  RenderEvent renderEvent{interpolation};
  EventBus::instance().publish(renderEvent);
  EventBus::instance().publish(SwapBuffersEvent{});
#else
  // Update loop ALWAYS fires at 60 FPS
  UpdateEvent updateEvent{TARGET_DELTA_MS, frameNumber++};
  EventBus::instance().publish(updateEvent);

  // Render only if we have time
  float interpolation = elapsed / TARGET_DELTA_MS;
  interpolation = std::min(interpolation, 1.0f);

  RenderEvent renderEvent{interpolation};
  EventBus::instance().publish(renderEvent);

  // Swap buffers after all rendering is complete
  EventBus::instance().publish(SwapBuffersEvent{});

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

  if (sleepTime > 0) {
    SDL_Delay(static_cast<uint32_t>(sleepTime));
  }

  lastFrameTime = std::chrono::steady_clock::now();
#endif
}

void GameLoop::run() {
  running = true;
  lastFrameTime = std::chrono::steady_clock::now();

#ifdef __EMSCRIPTEN__
  // fps=0 means use requestAnimationFrame (syncs to monitor refresh)
  // simulate_infinite_loop=true prevents code after this from executing
  emscripten_set_main_loop_arg(emscriptenMainLoop, this, 0, true);
#else
  while (running) {
    tick();
  }
#endif
}

#ifdef __EMSCRIPTEN__
void GameLoop::emscriptenMainLoop(void* arg) {
  GameLoop* loop = static_cast<GameLoop*>(arg);
  if (loop->running) {
    loop->tick();
  } else {
    emscripten_cancel_main_loop();
  }
}
#endif

void GameLoop::stop() { running = false; }
