#include "CollisionSystem.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <utility>

CollisionSystem::CollisionSystem(
    const std::vector<CollisionShape>& collisionShapes,
    const TerrainGrid* terrain)
    : collisionShapes(collisionShapes), terrain(terrain) {}

bool CollisionSystem::checkMovement(float oldX, float oldY, float& newX,
                                    float& newY, float playerRadius) const {
  bool collided = false;

  // Check against all collision shapes
  for (const auto& shape : collisionShapes) {
    if (!isSolid(shape)) continue;
    if (intersects(newX, newY, playerRadius, shape)) {
      collided = true;
      // Resolve collision by sliding along edges
      resolveCollision(oldX, oldY, newX, newY, playerRadius, shape);
    }
  }

  // Blocked ground: slide along it like a wall, one axis at a time.
  if (terrainBlocks(newX, newY, playerRadius)) {
    collided = true;
    if (!terrainBlocks(newX, oldY, playerRadius)) {
      newY = oldY;
    } else if (!terrainBlocks(oldX, newY, playerRadius)) {
      newX = oldX;
    } else {
      newX = oldX;
      newY = oldY;
    }
  }

  return !collided;  // true if no collision
}

bool CollisionSystem::isPositionValid(float x, float y,
                                      float playerRadius) const {
  for (const auto& shape : collisionShapes) {
    if (isSolid(shape) && intersects(x, y, playerRadius, shape)) {
      return false;
    }
  }
  return !terrainBlocks(x, y, playerRadius);
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

int CollisionSystem::hullTierOf(const CollisionShape& shape) {
  // "hull", "hull1".."hull9", each optionally "@<site>" (a landing site).
  const std::string& full = shape.objectType;
  const size_t at = full.find('@');
  const size_t len = at == std::string::npos ? full.size() : at;
  if (len == 4 && full.compare(0, 4, "hull") == 0) return 0;
  if (len == 5 && full.compare(0, 4, "hull") == 0 && full[4] >= '1' &&
      full[4] <= '9') {
    return full[4] - '0';
  }
  return -1;
}

int CollisionSystem::hullSiteOf(const CollisionShape& shape) {
  const std::string& full = shape.objectType;
  const size_t at = full.find('@');
  if (at == std::string::npos || hullTierOf(shape) < 0) return -1;
  int site = std::atoi(full.c_str() + at + 1);
  assert(site >= 0 && site < 64 && "hull site out of range");
  return site;
}

void CollisionSystem::setHullTier(int tier) {
  assert(tier >= 0 && tier <= 9);
  hullTier = tier;
}

void CollisionSystem::setHullSite(int site) {
  assert(site >= -1 && site < 64);
  hullSite = site;
}

void CollisionSystem::placeHullNear(float x, float y) {
  // The landing site whose hull is nearest the ship. Cheap: a handful of
  // boxes, and only when the ship moves.
  int best = -1;
  float bestDist = 0.0f;
  for (const CollisionShape& shape : collisionShapes) {
    int site = hullSiteOf(shape);
    if (site < 0 || shape.type != CollisionShape::Type::Rectangle) continue;
    float cx = shape.aabb.x + shape.aabb.width * 0.5f;
    float cy = shape.aabb.y + shape.aabb.height * 0.5f;
    float d = (cx - x) * (cx - x) + (cy - y) * (cy - y);
    if (best < 0 || d < bestDist) {
      best = site;
      bestDist = d;
    }
  }
  hullSite = best;
}

const CollisionShape* CollisionSystem::terrainNear(float x, float y,
                                                   float maxDist) const {
  assert(maxDist >= 0.0f);
  const CollisionShape* best = nullptr;
  float bestDist = maxDist;
  for (const auto& shape : collisionShapes) {
    // Every prop that blocks players is an anchor, not only the rocks that
    // also block enemies. Ignore inactive hulls and unsupported polygons.
    if (!isSolid(shape) || shape.type != CollisionShape::Type::Rectangle) {
      continue;
    }
    float cx, cy;
    closestPoint(shape.aabb, x, y, cx, cy);
    float d = std::hypot(cx - x, cy - y);
    if (d <= bestDist) {
      bestDist = d;
      best = &shape;
    }
  }
  for (const auto& shape : anchors) {
    float cx, cy;
    closestPoint(shape.aabb, x, y, cx, cy);
    float d = std::hypot(cx - x, cy - y);
    if (d < bestDist) {  // a solid shape wins a tie
      bestDist = d;
      best = &shape;
    }
  }
  return best;
}

void CollisionSystem::setAnchors(std::vector<CollisionShape> anchorShapes) {
  for (const CollisionShape& shape : anchorShapes) {
    assert(shape.type == CollisionShape::Type::Rectangle);
    assert(shape.objectType == "anchor");
    assert(shape.aabb.width > 0.0f && shape.aabb.height > 0.0f);
  }
  anchors = std::move(anchorShapes);
}

bool CollisionSystem::anchorForProp(const std::string& kind, float x, float y,
                                    float scale, CollisionShape& out) {
  // Half the width of what a tether can wrap round, in world px at scale 1:
  // the trunk, not the canopy. Ground cover (grass, reeds, flora, debris,
  // bones, craters) holds nothing. City blocks are not here: they block as
  // terrain cells, not as boxes.
  struct Trunk {
    const char* kind;
    float half;
  };
  static const Trunk TRUNKS[] = {
      {"conifer", 18.0f},  {"snow_conifer", 18.0f}, {"cypress", 18.0f},
      {"jungle_tree", 20.0f}, {"palm", 14.0f},   {"cactus", 14.0f},
      {"boulder", 30.0f},  {"sea_stack", 45.0f},    {"ruin", 34.0f},
      {"monolith", 30.0f}, {"spire", 30.0f},        {"ribcage", 40.0f},
      {"freighter", 90.0f}, {"lamp", 10.0f},        {"sign", 10.0f},
  };
  assert(scale > 0.0f);
  for (const Trunk& trunk : TRUNKS) {
    if (kind != trunk.kind) continue;
    float half = trunk.half * scale;
    out.type = CollisionShape::Type::Rectangle;
    // Flattened like the ground plane (isometric: half as deep as wide).
    out.aabb = {x - half, y - half * 0.5f, half * 2.0f, half};
    out.name = kind;
    out.objectType = "anchor";
    return true;
  }
  return false;
}

void CollisionSystem::closestPoint(const AABB& box, float x, float y,
                                   float& outX, float& outY) {
  assert(box.width >= 0.0f && box.height >= 0.0f);
  outX = std::clamp(x, box.x, box.x + box.width);
  outY = std::clamp(y, box.y, box.y + box.height);
}

void CollisionSystem::pushOutOfTerrain(float& x, float& y, float radius) const {
  for (const auto& shape : collisionShapes) {
    if (!blocksEnemies(shape) || !intersects(x, y, radius, shape)) continue;
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
  if (terrain) terrain->nearestOpen(x, y, radius, 400.0f);
}
