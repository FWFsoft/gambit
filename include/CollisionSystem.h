#pragma once

#include <vector>

#include "CollisionShape.h"
#include "TerrainGrid.h"

class CollisionSystem {
 public:
  // `terrain` (optional): the map's blocked cells (sea, peaks, city blocks)
  // stop movement like the shapes do.
  CollisionSystem(const std::vector<CollisionShape>& collisionShapes,
                  const TerrainGrid* terrain = nullptr);

  // Returns true if movement is allowed, false if blocked
  // If blocked, adjusts newX/newY to slide along collision boundaries
  bool checkMovement(float oldX, float oldY, float& newX, float& newY,
                     float playerRadius = 16.0f) const;

  // Simple point-in-shapes test (for spawning, teleporting, etc.)
  bool isPositionValid(float x, float y, float playerRadius = 16.0f) const;

  const TerrainGrid* getTerrain() const { return terrain; }

  const std::vector<CollisionShape>& getShapes() const {
    return collisionShapes;
  }

  // ---- Terrain: solid outcrops on the map (Collision objects whose Tiled
  // type is "terrain"). Players and enemies go around them; Isaac's
  // Lifeline anchors to them, and to the ship's hull.
  static bool isTerrain(const CollisionShape& shape) {
    return shape.objectType == "terrain";
  }
  // ---- The ship's hull: a staircase of boxes under the ship as drawn, one
  // set per ship tier (Tiled type "hull" for tier 0, "hull1", "hull2").
  // Only the current tier's set is solid. Solid for players and enemies
  // like terrain, but not a rock: it is not drawn as one and Isaac's
  // Lifeline anchors to it like a rock (terrainNear).
  // The tier a hull box belongs to, or -1 for anything else.
  static int hullTierOf(const CollisionShape& shape);
  void setHullTier(int tier);
  int getHullTier() const { return hullTier; }
  // A hull box of the current tier.
  bool isActiveHull(const CollisionShape& shape) const {
    return hullTierOf(shape) == hullTier;
  }
  // Blocks movement now (a hull box of another tier does not).
  bool isSolid(const CollisionShape& shape) const {
    int tier = hullTierOf(shape);
    return tier < 0 || tier == hullTier;
  }
  // Solid for enemies (players collide with every solid shape).
  bool blocksEnemies(const CollisionShape& shape) const {
    return isTerrain(shape) || isActiveHull(shape);
  }
  // The terrain piece or current-tier hull box whose edge is nearest (x, y),
  // if within maxDist: what Isaac's Lifeline can anchor to.
  const CollisionShape* terrainNear(float x, float y, float maxDist) const;
  // The point of `box` nearest (x, y) (the point itself when inside).
  static void closestPoint(const AABB& box, float x, float y, float& outX,
                           float& outY);
  // Move (x, y) the shortest way out of every terrain piece and hull box it
  // overlaps.
  void pushOutOfTerrain(float& x, float& y, float radius) const;

 private:
  const std::vector<CollisionShape>& collisionShapes;
  const TerrainGrid* terrain = nullptr;
  int hullTier = 0;

  bool terrainBlocks(float x, float y, float radius) const {
    return terrain && terrain->blocksBody(x, y, radius);
  }

  // Check if player AABB intersects with collision shape
  bool intersects(float px, float py, float radius,
                  const CollisionShape& shape) const;

  // Swept AABB collision for smooth sliding
  void resolveCollision(float oldX, float oldY, float& newX, float& newY,
                        float radius, const CollisionShape& shape) const;
};
