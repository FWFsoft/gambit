#pragma once

#include <glad/glad.h>

#include "Camera.h"
#include "CollisionSystem.h"

class CollisionDebugRenderer {
 public:
  CollisionDebugRenderer(Camera* camera,
                         const CollisionSystem* collisionSystem);
  ~CollisionDebugRenderer();

  void render();
  void renderMapBounds(float worldWidth, float worldHeight);
  void setEnabled(bool enabled) { this->enabled = enabled; }
  bool isEnabled() const { return enabled; }
  void toggle() { enabled = !enabled; }

 private:
  Camera* camera;
  const CollisionSystem* collisionSystem;
  bool enabled;

  // OpenGL rendering state
  GLuint shaderProgram;
  GLuint VAO;
  GLuint VBO;

  void initRenderData();
  void renderShape(const CollisionShape& shape);
  void renderAABB(const AABB& aabb, uint8_t r, uint8_t g, uint8_t b);
};
