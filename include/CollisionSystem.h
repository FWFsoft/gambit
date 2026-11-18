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

  // ---- Terrain: solid outcrops on the map (Collision objects whose Tiled
  // type is "terrain"). Players and enemies go around them; Isaac's
  // Lifeline anchors to them.
  static bool isTerrain(const CollisionShape& shape) {
    return shape.objectType == "terrain";
  }
  // ---- The ship's hull: Collision objects whose Tiled type is "hull" (a
  // staircase of boxes under the crashed ship as drawn). Solid for players
  // and enemies like terrain, but not a rock: it is not drawn as one and
  // Isaac's Lifeline does not anchor to it.
  static bool isHull(const CollisionShape& shape) {
    return shape.objectType == "hull";
  }
  // Solid for enemies (players collide with every shape).
  static bool blocksEnemies(const CollisionShape& shape) {
    return isTerrain(shape) || isHull(shape);
  }
  // The terrain piece whose edge is nearest (x, y), if within maxDist.
  const CollisionShape* terrainNear(float x, float y, float maxDist) const;
  // The point of `box` nearest (x, y) (the point itself when inside).
  static void closestPoint(const AABB& box, float x, float y, float& outX,
                           float& outY);
  // Move (x, y) the shortest way out of every terrain piece and hull box it
  // overlaps.
  void pushOutOfTerrain(float& x, float& y, float radius) const;

 private:
  const std::vector<CollisionShape>& collisionShapes;

  // Check if player AABB intersects with collision shape
  bool intersects(float px, float py, float radius,
                  const CollisionShape& shape) const;

  // Swept AABB collision for smooth sliding
  void resolveCollision(float oldX, float oldY, float& newX, float& newY,
                        float radius, const CollisionShape& shape) const;
};
