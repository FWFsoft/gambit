#pragma once

#include <functional>

#include "CollisionDebugRenderer.h"
#include "EventBus.h"
#include "GameState.h"
#include "MusicZoneDebugRenderer.h"
#include "ObjectiveDebugRenderer.h"

struct Camera;
class ClientPrediction;

class InputSystem {
 public:
  InputSystem(ClientPrediction* clientPrediction,
              CollisionDebugRenderer* collisionDebugRenderer = nullptr,
              MusicZoneDebugRenderer* musicZoneDebugRenderer = nullptr,
              ObjectiveDebugRenderer* objectiveDebugRenderer = nullptr);

  // The camera this input drives: Y locks / unlocks it, the cursor pans it
  // from the screen edges while unlocked, and Space then recentres it
  // instead of attacking. Optional (tests and the headless client have none).
  void setCamera(Camera* camera) { this->camera = camera; }

  // Esc always closes whatever is on top of the screen first. Overlays this
  // system can't see (the settings panel, the suit screen) are closed by
  // this callback: it closes the top-most one open in `state` and returns
  // true, or returns false when none is. Only then does Esc fall through to
  // leaving the inventory, resuming or pausing. Optional (tests, headless).
  using OverlayCloser = std::function<bool(GameState state)>;
  void setOverlayCloser(OverlayCloser closer) {
    overlayCloser = std::move(closer);
  }

 private:
  bool moveLeft;
  bool moveRight;
  bool moveUp;
  bool moveDown;
  bool interactHeld;    // F key held for objective interaction
  uint8_t buttons = 0;  // InputButtons bitmask: LMB, RMB, Q, LSHIFT, E
  int mouseX = 0;
  int mouseY = 0;
  uint32_t inputSequence;
  ClientPrediction* clientPrediction;
  CollisionDebugRenderer* collisionDebugRenderer;
  MusicZoneDebugRenderer* musicZoneDebugRenderer;
  ObjectiveDebugRenderer* objectiveDebugRenderer;
  Camera* camera = nullptr;
  OverlayCloser overlayCloser;

  void onKeyDown(const KeyDownEvent& e);
  void onKeyUp(const KeyUpEvent& e);
  void onMouseMove(const MouseMoveEvent& e);
  void onMouseButton(const MouseButtonEvent& e);
  void onUpdate(const UpdateEvent& e);

  // --- Game controller -------------------------------------------------
  //
  //   Left stick / D-pad   move (8-way)
  //   Right stick          aim (aim follows movement while it is at rest)
  //   RT  LT               Primary, Secondary        (LMB, RMB)
  //   RB  LB               Special, Core             (Q, E)
  //   A                    Mobility                  (SHIFT)
  //   X                    Interact                  (F)
  //   Start                Pause                     (ESC)
  //   Back                 Suit infusions            (B)
  //   Y (hold)             Scoreboard                (TAB)
  //   Right stick click    Inventory                 (I)
  //
  // In menus the controller navigates instead (D-pad / stick, A, B).
  uint32_t padHeld = 0;   // PadBit mask of buttons / triggers held now
  uint32_t padStale = 0;  // held since a menu: ignored until released
  float padLeftX = 0.0f, padLeftY = 0.0f;
  float padRightX = 0.0f, padRightY = 0.0f;
  int padMenuX = 0, padMenuY = 0;  // left stick as a D-pad, for menus
  float padAimX = 0.0f;            // aim offset from the hero, world px
  float padAimY = 0.0f;

  void onControllerButton(const ControllerButtonEvent& e);
  void onControllerAxis(const ControllerAxisEvent& e);
  void padPress(uint32_t bit);
  void padMenuKey(SDL_Keycode key);
  bool padGameplayActive() const;
  void padMoveDirection(float& x, float& y) const;
};
