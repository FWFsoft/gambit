#pragma once

class CollisionSystem;
class TiledMap;

struct WorldConfig {
  float width;
  float height;
  const CollisionSystem* collisionSystem;
  const TiledMap* tiledMap;

  WorldConfig(float w, float h, const CollisionSystem* cs = nullptr,
              const TiledMap* map = nullptr)
      : width(w), height(h), collisionSystem(cs), tiledMap(map) {}
};
