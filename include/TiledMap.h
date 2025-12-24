#pragma once

#include <string>
#include <tmxlite/Map.hpp>
#include <tmxlite/ObjectGroup.hpp>
#include <tmxlite/TileLayer.hpp>
#include <vector>

#include "CollisionShape.h"

class TiledMap {
 public:
  TiledMap() = default;

  bool load(const std::string& filepath);

  int getWidth() const { return mapWidth; }
  int getHeight() const { return mapHeight; }
  int getTileWidth() const { return tileWidth; }
  int getTileHeight() const { return tileHeight; }
  int getWorldWidth() const;
  int getWorldHeight() const;
  const std::vector<const tmx::TileLayer*>& getTileLayers() const {
    return tileLayers;
  }
  const std::vector<CollisionShape>& getCollisionShapes() const {
    return collisionShapes;
  }

 private:
  tmx::Map tmxMap;
  int mapWidth = 0;
  int mapHeight = 0;
  int tileWidth = 0;
  int tileHeight = 0;
  std::vector<const tmx::TileLayer*> tileLayers;
  std::vector<CollisionShape> collisionShapes;

  void extractCollisionShapes();
};
