#pragma once

#include <SDL2/SDL.h>

#include "Camera.h"
#include "CollisionSystem.h"

class CollisionDebugRenderer {
 public:
  CollisionDebugRenderer(SDL_Renderer* renderer, Camera* camera,
                         const CollisionSystem* collisionSystem);

  void render();
  void setEnabled(bool enabled) { this->enabled = enabled; }
  bool isEnabled() const { return enabled; }
  void toggle() { enabled = !enabled; }

 private:
  SDL_Renderer* renderer;
  Camera* camera;
  const CollisionSystem* collisionSystem;
  bool enabled;

  void renderShape(const CollisionShape& shape);
  void renderAABB(const AABB& aabb, uint8_t r, uint8_t g, uint8_t b);
};
