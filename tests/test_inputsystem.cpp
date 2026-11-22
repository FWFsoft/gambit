#include <SDL2/SDL.h>

#include "Camera.h"
#include "GameStateManager.h"
#include "InputSystem.h"
#include "Logger.h"
#include "test_utils.h"

TEST(InputSystem_KeyBindings) {
  // Set game state to Playing so InputSystem processes key events
  GameStateManager::instance().transitionTo(GameState::Playing);
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
    InputSystem inputSystem(nullptr, nullptr, nullptr);

    LocalInputEvent downEvent =
        publishKeyAndUpdate(inputSystem, keyTest.sdlKey, true);
    assert(*keyTest.flagPtr(downEvent) == true);

    resetEventBus();
    InputSystem inputSystem2(nullptr, nullptr, nullptr);
    LocalInputEvent upEvent =
        publishKeyAndUpdate(inputSystem2, keyTest.sdlKey, false);
    assert(*keyTest.flagPtr(upEvent) == false);
  }
}

TEST(InputSystem_AllDirections) {
  resetEventBus();
  GameStateManager::instance().transitionTo(GameState::Playing);
  InputSystem inputSystem(nullptr, nullptr, nullptr);

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
  GameStateManager::instance().transitionTo(GameState::Playing);
  InputSystem inputSystem(nullptr, nullptr, nullptr);

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

// Space attacks while the camera is locked; once it is unlocked (Y) Space
// brings the camera back to the hero instead.
TEST(InputSystem_SpaceFollowsCameraMode) {
  resetEventBus();
  GameStateManager::instance().transitionTo(GameState::Playing);

  Camera camera(Config::Screen::WIDTH, Config::Screen::HEIGHT);
  camera.setMode(Camera::Mode::Locked);
  InputSystem inputSystem(nullptr);
  inputSystem.setCamera(&camera);

  LocalInputEvent event = publishKeyAndUpdate(inputSystem, SDLK_SPACE, true);
  assert((event.buttons & InputButtons::Primary) != 0);
  assert(!camera.recentreHeld);
  event = publishKeyAndUpdate(inputSystem, SDLK_SPACE, false);
  assert((event.buttons & InputButtons::Primary) == 0);

  publishKeyAndUpdate(inputSystem, SDLK_y, true);
  assert(!camera.isLocked());
  event = publishKeyAndUpdate(inputSystem, SDLK_SPACE, true);
  assert((event.buttons & InputButtons::Primary) == 0);
  assert(camera.recentreHeld && camera.isFollowing());
  publishKeyAndUpdate(inputSystem, SDLK_SPACE, false);
  assert(!camera.recentreHeld && !camera.isFollowing());

  publishKeyAndUpdate(inputSystem, SDLK_y, true);
  assert(camera.isLocked());
}

// Esc always closes what is on top of the screen: an overlay first, then
// the inventory or the pause menu, and only then does it pause.
TEST(InputSystem_EscapeClosesTopOverlay) {
  resetEventBus();
  GameStateManager& gsm = GameStateManager::instance();
  gsm.transitionTo(GameState::Playing);
  InputSystem inputSystem(nullptr);
  auto escape = [] { EventBus::instance().publish(KeyDownEvent{SDLK_ESCAPE}); };

  // Inventory: I opens it, Esc closes it back to play.
  EventBus::instance().publish(KeyDownEvent{SDLK_i});
  assert(gsm.getCurrentState() == GameState::Inventory);
  escape();
  assert(gsm.getCurrentState() == GameState::Playing);

  // Pause and resume.
  escape();
  assert(gsm.getCurrentState() == GameState::Paused);
  escape();
  assert(gsm.getCurrentState() == GameState::Playing);

  // An open overlay (settings panel, suit screen) eats the first Esc.
  int overlays = 1;
  GameState seen = GameState::TitleScreen;
  inputSystem.setOverlayCloser([&](GameState state) {
    seen = state;
    if (overlays == 0) return false;
    overlays--;
    return true;
  });
  escape();
  assert(seen == GameState::Playing);
  assert(overlays == 0 && gsm.getCurrentState() == GameState::Playing);
  escape();
  assert(gsm.getCurrentState() == GameState::Paused);

  // Settings over the pause menu: Esc closes settings, then resumes.
  overlays = 1;
  escape();
  assert(seen == GameState::Paused);
  assert(gsm.getCurrentState() == GameState::Paused);
  escape();
  assert(gsm.getCurrentState() == GameState::Playing);

  // Settings over the title screen: Esc closes them, it doesn't advance.
  gsm.transitionTo(GameState::TitleScreen);
  overlays = 1;
  escape();
  assert(gsm.getCurrentState() == GameState::TitleScreen);
  gsm.transitionTo(GameState::Playing);
}

int main() {
  Logger::init();

  test_InputSystem_KeyBindings();
  test_InputSystem_AllDirections();
  test_InputSystem_SequenceIncrement();
  test_InputSystem_SpaceFollowsCameraMode();
  test_InputSystem_EscapeClosesTopOverlay();

  return 0;
}
