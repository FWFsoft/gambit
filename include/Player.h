#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Logger.h"

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

inline void applyInput(Player& player, bool moveLeft, bool moveRight,
                       bool moveUp, bool moveDown, float deltaTime,
                       float worldWidth, float worldHeight) {
  bool anyInput = moveLeft || moveRight || moveUp || moveDown;

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

  float prevX = player.x;
  float prevY = player.y;

  player.x += player.vx * deltaTime / 1000.0f;
  player.y += player.vy * deltaTime / 1000.0f;

  float preClampX = player.x;
  float preClampY = player.y;

  player.x = std::clamp(player.x, 0.0f, worldWidth);
  player.y = std::clamp(player.y, 0.0f, worldHeight);

  if (anyInput && (prevX != player.x || prevY != player.y)) {
    Logger::info("MOVE - Player " + std::to_string(player.id) + " from (" +
                 std::to_string(prevX) + ", " + std::to_string(prevY) +
                 ") to (" + std::to_string(player.x) + ", " +
                 std::to_string(player.y) + ") - dx/dy: (" +
                 std::to_string(dx) + ", " + std::to_string(dy) + ")");
  }

  if (preClampX != player.x || preClampY != player.y) {
    Logger::info("BOUNDS HIT - Player " + std::to_string(player.id) +
                 " - Position before: (" + std::to_string(preClampX) + ", " +
                 std::to_string(preClampY) + ") after: (" +
                 std::to_string(player.x) + ", " + std::to_string(player.y) +
                 ") - World bounds: (" + std::to_string(worldWidth) + ", " +
                 std::to_string(worldHeight) + ")");
  }
}
