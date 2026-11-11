#pragma once

#include "CollisionDebugRenderer.h"
#include "EventBus.h"
#include "MusicZoneDebugRenderer.h"
#include "ObjectiveDebugRenderer.h"

class ClientPrediction;

class InputSystem {
 public:
  InputSystem(ClientPrediction* clientPrediction,
              CollisionDebugRenderer* collisionDebugRenderer = nullptr,
              MusicZoneDebugRenderer* musicZoneDebugRenderer = nullptr,
              ObjectiveDebugRenderer* objectiveDebugRenderer = nullptr);

 private:
  bool moveLeft;
  bool moveRight;
  bool moveUp;
  bool moveDown;
  bool interactHeld;  // F key held for objective interaction
  uint8_t buttons = 0;  // InputButtons bitmask: LMB, RMB, Q, LSHIFT, E
  int mouseX = 0;
  int mouseY = 0;
  uint32_t inputSequence;
  ClientPrediction* clientPrediction;
  CollisionDebugRenderer* collisionDebugRenderer;
  MusicZoneDebugRenderer* musicZoneDebugRenderer;
  ObjectiveDebugRenderer* objectiveDebugRenderer;

  void onKeyDown(const KeyDownEvent& e);
  void onKeyUp(const KeyUpEvent& e);
  void onMouseMove(const MouseMoveEvent& e);
  void onMouseButton(const MouseButtonEvent& e);
  void onUpdate(const UpdateEvent& e);
};
