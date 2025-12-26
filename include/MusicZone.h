#pragma once

#include <string>

struct MusicZone {
  std::string name;       // Zone name (e.g., "forest_zone")
  std::string trackName;  // Music file (e.g., "forest_ambience.ogg")
  float x, y;             // Zone position (top-left)
  float width, height;    // Zone dimensions

  // Check if point is inside zone (AABB test)
  bool contains(float px, float py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
  }
};
