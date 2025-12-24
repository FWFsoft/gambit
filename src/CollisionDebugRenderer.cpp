#include "CollisionDebugRenderer.h"

CollisionDebugRenderer::CollisionDebugRenderer(
    SDL_Renderer* renderer, Camera* camera,
    const CollisionSystem* collisionSystem)
    : renderer(renderer),
      camera(camera),
      collisionSystem(collisionSystem),
      enabled(false) {}

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
  // Convert world corners to screen coordinates
  int screenX1, screenY1, screenX2, screenY2, screenX3, screenY3, screenX4,
      screenY4;

  // Top-left corner
  camera->worldToScreen(aabb.x, aabb.y, screenX1, screenY1);

  // Top-right corner
  camera->worldToScreen(aabb.x + aabb.width, aabb.y, screenX2, screenY2);

  // Bottom-right corner
  camera->worldToScreen(aabb.x + aabb.width, aabb.y + aabb.height, screenX3,
                        screenY3);

  // Bottom-left corner
  camera->worldToScreen(aabb.x, aabb.y + aabb.height, screenX4, screenY4);

  // Draw four lines to form the rectangle outline
  SDL_SetRenderDrawColor(renderer, r, g, b, 255);
  SDL_RenderDrawLine(renderer, screenX1, screenY1, screenX2, screenY2);
  SDL_RenderDrawLine(renderer, screenX2, screenY2, screenX3, screenY3);
  SDL_RenderDrawLine(renderer, screenX3, screenY3, screenX4, screenY4);
  SDL_RenderDrawLine(renderer, screenX4, screenY4, screenX1, screenY1);
}
