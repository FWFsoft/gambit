#pragma once

class CollisionSystem;
class TiledMap;

struct WorldConfig {
  float width;
  float height;
  // Mutable: the ship's hull footprint changes with its tier.
  CollisionSystem* collisionSystem;
  const TiledMap* tiledMap;

  WorldConfig(float w, float h, CollisionSystem* cs = nullptr,
              const TiledMap* map = nullptr)
      : width(w), height(h), collisionSystem(cs), tiledMap(map) {}
};
