#include "TiledMap.h"

#include <cassert>

#include "Logger.h"

bool TiledMap::load(const std::string& filepath) {
  if (!tmxMap.load(filepath)) {
    Logger::error("Failed to load map: " + filepath);
    return false;
  }

  assert(tmxMap.getOrientation() == tmx::Orientation::Isometric &&
         "Map must be isometric orientation");

  mapWidth = static_cast<int>(tmxMap.getTileCount().x);
  mapHeight = static_cast<int>(tmxMap.getTileCount().y);
  tileWidth = static_cast<int>(tmxMap.getTileSize().x);
  tileHeight = static_cast<int>(tmxMap.getTileSize().y);

  tileLayers.clear();
  const auto& layers = tmxMap.getLayers();
  for (const auto& layer : layers) {
    if (layer->getType() == tmx::Layer::Type::Tile) {
      tileLayers.push_back(dynamic_cast<const tmx::TileLayer*>(layer.get()));
    }
  }

  extractCollisionShapes();

  Logger::info("Loaded map: " + filepath + " (" + std::to_string(mapWidth) +
               "x" + std::to_string(mapHeight) + " tiles, " +
               std::to_string(tileWidth) + "x" + std::to_string(tileHeight) +
               "px, " + std::to_string(collisionShapes.size()) +
               " collision shapes)");

  return true;
}

int TiledMap::getWorldWidth() const {
  return mapWidth * (tileWidth / 2) + (tileHeight / 2);
}

int TiledMap::getWorldHeight() const {
  return mapHeight * (tileWidth / 2) + (tileHeight / 2);
}

void TiledMap::extractCollisionShapes() {
  collisionShapes.clear();

  const auto& layers = tmxMap.getLayers();
  for (const auto& layer : layers) {
    if (layer->getType() == tmx::Layer::Type::Object) {
      const auto& objectLayer = layer->getLayerAs<tmx::ObjectGroup>();
      const auto& objects = objectLayer.getObjects();

      for (const auto& obj : objects) {
        CollisionShape shape;
        shape.name = obj.getName();
        shape.objectType = obj.getType();

        tmx::Object::Shape objShape = obj.getShape();

        if (objShape == tmx::Object::Shape::Rectangle) {
          shape.type = CollisionShape::Type::Rectangle;

          const auto& aabb = obj.getAABB();
          const auto& pos = obj.getPosition();

          shape.aabb.x = pos.x;
          shape.aabb.y = pos.y;
          shape.aabb.width = aabb.width;
          shape.aabb.height = aabb.height;

          collisionShapes.push_back(shape);

          Logger::info("Loaded collision rect: " + shape.name + " at (" +
                       std::to_string(shape.aabb.x) + ", " +
                       std::to_string(shape.aabb.y) + ") size " +
                       std::to_string(shape.aabb.width) + "x" +
                       std::to_string(shape.aabb.height));
        }
        // Future: Add polygon support here
      }
    }
  }
}
