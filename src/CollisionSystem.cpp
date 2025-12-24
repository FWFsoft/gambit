#include "CollisionSystem.h"

#include <algorithm>
#include <cmath>

CollisionSystem::CollisionSystem(
    const std::vector<CollisionShape>& collisionShapes)
    : collisionShapes(collisionShapes) {}

bool CollisionSystem::checkMovement(float oldX, float oldY, float& newX,
                                    float& newY, float playerRadius) const {
  bool collided = false;

  // Check against all collision shapes
  for (const auto& shape : collisionShapes) {
    if (intersects(newX, newY, playerRadius, shape)) {
      collided = true;
      // Resolve collision by sliding along edges
      resolveCollision(oldX, oldY, newX, newY, playerRadius, shape);
    }
  }

  return !collided;  // true if no collision
}

bool CollisionSystem::isPositionValid(float x, float y,
                                      float playerRadius) const {
  for (const auto& shape : collisionShapes) {
    if (intersects(x, y, playerRadius, shape)) {
      return false;
    }
  }
  return true;
}

bool CollisionSystem::intersects(float px, float py, float radius,
                                 const CollisionShape& shape) const {
  // Player bounding box (circle approximated as square for simplicity)
  AABB playerAABB;
  playerAABB.x = px - radius;
  playerAABB.y = py - radius;
  playerAABB.width = radius * 2;
  playerAABB.height = radius * 2;

  if (shape.type == CollisionShape::Type::Rectangle) {
    return playerAABB.intersects(shape.aabb);
  }
  // Future: Add polygon collision support

  return false;
}

void CollisionSystem::resolveCollision(float oldX, float oldY, float& newX,
                                       float& newY, float radius,
                                       const CollisionShape& shape) const {
  // Simple axis-aligned sliding collision resolution
  // Try moving only in X direction
  float testX = newX;
  float testY = oldY;
  if (!intersects(testX, testY, radius, shape)) {
    newX = testX;
    newY = testY;
    return;
  }

  // Try moving only in Y direction
  testX = oldX;
  testY = newY;
  if (!intersects(testX, testY, radius, shape)) {
    newX = testX;
    newY = testY;
    return;
  }

  // Both blocked, stay at old position
  newX = oldX;
  newY = oldY;
}
