#include "InputSystem.h"

#include "Camera.h"
#include "ClientPrediction.h"
#include "GameStateManager.h"
#include "Logger.h"
#include "NetworkProtocol.h"
#include "PadState.h"
#include "Player.h"

#include <algorithm>
#include <cmath>

namespace {

// Controller buttons and triggers as one held-state mask.
namespace PadBit {
constexpr uint32_t A = 1u << 0;
constexpr uint32_t B = 1u << 1;
constexpr uint32_t X = 1u << 2;
constexpr uint32_t Y = 1u << 3;
constexpr uint32_t LB = 1u << 4;
constexpr uint32_t RB = 1u << 5;
constexpr uint32_t LT = 1u << 6;
constexpr uint32_t RT = 1u << 7;
constexpr uint32_t Start = 1u << 8;
constexpr uint32_t Back = 1u << 9;
constexpr uint32_t Up = 1u << 10;
constexpr uint32_t Down = 1u << 11;
constexpr uint32_t Left = 1u << 12;
constexpr uint32_t Right = 1u << 13;
constexpr uint32_t R3 = 1u << 14;  // right stick click
}  // namespace PadBit

constexpr float PAD_MOVE_DEADZONE = 0.40f;   // left stick, to start moving
constexpr float PAD_AIM_DEADZONE = 0.30f;    // right stick, to take the aim
constexpr float PAD_TRIGGER_PRESSED = 0.50f; // trigger travel that counts
constexpr float PAD_MENU_FLICK = 0.60f;      // left stick as a menu D-pad
// How far from the hero the aim point sits (world pixels): the right stick
// reaches from MIN at the deadzone to MAX at full tilt; aiming by moving
// uses the middle.
constexpr float PAD_AIM_MIN = 110.0f;
constexpr float PAD_AIM_MAX = 320.0f;
constexpr float PAD_AIM_MOVING = 200.0f;
// sin(22.5 deg): splits the stick circle into eight equal move sectors.
constexpr float PAD_SECTOR = 0.3827f;

uint32_t padBitForButton(uint8_t button) {
  switch (button) {
    case SDL_CONTROLLER_BUTTON_A:
      return PadBit::A;
    case SDL_CONTROLLER_BUTTON_B:
      return PadBit::B;
    case SDL_CONTROLLER_BUTTON_X:
      return PadBit::X;
    case SDL_CONTROLLER_BUTTON_Y:
      return PadBit::Y;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
      return PadBit::LB;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
      return PadBit::RB;
    case SDL_CONTROLLER_BUTTON_START:
      return PadBit::Start;
    case SDL_CONTROLLER_BUTTON_BACK:
      return PadBit::Back;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      return PadBit::Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      return PadBit::Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      return PadBit::Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      return PadBit::Right;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
      return PadBit::R3;
    default:
      return 0;
  }
}

}  // namespace

InputSystem::InputSystem(ClientPrediction* clientPrediction,
                         CollisionDebugRenderer* collisionDebugRenderer,
                         MusicZoneDebugRenderer* musicZoneDebugRenderer,
                         ObjectiveDebugRenderer* objectiveDebugRenderer)
    : moveLeft(false),
      moveRight(false),
      moveUp(false),
      moveDown(false),
      interactHeld(false),
      inputSequence(0),
      clientPrediction(clientPrediction),
      collisionDebugRenderer(collisionDebugRenderer),
      musicZoneDebugRenderer(musicZoneDebugRenderer),
      objectiveDebugRenderer(objectiveDebugRenderer) {
  // Subscribe to key events
  EventBus::instance().subscribe<KeyDownEvent>(
      [this](const KeyDownEvent& e) { onKeyDown(e); });

  EventBus::instance().subscribe<KeyUpEvent>(
      [this](const KeyUpEvent& e) { onKeyUp(e); });

  EventBus::instance().subscribe<MouseMoveEvent>(
      [this](const MouseMoveEvent& e) { onMouseMove(e); });
  EventBus::instance().subscribe<MouseButtonEvent>(
      [this](const MouseButtonEvent& e) { onMouseButton(e); });

  EventBus::instance().subscribe<ControllerButtonEvent>(
      [this](const ControllerButtonEvent& e) { onControllerButton(e); });
  EventBus::instance().subscribe<ControllerAxisEvent>(
      [this](const ControllerAxisEvent& e) { onControllerAxis(e); });
  EventBus::instance().subscribe<ControllerDisconnectedEvent>(
      [this](const ControllerDisconnectedEvent&) {
        // Unplugged mid-press: nothing stays held.
        padHeld = 0;
        padStale = 0;
        padLeftX = padLeftY = padRightX = padRightY = 0.0f;
        padMenuX = padMenuY = 0;
        PadState::holding = false;
        PadState::scoreboardHeld = false;
      });

  // Subscribe to update events to publish input state
  EventBus::instance().subscribe<UpdateEvent>(
      [this](const UpdateEvent& e) { onUpdate(e); });
}

void InputSystem::onKeyDown(const KeyDownEvent& e) {
  GameStateManager& gsm = GameStateManager::instance();
  GameState currentState = gsm.getCurrentState();

  // Esc closes the top-most overlay before it means anything else.
  if (e.key == SDLK_ESCAPE && overlayCloser && overlayCloser(currentState)) {
    return;
  }

  // TitleScreen: any key advances to character select
  if (currentState == GameState::TitleScreen) {
    gsm.transitionTo(GameState::CharacterSelect);
    return;
  }

  // CharacterSelect: ESC returns to title screen
  if (currentState == GameState::CharacterSelect) {
    if (e.key == SDLK_ESCAPE) {
      gsm.transitionTo(GameState::TitleScreen);
    }
    return;  // All other input handled by ImGui clicks
  }

  // ESC: Toggle pause (Playing <-> Paused); also closes the inventory
  if (e.key == SDLK_ESCAPE) {
    if (currentState == GameState::Playing) {
      // Clear movement state and reset animation to idle when pausing
      moveLeft = moveRight = moveUp = moveDown = false;
      buttons = 0;
      if (camera) camera->recentreHeld = false;
      if (clientPrediction) {
        Player& localPlayer = clientPrediction->getLocalPlayerMutable();
        localPlayer.getAnimationController()->updateAnimationState(0.0f, 0.0f);
      }
      gsm.transitionTo(GameState::Paused);
    } else if (currentState == GameState::Paused ||
               currentState == GameState::Inventory) {
      gsm.transitionTo(GameState::Playing);
    }
    return;
  }

  // I: Toggle inventory (Playing/Paused <-> Inventory)
  if (e.key == SDLK_i) {
    if (currentState == GameState::Inventory) {
      gsm.transitionTo(GameState::Playing);
    } else if (currentState == GameState::Playing ||
               currentState == GameState::Paused) {
      // Clear movement state and reset animation to idle when opening inventory
      moveLeft = moveRight = moveUp = moveDown = false;
      buttons = 0;
      if (camera) camera->recentreHeld = false;
      if (clientPrediction) {
        Player& localPlayer = clientPrediction->getLocalPlayerMutable();
        localPlayer.getAnimationController()->updateAnimationState(0.0f, 0.0f);
      }
      gsm.transitionTo(GameState::Inventory);
    }
    return;
  }

  // Toggle all debug renderers with F1
  if (e.key == SDLK_F1) {
    if (collisionDebugRenderer) {
      collisionDebugRenderer->toggle();
      Logger::info(
          "Collision debug: " +
          std::string(collisionDebugRenderer->isEnabled() ? "ON" : "OFF"));
    }
    if (musicZoneDebugRenderer) {
      musicZoneDebugRenderer->toggle();
      Logger::info(
          "Music zone debug: " +
          std::string(musicZoneDebugRenderer->isEnabled() ? "ON" : "OFF"));
    }
    if (objectiveDebugRenderer) {
      objectiveDebugRenderer->toggle();
      Logger::info(
          "Objective debug: " +
          std::string(objectiveDebugRenderer->isEnabled() ? "ON" : "OFF"));
    }
  }

  // Toggle mute with M or F2
  if (e.key == SDLK_m || e.key == SDLK_F2) {
    EventBus::instance().publish(ToggleMuteEvent{});
  }

  // Only process game input during Playing state
  if (currentState != GameState::Playing) {
    return;
  }

  // Playing on the keyboard hands the aim back to the mouse.
  PadState::active = false;

  // Y: lock / unlock the camera.
  if (e.key == SDLK_y && camera) {
    camera->toggleMode();
    buttons &= ~InputButtons::Primary;  // Space may have been attacking
    Logger::info(std::string("Camera: ") +
                 (camera->isLocked() ? "locked" : "unlocked"));
  }

  // Abilities: LMB / RMB come from the mouse; these are the keyboard ones.
  if (e.key == SDLK_q) buttons |= InputButtons::Special;
  if (e.key == SDLK_LSHIFT) buttons |= InputButtons::Mobility;
  if (e.key == SDLK_e) buttons |= InputButtons::Core;
  // Space: with the camera locked it doubles as the basic attack (for
  // trackpads); with the camera unlocked it brings the camera back to your
  // hero for as long as it is held (Controls/HUD doc).
  if (e.key == SDLK_SPACE) {
    if (camera && !camera->isLocked()) {
      camera->recentreHeld = true;
    } else {
      buttons |= InputButtons::Primary;
    }
  }

  // Interact input (F key) - for objectives
  if (e.key == SDLK_f) {
    if (!interactHeld) {
      interactHeld = true;
      EventBus::instance().publish(InteractInputEvent{});
    }
  }

  if (e.key == SDLK_a || e.key == SDLK_LEFT) moveLeft = true;
  if (e.key == SDLK_d || e.key == SDLK_RIGHT) moveRight = true;
  if (e.key == SDLK_w || e.key == SDLK_UP) moveUp = true;
  if (e.key == SDLK_s || e.key == SDLK_DOWN) moveDown = true;
}

void InputSystem::onKeyUp(const KeyUpEvent& e) {
  // Only process game input during Playing state
  if (GameStateManager::instance().getCurrentState() != GameState::Playing) {
    return;
  }

  if (e.key == SDLK_a || e.key == SDLK_LEFT) moveLeft = false;
  if (e.key == SDLK_d || e.key == SDLK_RIGHT) moveRight = false;
  if (e.key == SDLK_w || e.key == SDLK_UP) moveUp = false;
  if (e.key == SDLK_s || e.key == SDLK_DOWN) moveDown = false;
  if (e.key == SDLK_f) interactHeld = false;
  if (e.key == SDLK_q) buttons &= ~InputButtons::Special;
  if (e.key == SDLK_LSHIFT) buttons &= ~InputButtons::Mobility;
  if (e.key == SDLK_e) buttons &= ~InputButtons::Core;
  if (e.key == SDLK_SPACE) {
    buttons &= ~InputButtons::Primary;
    if (camera) camera->recentreHeld = false;
  }
}

void InputSystem::onMouseMove(const MouseMoveEvent& e) {
  mouseX = e.x;
  mouseY = e.y;
}

void InputSystem::onMouseButton(const MouseButtonEvent& e) {
  mouseX = e.x;
  mouseY = e.y;
  uint8_t bit = 0;
  if (e.button == SDL_BUTTON_LEFT) bit = InputButtons::Primary;
  if (e.button == SDL_BUTTON_RIGHT) bit = InputButtons::Secondary;
  if (bit == 0) return;
  if (!e.down) {
    buttons &= ~bit;
    return;
  }
  // Only start abilities while actually playing (not in menus).
  if (GameStateManager::instance().getCurrentState() == GameState::Playing) {
    buttons |= bit;
    PadState::active = false;  // clicking hands the aim back to the mouse
  }
}

// ---------------------------------------------------------------------------
// Game controller
// ---------------------------------------------------------------------------

// The controller drives the hero only while the game is being played and no
// panel is open over it; otherwise it drives the menu.
bool InputSystem::padGameplayActive() const {
  return GameStateManager::instance().getCurrentState() == GameState::Playing &&
         !PadState::overlayOpen;
}

// A menu key press on behalf of the controller. Goes through the event bus
// so every keyboard listener (this system, the UI) reacts as if it was typed.
void InputSystem::padMenuKey(SDL_Keycode key) {
  EventBus::instance().publish(KeyDownEvent{key});
}

void InputSystem::onControllerButton(const ControllerButtonEvent& e) {
  const uint32_t bit = padBitForButton(e.button);
  if (bit == 0) return;
  if (!e.down) {
    // Always take releases so a held button can never get stuck.
    padHeld &= ~bit;
    padStale &= ~bit;
    return;
  }
  padHeld |= bit;
  padPress(bit);
}

void InputSystem::onControllerAxis(const ControllerAxisEvent& e) {
  auto trigger = [this](uint32_t bit, float value) {
    const bool wasDown = (padHeld & bit) != 0;
    const bool down = value > PAD_TRIGGER_PRESSED;
    if (down == wasDown) return;
    if (down) {
      padHeld |= bit;
      padPress(bit);
    } else {
      padHeld &= ~bit;
      padStale &= ~bit;
    }
  };

  switch (e.axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
      padLeftX = e.value;
      break;
    case SDL_CONTROLLER_AXIS_LEFTY:
      padLeftY = e.value;
      break;
    case SDL_CONTROLLER_AXIS_RIGHTX:
      padRightX = e.value;
      break;
    case SDL_CONTROLLER_AXIS_RIGHTY:
      padRightY = e.value;
      break;
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
      trigger(PadBit::LT, e.value);
      return;
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
      trigger(PadBit::RT, e.value);
      return;
    default:
      return;
  }

  // In menus a flick of the left stick is a D-pad press.
  const int menuX =
      padLeftX > PAD_MENU_FLICK ? 1 : (padLeftX < -PAD_MENU_FLICK ? -1 : 0);
  const int menuY =
      padLeftY > PAD_MENU_FLICK ? 1 : (padLeftY < -PAD_MENU_FLICK ? -1 : 0);
  const GameState state = GameStateManager::instance().getCurrentState();
  if (state == GameState::CharacterSelect) {
    if (menuX != padMenuX && menuX != 0) {
      padMenuKey(menuX > 0 ? SDLK_RIGHT : SDLK_LEFT);
    }
    if (menuY != padMenuY && menuY != 0) {
      padMenuKey(menuY > 0 ? SDLK_DOWN : SDLK_UP);
    }
  }
  padMenuX = menuX;
  padMenuY = menuY;

  // Pushing a stick while playing makes the controller the aiming device.
  if (padGameplayActive() &&
      (std::hypot(padLeftX, padLeftY) > PAD_MOVE_DEADZONE ||
       std::hypot(padRightX, padRightY) > PAD_AIM_DEADZONE)) {
    PadState::active = true;
  }
}

// A controller button (or trigger) went down.
void InputSystem::padPress(uint32_t bit) {
  GameStateManager& gsm = GameStateManager::instance();
  const GameState state = gsm.getCurrentState();

  if (!padGameplayActive()) {
    // Whatever is held when the game resumes must be pressed again: the A
    // that confirmed a menu is not also a dash.
    padStale |= bit;
  }

  switch (state) {
    case GameState::TitleScreen:
      padMenuKey(SDLK_RETURN);  // any button starts
      return;

    case GameState::CharacterSelect:
      if (bit == PadBit::Left) padMenuKey(SDLK_LEFT);
      if (bit == PadBit::Right) padMenuKey(SDLK_RIGHT);
      if (bit == PadBit::Up) padMenuKey(SDLK_UP);
      if (bit == PadBit::Down) padMenuKey(SDLK_DOWN);
      if (bit == PadBit::X) padMenuKey(SDLK_SPACE);  // preview
      if (bit == PadBit::A || bit == PadBit::Start) padMenuKey(SDLK_RETURN);
      if (bit == PadBit::B) padMenuKey(SDLK_ESCAPE);
      return;

    case GameState::Paused:
      // The buttons themselves are ImGui gamepad navigation (UISystem).
      if (bit == PadBit::Start || bit == PadBit::B) padMenuKey(SDLK_ESCAPE);
      return;

    case GameState::Inventory:
      if (bit == PadBit::B || bit == PadBit::Start || bit == PadBit::R3) {
        padMenuKey(SDLK_i);
      }
      return;

    case GameState::MainMenu:
      return;

    case GameState::Playing:
      break;
  }

  if (bit == PadBit::Start) {
    padMenuKey(SDLK_ESCAPE);
    return;
  }
  if (PadState::overlayOpen) {
    // Suit screen / end screen: ImGui navigation has the buttons. Back or B
    // closes the suit screen.
    if (bit == PadBit::Back || bit == PadBit::B) {
      PadState::toggleSuitRequested = true;
    }
    return;
  }

  PadState::active = true;
  if (bit == PadBit::Back) PadState::toggleSuitRequested = true;
  // Inventory is on a stick click, out of the way: it stops your hero, so
  // it must not be easy to hit by accident in a fight.
  if (bit == PadBit::R3) padMenuKey(SDLK_i);
  if (bit == PadBit::X) EventBus::instance().publish(InteractInputEvent{});
}

// Where the controller wants to walk: D-pad first, then the left stick.
// Zero when neither is pushed; otherwise a unit vector.
void InputSystem::padMoveDirection(float& x, float& y) const {
  x = 0.0f;
  y = 0.0f;
  const uint32_t held = padHeld & ~padStale;
  if (held & PadBit::Left) x -= 1.0f;
  if (held & PadBit::Right) x += 1.0f;
  if (held & PadBit::Up) y -= 1.0f;
  if (held & PadBit::Down) y += 1.0f;
  if (x == 0.0f && y == 0.0f &&
      std::hypot(padLeftX, padLeftY) > PAD_MOVE_DEADZONE) {
    x = padLeftX;
    y = padLeftY;
  }
  const float length = std::hypot(x, y);
  if (length > 0.0f) {
    x /= length;
    y /= length;
  }
}

void InputSystem::onUpdate(const UpdateEvent& e) {
  // Buttons held through a menu do nothing until they are released.
  if (!padGameplayActive()) padStale |= padHeld;
  PadState::holding = padHeld != 0 ||
                      std::hypot(padLeftX, padLeftY) > PAD_MOVE_DEADZONE;
  PadState::scoreboardHeld = (padHeld & PadBit::Y) != 0;

  // Only publish input during Playing state
  if (GameStateManager::instance().getCurrentState() != GameState::Playing) {
    return;
  }

  // Unlocked camera: the cursor pans it from the screen edges. Not while
  // the cursor is outside the window, or the view would drift on its own.
  if (camera && SDL_GetMouseFocus() != nullptr) {
    camera->pan(mouseX, mouseY, e.deltaTime / 1000.0f);
  }

  // Publish input state every frame
  LocalInputEvent input;
  input.moveLeft = moveLeft;
  input.moveRight = moveRight;
  input.moveUp = moveUp;
  input.moveDown = moveDown;
  input.inputSequence = inputSequence++;
  input.buttons = buttons;
  input.mouseX = mouseX;
  input.mouseY = mouseY;

  if (padGameplayActive()) {
    const uint32_t held = padHeld & ~padStale;
    if (held & PadBit::RT) input.buttons |= InputButtons::Primary;
    if (held & PadBit::LT) input.buttons |= InputButtons::Secondary;
    if (held & PadBit::RB) input.buttons |= InputButtons::Special;
    if (held & PadBit::LB) input.buttons |= InputButtons::Core;
    if (held & PadBit::A) input.buttons |= InputButtons::Mobility;

    // Movement is 8-way on the wire: snap the stick to the nearest sector.
    float moveX, moveY;
    padMoveDirection(moveX, moveY);
    if (moveX < -PAD_SECTOR) input.moveLeft = true;
    if (moveX > PAD_SECTOR) input.moveRight = true;
    if (moveY < -PAD_SECTOR) input.moveUp = true;
    if (moveY > PAD_SECTOR) input.moveDown = true;

    // Aim: the right stick when it is pushed, otherwise the way you walk,
    // otherwise wherever you aimed last.
    const float aimTilt = std::hypot(padRightX, padRightY);
    if (aimTilt > PAD_AIM_DEADZONE) {
      const float reach = std::min(
          1.0f, (aimTilt - PAD_AIM_DEADZONE) / (1.0f - PAD_AIM_DEADZONE));
      const float distance = PAD_AIM_MIN + (PAD_AIM_MAX - PAD_AIM_MIN) * reach;
      padAimX = padRightX / aimTilt * distance;
      padAimY = padRightY / aimTilt * distance;
    } else if (moveX != 0.0f || moveY != 0.0f) {
      padAimX = moveX * PAD_AIM_MOVING;
      padAimY = moveY * PAD_AIM_MOVING;
    } else if (padAimX == 0.0f && padAimY == 0.0f) {
      padAimY = PAD_AIM_MOVING;  // never aimed yet: face the camera
    }
  }
  if (PadState::active) {
    input.padAim = true;
    input.padAimX = padAimX;
    input.padAimY = padAimY;
  }

  EventBus::instance().publish(input);
}
