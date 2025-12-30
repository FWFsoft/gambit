#include <atomic>
#include <thread>

#include "GameLoop.h"
#include "Logger.h"
#include "test_utils.h"

TEST(GameLoop_PublishesUpdateEvent) {
  resetEventBus();

  GameLoop gameLoop;

  std::atomic<int> updateCount{0};
  std::atomic<bool> shouldStop{false};

  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
    updateCount++;

    // Verify delta time is correct
    assert(e.deltaTime == 16.67f);

    // Stop after 5 updates
    if (updateCount >= 5) {
      shouldStop = true;
    }
  });

  // Run in separate thread to avoid blocking
  std::thread loopThread([&]() { gameLoop.run(); });

  // Wait for updates
  while (updateCount < 5 && !shouldStop) {
    SDL_Delay(10);
  }

  gameLoop.stop();
  loopThread.join();

  // Verify we got at least 5 updates
  assert(updateCount >= 5);
}

TEST(GameLoop_PublishesRenderEvent) {
  resetEventBus();

  GameLoop gameLoop;

  std::atomic<int> renderCount{0};

  EventBus::instance().subscribe<RenderEvent>([&](const RenderEvent& e) {
    renderCount++;

    // Verify interpolation is between 0.0 and 1.0
    assert(e.interpolation >= 0.0f && e.interpolation <= 1.0f);
  });

  std::thread loopThread([&]() { gameLoop.run(); });

  // Wait for renders
  SDL_Delay(100);

  gameLoop.stop();
  loopThread.join();

  // Verify we got at least 3 renders (100ms / 16.67ms = ~6 frames)
  assert(renderCount >= 3);
}

TEST(GameLoop_PublishesSwapBuffersEvent) {
  resetEventBus();

  GameLoop gameLoop;

  std::atomic<int> swapCount{0};

  EventBus::instance().subscribe<SwapBuffersEvent>(
      [&](const SwapBuffersEvent&) { swapCount++; });

  std::thread loopThread([&]() { gameLoop.run(); });

  SDL_Delay(100);

  gameLoop.stop();
  loopThread.join();

  assert(swapCount >= 3);
}

TEST(GameLoop_FrameNumberIncreases) {
  resetEventBus();

  GameLoop gameLoop;

  std::atomic<uint64_t> lastFrameNumber{0};
  std::atomic<int> updateCount{0};

  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
    // Frame number should always increase (or be first frame)
    assert(e.frameNumber >= lastFrameNumber);
    lastFrameNumber = e.frameNumber;
    updateCount++;
  });

  std::thread loopThread([&]() { gameLoop.run(); });

  while (updateCount < 10) {
    SDL_Delay(10);
  }

  gameLoop.stop();
  loopThread.join();

  // Verify we got at least 10 updates (frame numbers may not match update count
  // exactly)
  assert(updateCount >= 10);
}

TEST(GameLoop_StopWorks) {
  resetEventBus();

  GameLoop gameLoop;

  std::atomic<int> updateCount{0};

  EventBus::instance().subscribe<UpdateEvent>(
      [&](const UpdateEvent&) { updateCount++; });

  std::thread loopThread([&]() { gameLoop.run(); });

  // Let it run for a bit
  SDL_Delay(50);

  int countBeforeStop = updateCount;

  // Stop the loop
  gameLoop.stop();
  loopThread.join();

  // Wait a bit more
  SDL_Delay(50);

  // Count should not have increased after stop
  assert(updateCount == countBeforeStop);
}

TEST(GameLoop_EventOrder) {
  // Test that events are published in correct order: Update, Render,
  // SwapBuffers
  resetEventBus();

  GameLoop gameLoop;

  std::atomic<int> eventSequence{0};
  std::atomic<bool> orderCorrect{true};

  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent&) {
    if (eventSequence % 3 != 0) {
      orderCorrect = false;
    }
    eventSequence++;
  });

  EventBus::instance().subscribe<RenderEvent>([&](const RenderEvent&) {
    if (eventSequence % 3 != 1) {
      orderCorrect = false;
    }
    eventSequence++;
  });

  EventBus::instance().subscribe<SwapBuffersEvent>(
      [&](const SwapBuffersEvent&) {
        if (eventSequence % 3 != 2) {
          orderCorrect = false;
        }
        eventSequence++;
      });

  std::thread loopThread([&]() { gameLoop.run(); });

  // Let a few frames run
  SDL_Delay(100);

  gameLoop.stop();
  loopThread.join();

  assert(orderCorrect);
}

TEST(GameLoop_MultipleStartStop) {
  // Test starting and stopping multiple times
  resetEventBus();

  for (int i = 0; i < 3; i++) {
    GameLoop gameLoop;

    std::atomic<int> updateCount{0};

    EventBus::instance().subscribe<UpdateEvent>(
        [&](const UpdateEvent&) { updateCount++; });

    std::thread loopThread([&]() { gameLoop.run(); });

    while (updateCount < 5) {
      SDL_Delay(10);
    }

    gameLoop.stop();
    loopThread.join();

    assert(updateCount >= 5);

    resetEventBus();
  }
}

int main() {
  Logger::init();

  test_GameLoop_PublishesUpdateEvent();
  test_GameLoop_PublishesRenderEvent();
  test_GameLoop_PublishesSwapBuffersEvent();
  test_GameLoop_FrameNumberIncreases();
  test_GameLoop_StopWorks();
  test_GameLoop_EventOrder();
  test_GameLoop_MultipleStartStop();

  return 0;
}
