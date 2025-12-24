#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "CollisionSystem.h"

struct Player {
  uint32_t id;
  float x, y;
  float vx, vy;
  float health;
  uint8_t r, g, b;

  // Server-only fields
  uint32_t lastInputSequence;

  // Client-only fields
  uint32_t lastServerTick;

  Player()
      : id(0),
        x(0),
        y(0),
        vx(0),
        vy(0),
        health(100.0f),
        r(255),
        g(255),
        b(255),
        lastInputSequence(0),
        lastServerTick(0) {}
};

constexpr float PLAYER_SPEED = 200.0f;
constexpr float PLAYER_RADIUS = 16.0f;  // Half of PLAYER_SIZE (32x32)

inline void applyInput(Player& player, bool moveLeft, bool moveRight,
                       bool moveUp, bool moveDown, float deltaTime,
                       float worldWidth, float worldHeight,
                       const CollisionSystem* collisionSystem = nullptr) {
  float dx = 0, dy = 0;

  if (moveUp) {
    dx -= 1;
    dy -= 1;
  }
  if (moveRight) {
    dx += 1;
    dy -= 1;
  }
  if (moveDown) {
    dx += 1;
    dy += 1;
  }
  if (moveLeft) {
    dx -= 1;
    dy += 1;
  }

  float len = std::sqrt(dx * dx + dy * dy);
  if (len > 0) {
    dx /= len;
    dy /= len;
  }

  player.vx = dx * PLAYER_SPEED;
  player.vy = dy * PLAYER_SPEED;

  // Calculate new position
  float oldX = player.x;
  float oldY = player.y;
  float newX = player.x + player.vx * deltaTime / 1000.0f;
  float newY = player.y + player.vy * deltaTime / 1000.0f;

  // Check collision if system is provided
  if (collisionSystem != nullptr) {
    collisionSystem->checkMovement(oldX, oldY, newX, newY, PLAYER_RADIUS);
  }

  player.x = newX;
  player.y = newY;

  // Clamp to world bounds
  player.x = std::clamp(player.x, 0.0f, worldWidth);
  player.y = std::clamp(player.y, 0.0f, worldHeight);
}
