#pragma once

#include <cstdint>

// World item entity (items lying on the ground that can be picked up)
struct WorldItem {
  uint32_t id;      // Unique world item ID
  uint32_t itemId;  // References ItemDefinition in ItemRegistry
  float x, y;       // World position
  float spawnTime;  // Server tick when spawned (for potential auto-despawn)

  WorldItem() : id(0), itemId(0), x(0.0f), y(0.0f), spawnTime(0.0f) {}

  WorldItem(uint32_t worldItemId, uint32_t itemDefId, float posX, float posY,
            float tick)
      : id(worldItemId), itemId(itemDefId), x(posX), y(posY), spawnTime(tick) {}
};
