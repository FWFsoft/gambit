#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

#include "Animatable.h"
#include "AnimationController.h"
#include "CollisionSystem.h"
#include "MovementInput.h"
#include "config/PlayerConfig.h"

struct Player : public Animatable {
  uint32_t id;
  float x, y;
  float vx, vy;
  float health;
  uint8_t r, g, b;

  // Animation (shared_ptr allows Player to be copyable for remote
  // interpolation)
  std::shared_ptr<AnimationController> animationController;

  // Server-only fields
  uint32_t lastInputSequence;
  float deathTime;  // Server tick when player died (0 = alive)

  // Client-only fields
  uint32_t lastServerTick;

  Player()
      : id(0),
        x(0),
        y(0),
        vx(0),
        vy(0),
        health(Config::Player::MAX_HEALTH),
        r(255),
        g(255),
        b(255),
        animationController(std::make_shared<AnimationController>()),
        lastInputSequence(0),
        deathTime(0.0f),
        lastServerTick(0) {}

  // Helper methods
  bool isDead() const { return health <= 0.0f; }
  bool isAlive() const { return health > 0.0f; }

  // Animatable interface implementation
  AnimationController* getAnimationController() override {
    return animationController.get();
  }

  const AnimationController* getAnimationController() const override {
    return animationController.get();
  }

  float getVelocityX() const override { return vx; }

  float getVelocityY() const override { return vy; }
};

inline void applyInput(Player& player, const MovementInput& input) {
  float dx = 0, dy = 0;

  if (input.moveUp) {
    dx -= 1;
    dy -= 1;
  }
  if (input.moveRight) {
    dx += 1;
    dy -= 1;
  }
  if (input.moveDown) {
    dx += 1;
    dy += 1;
  }
  if (input.moveLeft) {
    dx -= 1;
    dy += 1;
  }

  float len = std::sqrt(dx * dx + dy * dy);
  if (len > 0) {
    dx /= len;
    dy /= len;
  }

  player.vx = dx * Config::Player::SPEED;
  player.vy = dy * Config::Player::SPEED;

  // Update animation state based on new velocity
  if (player.animationController) {
    player.animationController->updateAnimationState(player.vx, player.vy);
  }

  // Calculate new position
  float oldX = player.x;
  float oldY = player.y;
  float newX = player.x + player.vx * input.deltaTime / 1000.0f;
  float newY = player.y + player.vy * input.deltaTime / 1000.0f;

  // Check collision if system is provided
  if (input.collisionSystem != nullptr) {
    input.collisionSystem->checkMovement(oldX, oldY, newX, newY,
                                         Config::Player::RADIUS);
  }

  player.x = newX;
  player.y = newY;

  // Clamp to world bounds
  player.x = std::clamp(player.x, 0.0f, input.worldWidth);
  player.y = std::clamp(player.y, 0.0f, input.worldHeight);
}
