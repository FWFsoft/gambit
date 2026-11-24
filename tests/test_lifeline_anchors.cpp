// Dependency-light regression for Lifeline's solid-prop query.
#include <cassert>
#include <iostream>
#include <vector>

#include "CollisionSystem.h"

int main() {
  for (const char* type : {"terrain", "tree", "prop", ""}) {
    CollisionShape shape;
    shape.type = CollisionShape::Type::Rectangle;
    shape.objectType = type;
    shape.aabb = {100.0f, -20.0f, 40.0f, 40.0f};
    std::vector<CollisionShape> shapes{shape};
    CollisionSystem solid(shapes);
    assert(solid.terrainNear(90.0f, 0.0f, 10.0f) == &shapes[0]);
    assert(!solid.terrainNear(89.0f, 0.0f, 10.0f));
  }
  CollisionShape hull;
  hull.type = CollisionShape::Type::Rectangle;
  hull.objectType = "hull1@1";
  hull.aabb = {100.0f, -20.0f, 40.0f, 40.0f};
  std::vector<CollisionShape> shapes{hull};
  CollisionSystem solid(shapes);
  solid.setHullSite(1);
  assert(!solid.terrainNear(100.0f, 0.0f, 0.0f));  // inactive tier
  solid.setHullTier(1);
  assert(solid.terrainNear(100.0f, 0.0f, 0.0f));
  solid.setHullSite(0);
  assert(!solid.terrainNear(100.0f, 0.0f, 0.0f));  // inactive site
  std::cout << "Lifeline solid anchors passed\n";
}
