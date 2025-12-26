#pragma once

#include <cmath>

// 8-directional movement for isometric games
enum class AnimationDirection {
  IDLE = 0,
  NORTH,      // Up-left in isometric
  NORTHEAST,  // Up in isometric
  EAST,       // Up-right in isometric
  SOUTHEAST,  // Right in isometric
  SOUTH,      // Down-right in isometric
  SOUTHWEST,  // Down in isometric
  WEST,       // Down-left in isometric
  NORTHWEST   // Left in isometric
};

// Convert velocity vector to animation direction
inline AnimationDirection velocityToDirection(float vx, float vy) {
  // If not moving, return idle
  float speed = std::sqrt(vx * vx + vy * vy);
  if (speed < 0.1f) {
    return AnimationDirection::IDLE;
  }

  // Calculate angle in radians
  float angle = std::atan2(vy, vx);

  // Convert to degrees (0-360)
  float degrees = angle * 180.0f / 3.14159265f;
  if (degrees < 0) degrees += 360.0f;

  // Map to 8 directions (45-degree slices)
  // In Gambit's isometric coordinate system:
  //   dx=-1, dy=-1 → NORTH (up-left)
  //   dx=+1, dy=-1 → NORTHEAST (up)
  //   dx=+1, dy=+1 → EAST (up-right)
  //   etc.

  if (degrees >= 337.5f || degrees < 22.5f) {
    return AnimationDirection::EAST;
  } else if (degrees >= 22.5f && degrees < 67.5f) {
    return AnimationDirection::SOUTHEAST;
  } else if (degrees >= 67.5f && degrees < 112.5f) {
    return AnimationDirection::SOUTH;
  } else if (degrees >= 112.5f && degrees < 157.5f) {
    return AnimationDirection::SOUTHWEST;
  } else if (degrees >= 157.5f && degrees < 202.5f) {
    return AnimationDirection::WEST;
  } else if (degrees >= 202.5f && degrees < 247.5f) {
    return AnimationDirection::NORTHWEST;
  } else if (degrees >= 247.5f && degrees < 292.5f) {
    return AnimationDirection::NORTH;
  } else {
    return AnimationDirection::NORTHEAST;
  }
}
