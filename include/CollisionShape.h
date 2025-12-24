#pragma once

#include <string>
#include <vector>

// Axis-Aligned Bounding Box for simple collision detection
struct AABB {
  float x, y;  // Top-left corner in world coordinates
  float width, height;

  bool intersects(const AABB& other) const {
    return !(x + width < other.x || other.x + other.width < x ||
             y + height < other.y || other.y + other.height < y);
  }

  bool contains(float px, float py) const {
    return px >= x && px <= x + width && py >= y && py <= y + height;
  }
};

struct CollisionShape {
  enum class Type {
    Rectangle,
    Polygon  // Future expansion
  };

  Type type;
  AABB aabb;  // Always computed for broad-phase

  std::string name;        // From Tiled object name
  std::string objectType;  // From Tiled object type
};
