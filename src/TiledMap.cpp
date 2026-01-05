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
  extractEnemySpawns();
  extractPlayerSpawns();

  // Calculate map centering offset for music zones
  float centerTileX = (mapWidth - 1) / 2.0f;
  float centerTileY = (mapHeight - 1) / 2.0f;
  float centerWorldX = (centerTileX - centerTileY) * tileWidth / 2.0f;
  float centerWorldY = (centerTileX + centerTileY) * tileHeight / 4.0f;

  // Extract music zones from object layers
  musicZones.clear();
  for (const auto& layer : layers) {
    if (layer->getType() == tmx::Layer::Type::Object) {
      const auto& objectLayer = layer->getLayerAs<tmx::ObjectGroup>();

      // Only process "Music" or "MusicZones" layers
      if (objectLayer.getName() != "Music" &&
          objectLayer.getName() != "MusicZones") {
        continue;
      }

      for (const auto& object : objectLayer.getObjects()) {
        // Only process rectangle zones
        if (object.getShape() != tmx::Object::Shape::Rectangle) {
          continue;
        }

        // Extract music_track property
        std::string trackName;
        const auto& properties = object.getProperties();
        for (const auto& prop : properties) {
          if (prop.getName() == "music_track" &&
              prop.getType() == tmx::Property::Type::String) {
            trackName = prop.getStringValue();
            break;
          }
        }

        // Skip if no music_track property
        if (trackName.empty()) {
          Logger::info("Music zone '" + object.getName() +
                       "' missing music_track property, skipping");
          continue;
        }

        // Create MusicZone with centering offset
        const auto& aabb = object.getAABB();
        MusicZone zone;
        zone.name = object.getName();
        zone.trackName = trackName;
        zone.x = aabb.left - centerWorldX;
        zone.y = aabb.top - centerWorldY;
        zone.width = aabb.width;
        zone.height = aabb.height;

        musicZones.push_back(zone);
        Logger::info("Loaded music zone: " + zone.name + " -> " +
                     zone.trackName);
      }
    }
  }

  Logger::info("Loaded map: " + filepath + " (" + std::to_string(mapWidth) +
               "x" + std::to_string(mapHeight) + " tiles, " +
               std::to_string(tileWidth) + "x" + std::to_string(tileHeight) +
               "px, " + std::to_string(collisionShapes.size()) +
               " collision shapes, " + std::to_string(enemySpawns.size()) +
               " enemy spawns, " + std::to_string(playerSpawns.size()) +
               " player spawns)");

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

  // Calculate map centering offset (same as in TileRenderer::gridToWorld)
  int mapWidth = tmxMap.getTileCount().x;
  int mapHeight = tmxMap.getTileCount().y;
  int tileWidth = tmxMap.getTileSize().x;
  int tileHeight = tmxMap.getTileSize().y;

  float centerTileX = (mapWidth - 1) / 2.0f;
  float centerTileY = (mapHeight - 1) / 2.0f;
  float centerWorldX = (centerTileX - centerTileY) * tileWidth / 2.0f;
  float centerWorldY = (centerTileX + centerTileY) * tileHeight / 4.0f;

  const auto& layers = tmxMap.getLayers();
  for (const auto& layer : layers) {
    if (layer->getType() == tmx::Layer::Type::Object) {
      const auto& objectLayer = layer->getLayerAs<tmx::ObjectGroup>();

      // Skip Music/MusicZones layers (they're for music zones, not collision)
      if (objectLayer.getName() == "Music" ||
          objectLayer.getName() == "MusicZones") {
        continue;
      }

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

          // Apply centering offset to match tile coordinate system
          shape.aabb.x = pos.x - centerWorldX;
          shape.aabb.y = pos.y - centerWorldY;
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

void TiledMap::extractEnemySpawns() {
  enemySpawns.clear();

  // Calculate map centering offset
  float centerTileX = (tmxMap.getTileCount().x - 1) / 2.0f;
  float centerTileY = (tmxMap.getTileCount().y - 1) / 2.0f;
  float centerWorldX =
      (centerTileX - centerTileY) * tmxMap.getTileSize().x / 2.0f;
  float centerWorldY =
      (centerTileX + centerTileY) * tmxMap.getTileSize().y / 4.0f;

  const auto& layers = tmxMap.getLayers();
  for (const auto& layer : layers) {
    if (layer->getType() == tmx::Layer::Type::Object) {
      const auto& objectLayer = layer->getLayerAs<tmx::ObjectGroup>();

      // Only process "Spawns" or "EnemySpawns" layers
      if (objectLayer.getName() != "Spawns" &&
          objectLayer.getName() != "EnemySpawns") {
        continue;
      }

      for (const auto& object : objectLayer.getObjects()) {
        // Only process point objects (spawn locations)
        if (object.getShape() != tmx::Object::Shape::Point) {
          continue;
        }

        // Extract enemy_type property
        std::string enemyTypeStr;
        const auto& properties = object.getProperties();
        for (const auto& prop : properties) {
          if (prop.getName() == "enemy_type" &&
              prop.getType() == tmx::Property::Type::String) {
            enemyTypeStr = prop.getStringValue();
            break;
          }
        }

        // Skip if no enemy_type property
        if (enemyTypeStr.empty()) {
          Logger::info("Enemy spawn '" + object.getName() +
                       "' missing enemy_type property, skipping");
          continue;
        }

        // Parse enemy type
        EnemyType type;
        if (enemyTypeStr == "slime") {
          type = EnemyType::Slime;
        } else if (enemyTypeStr == "goblin") {
          type = EnemyType::Goblin;
        } else if (enemyTypeStr == "skeleton") {
          type = EnemyType::Skeleton;
        } else {
          Logger::info("Unknown enemy_type: " + enemyTypeStr + ", skipping");
          continue;
        }

        // Create spawn point with centering offset
        const auto& pos = object.getPosition();
        EnemySpawn spawn;
        spawn.type = type;
        spawn.x = pos.x - centerWorldX;
        spawn.y = pos.y - centerWorldY;
        spawn.name = object.getName();

        enemySpawns.push_back(spawn);

        Logger::info("Loaded enemy spawn: " + spawn.name + " type=" +
                     enemyTypeStr + " at (" + std::to_string(spawn.x) + ", " +
                     std::to_string(spawn.y) + ")");
      }
    }
  }
}

std::string TiledMap::getTilesetImagePath() const {
  const auto& tilesets = tmxMap.getTilesets();
  if (tilesets.empty()) {
    return "";
  }
  return tilesets[0].getImagePath();
}

int TiledMap::getTilesetColumns() const {
  const auto& tilesets = tmxMap.getTilesets();
  if (tilesets.empty()) {
    return 1;
  }
  return tilesets[0].getColumnCount();
}

int TiledMap::getTilesetSpacing() const {
  const auto& tilesets = tmxMap.getTilesets();
  if (tilesets.empty()) {
    return 0;
  }
  return tilesets[0].getSpacing();
}

void TiledMap::extractPlayerSpawns() {
  playerSpawns.clear();

  // Calculate map centering offset
  float centerTileX = (tmxMap.getTileCount().x - 1) / 2.0f;
  float centerTileY = (tmxMap.getTileCount().y - 1) / 2.0f;
  float centerWorldX =
      (centerTileX - centerTileY) * tmxMap.getTileSize().x / 2.0f;
  float centerWorldY =
      (centerTileX + centerTileY) * tmxMap.getTileSize().y / 4.0f;

  const auto& layers = tmxMap.getLayers();
  for (const auto& layer : layers) {
    if (layer->getType() == tmx::Layer::Type::Object) {
      const auto& objectLayer = layer->getLayerAs<tmx::ObjectGroup>();

      // Only process "PlayerSpawns" layer
      if (objectLayer.getName() != "PlayerSpawns") {
        continue;
      }

      for (const auto& object : objectLayer.getObjects()) {
        // Only process point objects (spawn locations)
        if (object.getShape() != tmx::Object::Shape::Point) {
          continue;
        }

        // Create spawn point with centering offset
        const auto& pos = object.getPosition();
        PlayerSpawn spawn;
        spawn.x = pos.x - centerWorldX;
        spawn.y = pos.y - centerWorldY;
        spawn.name = object.getName();

        playerSpawns.push_back(spawn);

        Logger::info("Loaded player spawn: " + spawn.name + " at (" +
                     std::to_string(spawn.x) + ", " + std::to_string(spawn.y) +
                     ")");
      }
    }
  }
}
