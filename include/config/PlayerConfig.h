#pragma once

#include <cstdint>

// Player configuration
// Game-specific constants for player behavior and appearance

namespace Config {
namespace Player {

// Movement
constexpr float SPEED =
    600.0f;  // Pixels per second (3x boost for faster gameplay)

// Dimensions
constexpr int SIZE = 32;  // Player sprite size (pixels)
constexpr float RADIUS = static_cast<float>(SIZE) / 2.0f;  // Collision radius

// Stats
constexpr float MAX_HEALTH = 1000.0f;  // Increased for testing objectives

// Spawn location (center of screen by default)
constexpr float DEFAULT_SPAWN_X = 400.0f;
constexpr float DEFAULT_SPAWN_Y = 300.0f;

// Animation frame dimensions
constexpr int FRAME_WIDTH = SIZE;
constexpr int FRAME_HEIGHT = SIZE;

// Default color palette for multiplayer (RGBA)
struct Color {
  uint8_t r, g, b;
};

constexpr Color COLORS[] = {
    {255, 0, 0},    // Red (Player 1)
    {0, 255, 0},    // Green (Player 2)
    {0, 0, 255},    // Blue (Player 3)
    {255, 255, 0},  // Yellow (Player 4)
};

constexpr int MAX_PLAYERS = 4;

}  // namespace Player
}  // namespace Config
