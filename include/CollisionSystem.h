#pragma once

#include <vector>

#include "CollisionShape.h"

class CollisionSystem {
 public:
  CollisionSystem(const std::vector<CollisionShape>& collisionShapes);

  // Returns true if movement is allowed, false if blocked
  // If blocked, adjusts newX/newY to slide along collision boundaries
  bool checkMovement(float oldX, float oldY, float& newX, float& newY,
                     float playerRadius = 16.0f) const;

  // Simple point-in-shapes test (for spawning, teleporting, etc.)
  bool isPositionValid(float x, float y, float playerRadius = 16.0f) const;

  const std::vector<CollisionShape>& getShapes() const {
    return collisionShapes;
  }

 private:
  const std::vector<CollisionShape>& collisionShapes;

  // Check if player AABB intersects with collision shape
  bool intersects(float px, float py, float radius,
                  const CollisionShape& shape) const;

  // Swept AABB collision for smooth sliding
  void resolveCollision(float oldX, float oldY, float& newX, float& newY,
                        float radius, const CollisionShape& shape) const;
};
