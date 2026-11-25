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

  // Decorative trees: Lifeline anchors that block nothing.
  {
    std::vector<CollisionShape> none;
    CollisionSystem open(none);
    CollisionShape tree;
    assert(CollisionSystem::anchorForProp("conifer", 300.0f, 0.0f, 1.0f, tree));
    CollisionShape grass;
    assert(!CollisionSystem::anchorForProp("grass", 0.0f, 0.0f, 1.0f, grass));
    assert(!CollisionSystem::anchorForProp("reeds", 0.0f, 0.0f, 1.0f, grass));
    open.setAnchors({tree});
    const CollisionShape* found = open.terrainNear(270.0f, 0.0f, 20.0f);
    assert(found && found->objectType == "anchor");
    assert(!open.terrainNear(200.0f, 0.0f, 20.0f));  // out of snap reach
    // Walking straight through the trunk is still allowed.
    assert(open.isPositionValid(300.0f, 0.0f, 16.0f));
    float nx = 320.0f, ny = 0.0f;
    assert(open.checkMovement(280.0f, 0.0f, nx, ny, 16.0f));
    assert(nx == 320.0f && ny == 0.0f);
    // A bigger tree has a wider trunk.
    CollisionShape big;
    assert(CollisionSystem::anchorForProp("conifer", 0.0f, 0.0f, 2.0f, big));
    assert(big.aabb.width == tree.aabb.width * 2.0f);
  }
  std::cout << "Lifeline solid anchors passed\n";
}
