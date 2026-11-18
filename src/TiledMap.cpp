#include "TiledMap.h"

#include <algorithm>
#include <cassert>

#include "ArenaData.h"
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
  extractShip();
  extractObjectives();
  extractTerrain();
  extractProps();

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
  // For centered isometric map, width is the horizontal extent of the diamond
  // worldX = (tileX - tileY) * tileWidth/2
  // Max occurs at right corner (mapWidth-1, 0): worldX = (mapWidth-1) *
  // tileWidth/2 After centering offset, max is still the same distance from
  // origin
  return (mapWidth - 1) * tileWidth;  // Full width is 2x maxX
}

int TiledMap::getWorldHeight() const {
  // For centered isometric map, height is the vertical extent of the diamond
  // worldY = (tileX + tileY) * tileHeight/4
  // Max occurs at bottom corner (mapWidth-1, mapHeight-1):
  // worldY = (mapWidth-1 + mapHeight-1) * tileHeight/4
  // After centering, max is still the same distance from origin
  return (mapWidth + mapHeight - 2) * tileHeight / 4;  // Full height is 2x maxY
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
      // and the set dressing (Props: looks only).
      if (objectLayer.getName() == "Music" ||
          objectLayer.getName() == "MusicZones" ||
          objectLayer.getName() == "Props") {
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
          if (shape.objectType == "terrain") continue;  // dozens: no log

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
        bool elite = false;
        const auto& properties = object.getProperties();
        for (const auto& prop : properties) {
          if (prop.getName() == "enemy_type" &&
              prop.getType() == tmx::Property::Type::String) {
            enemyTypeStr = prop.getStringValue();
          } else if (prop.getName() == "elite" &&
                     prop.getType() == tmx::Property::Type::Boolean) {
            elite = prop.getBoolValue();
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
        if (enemyTypeStr == "venoxarid_grunt") {
          type = EnemyType::VenoxaridGrunt;
        } else if (enemyTypeStr == "slime") {
          type = EnemyType::VenoxaridGrunt;
        } else if (enemyTypeStr == "goblin") {
          type = EnemyType::VenoxaridGrunt;
        } else if (enemyTypeStr == "skeleton") {
          type = EnemyType::VenoxaridGrunt;
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
        spawn.elite = elite;

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

void TiledMap::extractShip() {
  hasShip = false;

  float centerTileX = (tmxMap.getTileCount().x - 1) / 2.0f;
  float centerTileY = (tmxMap.getTileCount().y - 1) / 2.0f;
  float centerWorldX =
      (centerTileX - centerTileY) * tmxMap.getTileSize().x / 2.0f;
  float centerWorldY =
      (centerTileX + centerTileY) * tmxMap.getTileSize().y / 4.0f;

  for (const auto& layer : tmxMap.getLayers()) {
    if (layer->getType() != tmx::Layer::Type::Object) continue;
    const auto& objectLayer = layer->getLayerAs<tmx::ObjectGroup>();
    if (objectLayer.getName() != "Ship") continue;

    for (const auto& object : objectLayer.getObjects()) {
      if (object.getShape() != tmx::Object::Shape::Point) continue;
      const auto& pos = object.getPosition();
      shipX = pos.x - centerWorldX;
      shipY = pos.y - centerWorldY;
      hasShip = true;
      Logger::info("Loaded ship position at (" + std::to_string(shipX) + ", " +
                   std::to_string(shipY) + ")");
      return;
    }
  }

  Logger::info("No ship position found in map");
}

void TiledMap::extractObjectives() {
  objectives.clear();

  // Calculate map centering offset
  float centerTileX = (tmxMap.getTileCount().x - 1) / 2.0f;
  float centerTileY = (tmxMap.getTileCount().y - 1) / 2.0f;
  float centerWorldX =
      (centerTileX - centerTileY) * tmxMap.getTileSize().x / 2.0f;
  float centerWorldY =
      (centerTileX + centerTileY) * tmxMap.getTileSize().y / 4.0f;

  uint32_t objectiveId = 0;

  const auto& layers = tmxMap.getLayers();
  for (const auto& layer : layers) {
    if (layer->getType() == tmx::Layer::Type::Object) {
      const auto& objectLayer = layer->getLayerAs<tmx::ObjectGroup>();

      // Only process "Objectives" layer
      if (objectLayer.getName() != "Objectives") {
        continue;
      }

      for (const auto& object : objectLayer.getObjects()) {
        // Only process point objects (objective locations)
        if (object.getShape() != tmx::Object::Shape::Point) {
          continue;
        }

        // Extract properties
        std::string objectiveTypeStr;
        std::string description;
        float radius = 50.0f;
        float interactionTime = 3.0f;
        int enemiesRequired = 0;
        float depositX = 0.0f;
        float depositY = 0.0f;
        bool hasDepositPoint = false;

        const auto& properties = object.getProperties();
        for (const auto& prop : properties) {
          if (prop.getName() == "objective_type" &&
              prop.getType() == tmx::Property::Type::String) {
            objectiveTypeStr = prop.getStringValue();
          } else if (prop.getName() == "description" &&
                     prop.getType() == tmx::Property::Type::String) {
            description = prop.getStringValue();
          } else if (prop.getName() == "radius" &&
                     prop.getType() == tmx::Property::Type::Float) {
            radius = prop.getFloatValue();
          } else if (prop.getName() == "interaction_time" &&
                     prop.getType() == tmx::Property::Type::Float) {
            interactionTime = prop.getFloatValue();
          } else if (prop.getName() == "enemies_required" &&
                     prop.getType() == tmx::Property::Type::Int) {
            enemiesRequired = prop.getIntValue();
          } else if (prop.getName() == "deposit_x" &&
                     prop.getType() == tmx::Property::Type::Float) {
            depositX = prop.getFloatValue();
            hasDepositPoint = true;
          } else if (prop.getName() == "deposit_y" &&
                     prop.getType() == tmx::Property::Type::Float) {
            depositY = prop.getFloatValue();
            hasDepositPoint = true;
          }
        }

        // Skip if no objective_type property
        if (objectiveTypeStr.empty()) {
          Logger::info("Objective '" + object.getName() +
                       "' missing objective_type property, skipping");
          continue;
        }

        // Create objective with centering offset
        const auto& pos = object.getPosition();
        Objective objective;
        objective.id = objectiveId++;
        objective.type = parseObjectiveType(objectiveTypeStr);
        objective.state = ObjectiveState::Inactive;
        objective.name = object.getName();
        objective.description = description.empty()
                                    ? objectiveTypeToString(objective.type)
                                    : description;
        objective.x = pos.x - centerWorldX;
        objective.y = pos.y - centerWorldY;
        objective.radius = radius;
        objective.interactionTime = interactionTime;
        objective.enemiesRequired = enemiesRequired;

        // Load frogs_required for SaveTheFrogs (stored in enemiesRequired field
        // of TMX as enemies_required, mapped to frogsRequired in the struct)
        if (objective.type == ObjectiveType::SaveTheFrogs) {
          objective.frogsRequired =
              (enemiesRequired > 0) ? enemiesRequired : 3;
        }

        // Set deposit point — AlienScrapyard and RecoverProbe default to ship
        // Scrap is not on this list: salvaging finishes the scrapyard and
        // the player carries the scrap home (MatchDirector banks it).
        bool isDepositType = (objective.type == ObjectiveType::RecoverProbe ||
                              objective.type == ObjectiveType::SaveTheFrogs);

        if (hasDepositPoint) {
          objective.depositX = depositX - centerWorldX;
          objective.depositY = depositY - centerWorldY;
          objective.hasDepositPoint = true;
        } else if (isDepositType && hasShip) {
          // Default to ship as deposit point
          objective.depositX = shipX;
          objective.depositY = shipY;
          objective.hasDepositPoint = true;
          // The ship's hull is solid: drop off anywhere around it.
          objective.depositRadius = Arena::Tuning::SHIP_DROP_RADIUS;
          Logger::info("Objective '" + objective.name +
                       "' using ship as deposit point");
        } else if (isDepositType && !playerSpawns.empty()) {
          // Fallback to first player spawn if no ship defined
          objective.depositX = playerSpawns[0].x;
          objective.depositY = playerSpawns[0].y;
          objective.hasDepositPoint = true;
          Logger::info("Objective '" + objective.name +
                       "' using player spawn as deposit point");
        }

        objectives.push_back(objective);

        Logger::info("Loaded objective: " + objective.name +
                     " type=" + objectiveTypeToString(objective.type) +
                     " at (" + std::to_string(objective.x) + ", " +
                     std::to_string(objective.y) +
                     ") radius=" + std::to_string(objective.radius));
      }
    }
  }

  Logger::info("Loaded " + std::to_string(objectives.size()) +
               " objectives from map");
}

void TiledMap::extractTerrain() {
  terrainGrid.reset(mapWidth, mapHeight, tileWidth, tileHeight);
  // The hidden data layers index the "terrain_data" tileset.
  uint32_t dataGid = 0;
  for (const auto& tileset : tmxMap.getTilesets()) {
    if (tileset.getName() == "terrain_data") dataGid = tileset.getFirstGID();
  }
  const tmx::TileLayer* heightLayer = nullptr;
  const tmx::TileLayer* regionLayer = nullptr;
  const tmx::TileLayer* blockedLayer = nullptr;
  const tmx::TileLayer* biomeLayer = nullptr;
  for (const auto& layer : tmxMap.getLayers()) {
    if (layer->getType() != tmx::Layer::Type::Tile) continue;
    const auto* tiles = dynamic_cast<const tmx::TileLayer*>(layer.get());
    if (layer->getName() == "Height") heightLayer = tiles;
    if (layer->getName() == "Region") regionLayer = tiles;
    if (layer->getName() == "Blocked") blockedLayer = tiles;
    if (layer->getName() == "Biome") biomeLayer = tiles;
  }
  if (dataGid == 0 || !heightLayer) return;  // an old, flat map

  const size_t cells = static_cast<size_t>(mapWidth * mapHeight);
  assert(heightLayer->getTiles().size() == cells);
  assert(!regionLayer || regionLayer->getTiles().size() == cells);
  assert(!blockedLayer || blockedLayer->getTiles().size() == cells);
  assert(!biomeLayer || biomeLayer->getTiles().size() == cells);
  // terrain_data has one row of 16 tiles per layer: row 0 heights, row 1
  // regions, row 2 blocked, row 3 biomes. A layer's value is its tile's
  // column within its own row.
  auto value = [&](const tmx::TileLayer* layer, size_t i, int row, int fallback,
                   int limit) {
    if (!layer) return fallback;
    uint32_t gid = layer->getTiles()[i].ID;
    const uint32_t first = dataGid + static_cast<uint32_t>(row) * 16u;
    if (gid < first || gid >= first + 16u) return fallback;
    return std::clamp(static_cast<int>(gid - first), 0, limit - 1);
  };
  for (int ty = 0; ty < mapHeight; ty++) {
    for (int tx = 0; tx < mapWidth; tx++) {
      size_t i = static_cast<size_t>(ty * mapWidth + tx);
      int level = value(heightLayer, i, 0, TerrainGrid::BASE_LEVEL,
                        TerrainGrid::LEVEL_COUNT);
      int region = value(regionLayer, i, 1, 0, TerrainGrid::REGION_COUNT);
      bool solid =
          blockedLayer && blockedLayer->getTiles()[i].ID != 0;
      int biome = value(biomeLayer, i, 3, 0, TerrainGrid::BIOME_COUNT);
      terrainGrid.setCell(tx, ty, level, region, solid, biome);
    }
  }
  terrainGrid.finish();
  Logger::info(std::string("Loaded terrain: heights, regions, blocked cells") +
               (biomeLayer ? " and biomes" : ""));
}

void TiledMap::extractProps() {
  props.clear();
  float centerTileX = (mapWidth - 1) / 2.0f;
  float centerTileY = (mapHeight - 1) / 2.0f;
  float centerWorldX = (centerTileX - centerTileY) * tileWidth / 2.0f;
  float centerWorldY = (centerTileX + centerTileY) * tileHeight / 4.0f;
  for (const auto& layer : tmxMap.getLayers()) {
    if (layer->getType() != tmx::Layer::Type::Object) continue;
    const auto& group = layer->getLayerAs<tmx::ObjectGroup>();
    if (group.getName() != "Props") continue;
    for (const auto& object : group.getObjects()) {
      MapProp prop;
      prop.kind = object.getType().empty() ? object.getName() : object.getType();
      prop.x = object.getPosition().x - centerWorldX;
      prop.y = object.getPosition().y - centerWorldY;
      for (const auto& p : object.getProperties()) {
        const std::string& name = p.getName();
        float f = p.getType() == tmx::Property::Type::Float
                      ? p.getFloatValue()
                      : p.getType() == tmx::Property::Type::Int
                            ? static_cast<float>(p.getIntValue())
                            : 0.0f;
        if (name == "scale") prop.scale = f;
        else if (name == "variant") prop.variant = static_cast<int>(f);
        else if (name == "width") prop.width = f;
        else if (name == "depth") prop.depth = f;
        else if (name == "tall") prop.tall = f;
      }
      prop.region = terrainGrid.regionAt(prop.x, prop.y);
      props.push_back(prop);
    }
  }
  if (!props.empty()) {
    Logger::info("Loaded " + std::to_string(props.size()) + " props");
  }
}

float TiledMap::getMapFloat(const std::string& name, float fallback) const {
  for (const auto& p : tmxMap.getProperties()) {
    if (p.getName() != name) continue;
    if (p.getType() == tmx::Property::Type::Float) return p.getFloatValue();
    if (p.getType() == tmx::Property::Type::Int) {
      return static_cast<float>(p.getIntValue());
    }
  }
  return fallback;
}
