#pragma once

#include "CollisionDebugRenderer.h"
#include "EventBus.h"

class InputSystem {
 public:
  InputSystem(CollisionDebugRenderer* debugRenderer = nullptr);

 private:
  bool moveLeft;
  bool moveRight;
  bool moveUp;
  bool moveDown;
  uint32_t inputSequence;
  CollisionDebugRenderer* debugRenderer;

  void onKeyDown(const KeyDownEvent& e);
  void onKeyUp(const KeyUpEvent& e);
  void onUpdate(const UpdateEvent& e);
};
