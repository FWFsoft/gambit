#include "CollisionDebugRenderer.h"

CollisionDebugRenderer::CollisionDebugRenderer(
    Camera* camera, const CollisionSystem* collisionSystem)
    : camera(camera), collisionSystem(collisionSystem), enabled(false) {}

void CollisionDebugRenderer::render() {
  if (!enabled || !collisionSystem) {
    return;
  }

  const auto& shapes = collisionSystem->getShapes();
  for (const auto& shape : shapes) {
    renderShape(shape);
  }
}

void CollisionDebugRenderer::renderShape(const CollisionShape& shape) {
  if (shape.type == CollisionShape::Type::Rectangle) {
    renderAABB(shape.aabb, 255, 0, 0);  // Red for rectangles
  } else if (shape.type == CollisionShape::Type::Polygon) {
    // Render AABB in yellow for polygons
    renderAABB(shape.aabb, 255, 255, 0);
  }
}

void CollisionDebugRenderer::renderAABB(const AABB& aabb, uint8_t r, uint8_t g,
                                        uint8_t b) {
  // TODO: Implement OpenGL line rendering in Phase 3
  // For now, just suppress unused parameter warnings
  (void)aabb;
  (void)r;
  (void)g;
  (void)b;
}
