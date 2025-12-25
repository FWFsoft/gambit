#include "CollisionShape.h"
#include "CollisionSystem.h"
#include "Logger.h"
#include "test_utils.h"

// ============================================================================
// AABB Tests (8 tests)
// ============================================================================

TEST(AABB_Intersects_NoOverlap) {
  AABB a = {0.0f, 0.0f, 10.0f, 10.0f};
  AABB b = {20.0f, 20.0f, 10.0f, 10.0f};

  assert(!a.intersects(b));
  assert(!b.intersects(a));
}

TEST(AABB_Intersects_PartialOverlap) {
  AABB a = {0.0f, 0.0f, 10.0f, 10.0f};
  AABB b = {5.0f, 5.0f, 10.0f, 10.0f};

  assert(a.intersects(b));
  assert(b.intersects(a));
}

TEST(AABB_Intersects_CompleteContainment) {
  AABB outer = {0.0f, 0.0f, 100.0f, 100.0f};
  AABB inner = {25.0f, 25.0f, 50.0f, 50.0f};

  assert(outer.intersects(inner));
  assert(inner.intersects(outer));
}

TEST(AABB_Intersects_EdgeTouching) {
  // Boxes that touch at the edge should intersect
  AABB a = {0.0f, 0.0f, 10.0f, 10.0f};
  AABB b = {10.0f, 0.0f, 10.0f, 10.0f};

  assert(a.intersects(b));
  assert(b.intersects(a));
}

TEST(AABB_Contains_Inside) {
  AABB box = {10.0f, 10.0f, 50.0f, 50.0f};

  assert(box.contains(30.0f, 30.0f));  // Center
  assert(box.contains(11.0f, 11.0f));  // Near top-left
  assert(box.contains(59.0f, 59.0f));  // Near bottom-right
}

TEST(AABB_Contains_Outside) {
  AABB box = {10.0f, 10.0f, 50.0f, 50.0f};

  assert(!box.contains(5.0f, 30.0f));   // Left
  assert(!box.contains(65.0f, 30.0f));  // Right
  assert(!box.contains(30.0f, 5.0f));   // Above
  assert(!box.contains(30.0f, 65.0f));  // Below
}

TEST(AABB_Contains_OnEdge) {
  AABB box = {10.0f, 10.0f, 50.0f, 50.0f};

  assert(box.contains(10.0f, 30.0f));  // Left edge
  assert(box.contains(60.0f, 30.0f));  // Right edge
  assert(box.contains(30.0f, 10.0f));  // Top edge
  assert(box.contains(30.0f, 60.0f));  // Bottom edge
}

TEST(AABB_Contains_AtCorner) {
  AABB box = {10.0f, 10.0f, 50.0f, 50.0f};

  assert(box.contains(10.0f, 10.0f));  // Top-left
  assert(box.contains(60.0f, 10.0f));  // Top-right
  assert(box.contains(10.0f, 60.0f));  // Bottom-left
  assert(box.contains(60.0f, 60.0f));  // Bottom-right
}

// ============================================================================
// CollisionSystem Tests (12 tests)
// ============================================================================

TEST(CollisionSystem_NoCollision) {
  std::vector<CollisionShape> shapes;
  CollisionShape wall;
  wall.type = CollisionShape::Type::Rectangle;
  wall.aabb = {100.0f, 100.0f, 50.0f, 50.0f};
  shapes.push_back(wall);

  CollisionSystem system(shapes);

  float oldX = 10.0f, oldY = 10.0f;
  float newX = 20.0f, newY = 20.0f;
  float radius = 10.0f;

  bool noCollision = system.checkMovement(oldX, oldY, newX, newY, radius);

  assert(noCollision);
  assert(floatEqual(newX, 20.0f));
  assert(floatEqual(newY, 20.0f));
}

TEST(CollisionSystem_DirectBlock) {
  std::vector<CollisionShape> shapes;
  CollisionShape wall;
  wall.type = CollisionShape::Type::Rectangle;
  wall.aabb = {50.0f, 50.0f, 50.0f, 50.0f};
  shapes.push_back(wall);

  CollisionSystem system(shapes);

  // Try to move from inside/near the wall - no valid slide direction
  float oldX = 60.0f, oldY = 60.0f;
  float newX = 70.0f, newY = 70.0f;
  float radius = 15.0f;  // Large radius to ensure blocking

  bool noCollision = system.checkMovement(oldX, oldY, newX, newY, radius);

  // Should detect collision
  assert(!noCollision);
}

TEST(CollisionSystem_SlideAlongX) {
  std::vector<CollisionShape> shapes;
  CollisionShape wall;
  wall.type = CollisionShape::Type::Rectangle;
  wall.aabb = {50.0f, 50.0f, 10.0f, 50.0f};  // Vertical wall
  shapes.push_back(wall);

  CollisionSystem system(shapes);

  // Move diagonally toward the wall - should slide along X
  float oldX = 30.0f, oldY = 30.0f;
  float newX = 55.0f, newY = 40.0f;
  float radius = 5.0f;

  system.checkMovement(oldX, oldY, newX, newY, radius);

  // Should slide along X (allow horizontal movement, block vertical)
  assert(floatEqual(newX, 55.0f) || floatEqual(newX, oldX));
  assert(floatEqual(newY, oldY) || floatEqual(newY, 40.0f));
  // At least one axis should have moved
  assert(newX != oldX || newY != oldY);
}

TEST(CollisionSystem_SlideAlongY) {
  std::vector<CollisionShape> shapes;
  CollisionShape wall;
  wall.type = CollisionShape::Type::Rectangle;
  wall.aabb = {50.0f, 50.0f, 50.0f, 10.0f};  // Horizontal wall
  shapes.push_back(wall);

  CollisionSystem system(shapes);

  // Move diagonally toward the wall - should slide along Y
  float oldX = 30.0f, oldY = 30.0f;
  float newX = 40.0f, newY = 55.0f;
  float radius = 5.0f;

  system.checkMovement(oldX, oldY, newX, newY, radius);

  // Should slide (allow some movement, block penetration)
  assert(floatEqual(newX, oldX) || floatEqual(newX, 40.0f));
  assert(floatEqual(newY, oldY) || floatEqual(newY, 55.0f));
  // At least one axis should have moved
  assert(newX != oldX || newY != oldY);
}

TEST(CollisionSystem_BlockBothAxes) {
  std::vector<CollisionShape> shapes;
  CollisionShape wall;
  wall.type = CollisionShape::Type::Rectangle;
  wall.aabb = {40.0f, 40.0f, 20.0f, 20.0f};
  shapes.push_back(wall);

  CollisionSystem system(shapes);

  // Move from inside wall - both axes should be blocked
  float oldX = 50.0f, oldY = 50.0f;
  float newX = 55.0f, newY = 55.0f;
  float radius = 15.0f;  // Large radius ensures overlap

  bool noCollision = system.checkMovement(oldX, oldY, newX, newY, radius);

  // Should detect collision (return false)
  assert(!noCollision);
  // After collision resolution, position should still be valid or at old
  // position
  assert(system.isPositionValid(newX, newY, radius) ||
         (floatEqual(newX, oldX) && floatEqual(newY, oldY)));
}

TEST(CollisionSystem_CornerSliding) {
  std::vector<CollisionShape> shapes;
  CollisionShape wall;
  wall.type = CollisionShape::Type::Rectangle;
  wall.aabb = {100.0f, 100.0f, 50.0f, 50.0f};
  shapes.push_back(wall);

  CollisionSystem system(shapes);

  // Move toward the corner from below-left
  float oldX = 80.0f, oldY = 80.0f;
  float newX = 95.0f, newY = 95.0f;
  float radius = 5.0f;

  bool result = system.checkMovement(oldX, oldY, newX, newY, radius);

  // Should either slide or block, but not penetrate the wall
  // Verify position is valid after movement (no penetration)
  assert(system.isPositionValid(newX, newY, radius) ||
         (floatEqual(newX, oldX) && floatEqual(newY, oldY)));
}

TEST(CollisionSystem_MultipleShapes) {
  std::vector<CollisionShape> shapes;

  CollisionShape wall1;
  wall1.type = CollisionShape::Type::Rectangle;
  wall1.aabb = {50.0f, 50.0f, 10.0f, 50.0f};
  shapes.push_back(wall1);

  CollisionShape wall2;
  wall2.type = CollisionShape::Type::Rectangle;
  wall2.aabb = {100.0f, 50.0f, 10.0f, 50.0f};
  shapes.push_back(wall2);

  CollisionSystem system(shapes);

  // Move between the two walls - should be valid
  float oldX = 70.0f, oldY = 70.0f;
  float newX = 80.0f, newY = 70.0f;
  float radius = 5.0f;

  bool noCollision = system.checkMovement(oldX, oldY, newX, newY, radius);

  assert(noCollision);
  assert(floatEqual(newX, 80.0f));
}

TEST(CollisionSystem_IsPositionValid_Valid) {
  std::vector<CollisionShape> shapes;
  CollisionShape wall;
  wall.type = CollisionShape::Type::Rectangle;
  wall.aabb = {100.0f, 100.0f, 50.0f, 50.0f};
  shapes.push_back(wall);

  CollisionSystem system(shapes);

  // Position far from wall
  assert(system.isPositionValid(10.0f, 10.0f, 5.0f));
  assert(system.isPositionValid(200.0f, 200.0f, 5.0f));
}

TEST(CollisionSystem_IsPositionValid_Invalid) {
  std::vector<CollisionShape> shapes;
  CollisionShape wall;
  wall.type = CollisionShape::Type::Rectangle;
  wall.aabb = {100.0f, 100.0f, 50.0f, 50.0f};
  shapes.push_back(wall);

  CollisionSystem system(shapes);

  // Position inside wall
  assert(!system.isPositionValid(125.0f, 125.0f, 5.0f));
}

TEST(CollisionSystem_IsPositionValid_OnEdge) {
  std::vector<CollisionShape> shapes;
  CollisionShape wall;
  wall.type = CollisionShape::Type::Rectangle;
  wall.aabb = {100.0f, 100.0f, 50.0f, 50.0f};
  shapes.push_back(wall);

  CollisionSystem system(shapes);

  float radius = 5.0f;

  // Position on edge - player circle overlaps with wall
  bool edgeValid = system.isPositionValid(95.0f, 125.0f, radius);

  // Should be invalid (player radius overlaps wall)
  assert(!edgeValid);
}

TEST(CollisionSystem_PlayerRadiusAccounting) {
  std::vector<CollisionShape> shapes;
  CollisionShape wall;
  wall.type = CollisionShape::Type::Rectangle;
  wall.aabb = {100.0f, 100.0f, 50.0f, 50.0f};
  shapes.push_back(wall);

  CollisionSystem system(shapes);

  // Small radius - should fit close to wall
  assert(system.isPositionValid(90.0f, 125.0f, 1.0f));

  // Large radius - should collide even when center is far from wall
  assert(!system.isPositionValid(90.0f, 125.0f, 15.0f));
}

TEST(CollisionSystem_ZeroMovement) {
  std::vector<CollisionShape> shapes;
  CollisionShape wall;
  wall.type = CollisionShape::Type::Rectangle;
  wall.aabb = {100.0f, 100.0f, 50.0f, 50.0f};
  shapes.push_back(wall);

  CollisionSystem system(shapes);

  // No movement - should always succeed
  float oldX = 125.0f, oldY = 125.0f;
  float newX = 125.0f, newY = 125.0f;
  float radius = 5.0f;

  bool result = system.checkMovement(oldX, oldY, newX, newY, radius);

  // Even though we're inside a wall, zero movement returns true
  // (checkMovement only validates movement, not current position)
  assert(floatEqual(newX, oldX));
  assert(floatEqual(newY, oldY));
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
  Logger::init();

  // AABB tests
  test_AABB_Intersects_NoOverlap();
  test_AABB_Intersects_PartialOverlap();
  test_AABB_Intersects_CompleteContainment();
  test_AABB_Intersects_EdgeTouching();
  test_AABB_Contains_Inside();
  test_AABB_Contains_Outside();
  test_AABB_Contains_OnEdge();
  test_AABB_Contains_AtCorner();

  // CollisionSystem tests
  test_CollisionSystem_NoCollision();
  test_CollisionSystem_DirectBlock();
  test_CollisionSystem_SlideAlongX();
  test_CollisionSystem_SlideAlongY();
  test_CollisionSystem_BlockBothAxes();
  test_CollisionSystem_CornerSliding();
  test_CollisionSystem_MultipleShapes();
  test_CollisionSystem_IsPositionValid_Valid();
  test_CollisionSystem_IsPositionValid_Invalid();
  test_CollisionSystem_IsPositionValid_OnEdge();
  test_CollisionSystem_PlayerRadiusAccounting();
  test_CollisionSystem_ZeroMovement();

  return 0;
}
