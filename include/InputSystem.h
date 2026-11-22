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
};
