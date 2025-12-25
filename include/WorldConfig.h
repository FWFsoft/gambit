#pragma once

class CollisionSystem;

struct WorldConfig {
  float width;
  float height;
  const CollisionSystem* collisionSystem;

  WorldConfig(float w, float h, const CollisionSystem* cs = nullptr)
      : width(w), height(h), collisionSystem(cs) {}
};
