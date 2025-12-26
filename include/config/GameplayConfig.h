#pragma once

// Gameplay configuration
// Game logic constants for prediction, spawning, etc.

namespace Config {
namespace Gameplay {

// Client prediction
constexpr float PREDICTION_ERROR_THRESHOLD =
    50.0f;  // Pixels - log when error exceeds this

// Spawn search parameters
constexpr float SPAWN_SEARCH_RADIUS_INCREMENT =
    50.0f;  // Pixels to increment search radius
constexpr float SPAWN_SEARCH_MAX_RADIUS = 500.0f;  // Max search distance
constexpr float SPAWN_SEARCH_ANGLE_INCREMENT =
    45.0f;  // Degrees between test points

}  // namespace Gameplay
}  // namespace Config
