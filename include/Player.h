#pragma once

#include <cstdint>
#include <algorithm>

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
        : id(0), x(0), y(0), vx(0), vy(0), health(100.0f),
          r(255), g(255), b(255),
          lastInputSequence(0), lastServerTick(0) {
    }
};

// Movement constants
constexpr float PLAYER_SPEED = 200.0f;  // pixels per second
constexpr float WORLD_WIDTH = 800.0f;
constexpr float WORLD_HEIGHT = 600.0f;

// Apply input to player (shared by client prediction and server authority)
inline void applyInput(Player& player, bool moveLeft, bool moveRight, bool moveUp, bool moveDown, float deltaTime) {
    float dx = 0, dy = 0;

    if (moveLeft)  dx -= 1;
    if (moveRight) dx += 1;
    if (moveUp)    dy -= 1;
    if (moveDown)  dy += 1;

    // Normalize diagonal movement
    if (dx != 0 && dy != 0) {
        dx *= 0.707f;
        dy *= 0.707f;
    }

    player.vx = dx * PLAYER_SPEED;
    player.vy = dy * PLAYER_SPEED;
    player.x += player.vx * deltaTime / 1000.0f;
    player.y += player.vy * deltaTime / 1000.0f;

    // Clamp to world bounds
    player.x = std::clamp(player.x, 0.0f, WORLD_WIDTH);
    player.y = std::clamp(player.y, 0.0f, WORLD_HEIGHT);
}
