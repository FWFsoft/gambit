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

  Logger::info("Loaded map: " + filepath + " (" + std::to_string(mapWidth) +
               "x" + std::to_string(mapHeight) + " tiles, " +
               std::to_string(tileWidth) + "x" + std::to_string(tileHeight) +
               "px)");

  return true;
}

int TiledMap::getWorldWidth() const { return mapWidth * (tileWidth / 2); }

int TiledMap::getWorldHeight() const { return mapHeight * (tileWidth / 2); }
