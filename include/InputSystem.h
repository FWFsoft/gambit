#pragma once

#include "CollisionDebugRenderer.h"
#include "EventBus.h"
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

  void onKeyDown(const KeyDownEvent& e);
  void onKeyUp(const KeyUpEvent& e);
  void onMouseMove(const MouseMoveEvent& e);
  void onMouseButton(const MouseButtonEvent& e);
  void onUpdate(const UpdateEvent& e);
};
