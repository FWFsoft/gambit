#pragma once

#include "CollisionDebugRenderer.h"
#include "EventBus.h"
#include "MusicZoneDebugRenderer.h"

class ClientPrediction;

class InputSystem {
 public:
  InputSystem(ClientPrediction* clientPrediction,
              CollisionDebugRenderer* collisionDebugRenderer = nullptr,
              MusicZoneDebugRenderer* musicZoneDebugRenderer = nullptr);

 private:
  bool moveLeft;
  bool moveRight;
  bool moveUp;
  bool moveDown;
  uint32_t inputSequence;
  ClientPrediction* clientPrediction;
  CollisionDebugRenderer* collisionDebugRenderer;
  MusicZoneDebugRenderer* musicZoneDebugRenderer;

  void onKeyDown(const KeyDownEvent& e);
  void onKeyUp(const KeyUpEvent& e);
  void onUpdate(const UpdateEvent& e);
};
