#pragma once

#include "config/ScreenConfig.h"
#include "config/TimingConfig.h"

class CollisionSystem;

struct MovementInput {
  // Input state
  bool moveLeft;
  bool moveRight;
  bool moveUp;
  bool moveDown;

  // Simulation context
  float deltaTime;
  float worldWidth;
  float worldHeight;
  const CollisionSystem* collisionSystem;

  // Constructors for convenience
  MovementInput()
      : moveLeft(false),
        moveRight(false),
        moveUp(false),
        moveDown(false),
        deltaTime(Config::Timing::TARGET_DELTA_MS),
        worldWidth(static_cast<float>(Config::Screen::WIDTH)),
        worldHeight(static_cast<float>(Config::Screen::HEIGHT)),
        collisionSystem(nullptr) {}

  MovementInput(bool left, bool right, bool up, bool down, float dt, float ww,
                float wh, const CollisionSystem* cs = nullptr)
      : moveLeft(left),
        moveRight(right),
        moveUp(up),
        moveDown(down),
        deltaTime(dt),
        worldWidth(ww),
        worldHeight(wh),
        collisionSystem(cs) {}
};
