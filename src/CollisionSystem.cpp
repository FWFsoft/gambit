#include "CollisionSystem.h"

#include <algorithm>
#include <cassert>
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

const CollisionShape* CollisionSystem::terrainNear(float x, float y,
                                                   float maxDist) const {
  assert(maxDist >= 0.0f);
  const CollisionShape* best = nullptr;
  float bestDist = maxDist;
  for (const auto& shape : collisionShapes) {
    if (!isTerrain(shape)) continue;
    float cx, cy;
    closestPoint(shape.aabb, x, y, cx, cy);
    float d = std::hypot(cx - x, cy - y);
    if (d <= bestDist) {
      bestDist = d;
      best = &shape;
    }
  }
  return best;
}

void CollisionSystem::closestPoint(const AABB& box, float x, float y,
                                   float& outX, float& outY) {
  assert(box.width >= 0.0f && box.height >= 0.0f);
  outX = std::clamp(x, box.x, box.x + box.width);
  outY = std::clamp(y, box.y, box.y + box.height);
}

void CollisionSystem::pushOutOfTerrain(float& x, float& y, float radius) const {
  for (const auto& shape : collisionShapes) {
    if (!isTerrain(shape) || !intersects(x, y, radius, shape)) continue;
    const AABB& b = shape.aabb;
    // Distance to leave through each side; take the shortest.
    float left = x + radius - b.x;
    float right = b.x + b.width - (x - radius);
    float up = y + radius - b.y;
    float down = b.y + b.height - (y - radius);
    float best = std::min(std::min(left, right), std::min(up, down));
    if (best == left) {
      x -= left + 0.01f;
    } else if (best == right) {
      x += right + 0.01f;
    } else if (best == up) {
      y -= up + 0.01f;
    } else {
      y += down + 0.01f;
    }
  }
}
