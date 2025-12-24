#include <SDL2/SDL.h>

#include "InputSystem.h"
#include "Logger.h"
#include "test_utils.h"

TEST(InputSystem_KeyBindings) {
  struct KeyTest {
    int sdlKey;
    bool* flagPtr(LocalInputEvent& e) const {
      if (sdlKey == SDLK_w || sdlKey == SDLK_UP) return &e.moveUp;
      if (sdlKey == SDLK_s || sdlKey == SDLK_DOWN) return &e.moveDown;
      if (sdlKey == SDLK_a || sdlKey == SDLK_LEFT) return &e.moveLeft;
      if (sdlKey == SDLK_d || sdlKey == SDLK_RIGHT) return &e.moveRight;
      return nullptr;
    }
  };

  KeyTest keys[] = {{SDLK_w}, {SDLK_UP},   {SDLK_s}, {SDLK_DOWN},
                    {SDLK_a}, {SDLK_LEFT}, {SDLK_d}, {SDLK_RIGHT}};

  for (const auto& keyTest : keys) {
    resetEventBus();
    InputSystem inputSystem;

    LocalInputEvent downEvent =
        publishKeyAndUpdate(inputSystem, keyTest.sdlKey, true);
    assert(*keyTest.flagPtr(downEvent) == true);

    resetEventBus();
    InputSystem inputSystem2;
    LocalInputEvent upEvent =
        publishKeyAndUpdate(inputSystem2, keyTest.sdlKey, false);
    assert(*keyTest.flagPtr(upEvent) == false);
  }
}

TEST(InputSystem_AllDirections) {
  resetEventBus();
  InputSystem inputSystem;

  EventBus::instance().publish(KeyDownEvent{SDLK_w});
  EventBus::instance().publish(KeyDownEvent{SDLK_a});
  EventBus::instance().publish(KeyDownEvent{SDLK_s});
  EventBus::instance().publish(KeyDownEvent{SDLK_d});

  LocalInputEvent capturedEvent;
  EventBus::instance().subscribe<LocalInputEvent>(
      [&](const LocalInputEvent& e) { capturedEvent = e; });

  EventBus::instance().publish(UpdateEvent{16.67f, 1});

  assert(capturedEvent.moveUp && capturedEvent.moveDown &&
         capturedEvent.moveLeft && capturedEvent.moveRight);
}

TEST(InputSystem_SequenceIncrement) {
  resetEventBus();
  InputSystem inputSystem;

  uint32_t sequences[3] = {0};
  int count = 0;

  EventBus::instance().subscribe<LocalInputEvent>(
      [&](const LocalInputEvent& e) {
        if (count < 3) sequences[count++] = e.inputSequence;
      });

  for (int i = 0; i < 3; i++) {
    EventBus::instance().publish(UpdateEvent{16.67f, static_cast<uint32_t>(i)});
  }

  assert(sequences[1] == sequences[0] + 1);
  assert(sequences[2] == sequences[1] + 1);
}

int main() {
  Logger::init();

  test_InputSystem_KeyBindings();
  test_InputSystem_AllDirections();
  test_InputSystem_SequenceIncrement();

  return 0;
}
