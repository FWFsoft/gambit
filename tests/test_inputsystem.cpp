#include <SDL2/SDL.h>

#include <vector>

#include "Camera.h"
#include "GameStateManager.h"
#include "InputSystem.h"
#include "Logger.h"
#include "PadState.h"
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

// --- Game controller ---------------------------------------------------

namespace {

// The last input the system published. File scope, so the one handler
// padReset subscribes never outlives what it writes to.
LocalInputEvent padLastInput{};
bool padGotInput = false;

// One update tick; returns the input it published (padAim / buttons / moves).
LocalInputEvent padTick() {
  padGotInput = false;
  EventBus::instance().publish(UpdateEvent{16.67f, 1});
  assert(padGotInput);
  return padLastInput;
}

void padReset() {
  resetEventBus();
  EventBus::instance().subscribe<LocalInputEvent>(
      [](const LocalInputEvent& e) {
        padLastInput = e;
        padGotInput = true;
      });
  PadState::holding = false;
  PadState::scoreboardHeld = false;
  PadState::bound = true;
  PadState::active = false;
  PadState::overlayOpen = false;
  PadState::toggleSuitRequested = false;
  GameStateManager::instance().transitionTo(GameState::Playing);
}

void padButton(uint8_t button, bool down) {
  EventBus::instance().publish(ControllerButtonEvent{button, down});
}

void padAxis(uint8_t axis, float value) {
  EventBus::instance().publish(ControllerAxisEvent{axis, value});
}

}  // namespace

// Every ability has a button, and releasing it lets go.
TEST(InputSystem_PadAbilityButtons) {
  padReset();
  InputSystem inputSystem(nullptr);

  padAxis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 1.0f);
  assert(padTick().buttons == InputButtons::Primary);
  padAxis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 0.0f);
  assert(padTick().buttons == 0);

  padAxis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 0.9f);
  assert(padTick().buttons == InputButtons::Secondary);
  padAxis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 0.1f);
  assert(padTick().buttons == 0);

  padButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, true);
  assert(padTick().buttons == InputButtons::Special);
  padButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, false);

  padButton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, true);
  assert(padTick().buttons == InputButtons::Core);
  padButton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, false);

  padButton(SDL_CONTROLLER_BUTTON_A, true);
  assert(padTick().buttons == InputButtons::Mobility);
  padButton(SDL_CONTROLLER_BUTTON_A, false);
  assert(padTick().buttons == 0);

  // X is interact.
  int interacts = 0;
  EventBus::instance().subscribe<InteractInputEvent>(
      [&](const InteractInputEvent&) { interacts++; });
  padButton(SDL_CONTROLLER_BUTTON_X, true);
  assert(interacts == 1);
}

// The left stick and the D-pad both walk, snapped to eight directions.
TEST(InputSystem_PadMovement) {
  padReset();
  InputSystem inputSystem(nullptr);

  // Inside the deadzone: standing still.
  padAxis(SDL_CONTROLLER_AXIS_LEFTX, 0.2f);
  LocalInputEvent e = padTick();
  assert(!e.moveLeft && !e.moveRight && !e.moveUp && !e.moveDown);

  // Mostly right with a little drift down is still just "right".
  padAxis(SDL_CONTROLLER_AXIS_LEFTX, 0.95f);
  padAxis(SDL_CONTROLLER_AXIS_LEFTY, 0.15f);
  e = padTick();
  assert(e.moveRight && !e.moveLeft && !e.moveUp && !e.moveDown);

  // Up and left together is the diagonal.
  padAxis(SDL_CONTROLLER_AXIS_LEFTX, -0.7f);
  padAxis(SDL_CONTROLLER_AXIS_LEFTY, -0.7f);
  e = padTick();
  assert(e.moveLeft && e.moveUp && !e.moveRight && !e.moveDown);

  padAxis(SDL_CONTROLLER_AXIS_LEFTX, 0.0f);
  padAxis(SDL_CONTROLLER_AXIS_LEFTY, 0.0f);
  padButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN, true);
  e = padTick();
  assert(e.moveDown && !e.moveUp && !e.moveLeft && !e.moveRight);
  padButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN, false);
  e = padTick();
  assert(!e.moveDown);
}

// Aim: right stick first, else the walking direction, else the last aim.
// The mouse takes the aim back when it is clicked.
TEST(InputSystem_PadAim) {
  padReset();
  InputSystem inputSystem(nullptr);

  // Nothing touched yet: mouse aim.
  assert(!padTick().padAim);

  // Walking right aims right.
  padAxis(SDL_CONTROLLER_AXIS_LEFTX, 1.0f);
  LocalInputEvent e = padTick();
  assert(e.padAim && e.padAimX > 100.0f && e.padAimY == 0.0f);

  // The right stick overrides it: aim up while walking right.
  padAxis(SDL_CONTROLLER_AXIS_RIGHTY, -1.0f);
  e = padTick();
  assert(e.padAim && e.padAimY < -100.0f && e.padAimX == 0.0f);
  const float fullTilt = -e.padAimY;

  // Less tilt reaches less far.
  padAxis(SDL_CONTROLLER_AXIS_RIGHTY, -0.5f);
  e = padTick();
  assert(e.padAimY < 0.0f && -e.padAimY < fullTilt);

  // Sticks at rest: the last aim stays.
  padAxis(SDL_CONTROLLER_AXIS_RIGHTY, 0.0f);
  padAxis(SDL_CONTROLLER_AXIS_LEFTX, 0.0f);
  const float lastY = e.padAimY;
  e = padTick();
  assert(e.padAim && e.padAimY == lastY);

  // A mouse click hands the aim back to the cursor.
  EventBus::instance().publish(
      MouseButtonEvent{SDL_BUTTON_LEFT, true, 100, 100});
  e = padTick();
  assert(!e.padAim);
  EventBus::instance().publish(
      MouseButtonEvent{SDL_BUTTON_LEFT, false, 100, 100});

  // So does playing on the keyboard, after the stick took it again.
  padAxis(SDL_CONTROLLER_AXIS_LEFTX, 1.0f);
  assert(padTick().padAim);
  padAxis(SDL_CONTROLLER_AXIS_LEFTX, 0.0f);
  EventBus::instance().publish(KeyDownEvent{SDLK_w});
  assert(!padTick().padAim);
}

// A trigger held through the pause menu does not fire on resume, and an
// unplugged controller lets go of everything.
TEST(InputSystem_PadHeldThroughPauseAndDisconnect) {
  padReset();
  GameStateManager& gsm = GameStateManager::instance();
  InputSystem inputSystem(nullptr);

  padAxis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 1.0f);
  assert(padTick().buttons == InputButtons::Primary);
  assert(PadState::holding);

  padButton(SDL_CONTROLLER_BUTTON_START, true);
  padButton(SDL_CONTROLLER_BUTTON_START, false);
  assert(gsm.getCurrentState() == GameState::Paused);
  EventBus::instance().publish(UpdateEvent{16.67f, 1});  // paused tick
  padButton(SDL_CONTROLLER_BUTTON_START, true);
  padButton(SDL_CONTROLLER_BUTTON_START, false);
  assert(gsm.getCurrentState() == GameState::Playing);
  assert(padTick().buttons == 0);  // still held, but stale
  padAxis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 0.0f);
  padAxis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 1.0f);
  assert(padTick().buttons == InputButtons::Primary);

  // Walking and firing when the controller dies.
  padAxis(SDL_CONTROLLER_AXIS_LEFTX, 1.0f);
  padButton(SDL_CONTROLLER_BUTTON_Y, true);
  LocalInputEvent e = padTick();
  assert(e.moveRight && e.buttons == InputButtons::Primary);
  assert(PadState::scoreboardHeld);
  EventBus::instance().publish(ControllerDisconnectedEvent{});
  e = padTick();
  assert(!e.moveRight && e.buttons == 0);
  assert(!PadState::holding && !PadState::scoreboardHeld);
}

// In menus the controller is the menu's: it presses keys, never abilities,
// and a button held through a menu does nothing until it is pressed again.
TEST(InputSystem_PadMenus) {
  padReset();
  GameStateManager& gsm = GameStateManager::instance();
  InputSystem inputSystem(nullptr);

  std::vector<SDL_Keycode> keys;
  EventBus::instance().subscribe<KeyDownEvent>(
      [&](const KeyDownEvent& e) { keys.push_back(e.key); });

  // Title: any button starts.
  gsm.transitionTo(GameState::TitleScreen);
  padButton(SDL_CONTROLLER_BUTTON_X, true);
  padButton(SDL_CONTROLLER_BUTTON_X, false);
  assert(gsm.getCurrentState() == GameState::CharacterSelect);

  // Character select: D-pad and stick flicks move, A confirms.
  keys.clear();
  padButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, true);
  padButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, false);
  padAxis(SDL_CONTROLLER_AXIS_LEFTY, 0.9f);
  padAxis(SDL_CONTROLLER_AXIS_LEFTY, 0.95f);  // still held: no repeat
  padAxis(SDL_CONTROLLER_AXIS_LEFTY, 0.0f);
  padButton(SDL_CONTROLLER_BUTTON_A, true);
  assert((keys == std::vector<SDL_Keycode>{SDLK_RIGHT, SDLK_DOWN,
                                           SDLK_RETURN}));

  // A is still held as the game starts: it must not dash.
  gsm.transitionTo(GameState::Playing);
  assert(padTick().buttons == 0);
  padButton(SDL_CONTROLLER_BUTTON_A, false);
  padButton(SDL_CONTROLLER_BUTTON_A, true);
  assert(padTick().buttons == InputButtons::Mobility);
  padButton(SDL_CONTROLLER_BUTTON_A, false);

  // Start pauses and resumes.
  padButton(SDL_CONTROLLER_BUTTON_START, true);
  padButton(SDL_CONTROLLER_BUTTON_START, false);
  assert(gsm.getCurrentState() == GameState::Paused);
  padButton(SDL_CONTROLLER_BUTTON_START, true);
  padButton(SDL_CONTROLLER_BUTTON_START, false);
  assert(gsm.getCurrentState() == GameState::Playing);

  // Clicking the right stick opens the inventory, and closes it again.
  padButton(SDL_CONTROLLER_BUTTON_RIGHTSTICK, true);
  padButton(SDL_CONTROLLER_BUTTON_RIGHTSTICK, false);
  assert(gsm.getCurrentState() == GameState::Inventory);
  padButton(SDL_CONTROLLER_BUTTON_RIGHTSTICK, true);
  padButton(SDL_CONTROLLER_BUTTON_RIGHTSTICK, false);
  assert(gsm.getCurrentState() == GameState::Playing);

  // Back opens the suit screen; while it is open the hero stands still.
  padButton(SDL_CONTROLLER_BUTTON_BACK, true);
  padButton(SDL_CONTROLLER_BUTTON_BACK, false);
  assert(PadState::toggleSuitRequested);
  PadState::toggleSuitRequested = false;
  PadState::overlayOpen = true;
  padButton(SDL_CONTROLLER_BUTTON_A, true);
  padAxis(SDL_CONTROLLER_AXIS_LEFTX, 1.0f);
  LocalInputEvent e = padTick();
  assert(e.buttons == 0 && !e.moveRight);
  padButton(SDL_CONTROLLER_BUTTON_B, true);  // B closes it
  assert(PadState::toggleSuitRequested);

  PadState::overlayOpen = false;
  PadState::toggleSuitRequested = false;
}

int main() {
  Logger::init();

  test_InputSystem_KeyBindings();
  test_InputSystem_AllDirections();
  test_InputSystem_SequenceIncrement();
  test_InputSystem_SpaceFollowsCameraMode();
  test_InputSystem_EscapeClosesTopOverlay();
  test_InputSystem_PadAbilityButtons();
  test_InputSystem_PadMovement();
  test_InputSystem_PadAim();
  test_InputSystem_PadMenus();
  test_InputSystem_PadHeldThroughPauseAndDisconnect();

  return 0;
}
