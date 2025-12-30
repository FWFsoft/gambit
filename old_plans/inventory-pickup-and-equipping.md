# Item Pickup and Equipping Implementation Plan

## Overview

Add **item pickup from enemy loot drops** and **UI interactions** (click-to-equip, double-click consumables) to the Gambit inventory system.

**User Requirements:**
- Click-to-equip: Click items in inventory to equip to appropriate slot
- Double-click consumables to use them immediately
- Enemy loot drops: Enemies drop items when killed
- Auto-pickup on proximity: Items automatically picked up within 32 pixels

**Current State:**
- ✅ Inventory system fully implemented (20 slots + 2 equipment)
- ✅ Network protocol for UseItem/EquipItem exists
- ✅ Server handlers processUseItem() and processEquipItem() working
- ❌ NO UI click handlers (inventory is read-only)
- ❌ NO WorldItem entity type
- ❌ NO item spawn/pickup network packets

---

## Phase 1: WorldItem Entity & Network Protocol

### Create WorldItem Entity
**NEW FILE:** `include/WorldItem.h`

```cpp
#pragma once
#include <cstdint>

struct WorldItem {
  uint32_t id;        // Unique world item ID
  uint32_t itemId;    // References ItemDefinition
  float x, y;         // World position
  float spawnTime;    // Server tick when spawned

  WorldItem() : id(0), itemId(0), x(0.0f), y(0.0f), spawnTime(0.0f) {}
  WorldItem(uint32_t worldItemId, uint32_t itemDefId, float posX, float posY, float tick)
      : id(worldItemId), itemId(itemDefId), x(posX), y(posY), spawnTime(tick) {}
};
```

### Add Network Packets
**MODIFY:** `include/NetworkProtocol.h`

Add to PacketType enum (after line 20):
```cpp
ItemSpawned = 14,       // Server → Client (broadcast)
ItemPickupRequest = 15, // Client → Server
ItemPickedUp = 16       // Server → Client (broadcast)
```

Add packet structs (after EquipItemPacket, ~line 128):
```cpp
struct ItemSpawnedPacket {
  PacketType type = PacketType::ItemSpawned;
  uint32_t worldItemId;  // 17 bytes total
  uint32_t itemId;
  float x, y;
};

struct ItemPickupRequestPacket {
  PacketType type = PacketType::ItemPickupRequest;
  uint32_t worldItemId;  // 5 bytes total
};

struct ItemPickedUpPacket {
  PacketType type = PacketType::ItemPickedUp;
  uint32_t worldItemId;  // 9 bytes total
  uint32_t playerId;
};
```

**MODIFY:** `src/NetworkProtocol.cpp`
- Implement serialize() and deserialize() for 3 new packet types
- Follow existing patterns (writeUint32, writeFloat, etc.)

---

## Phase 2: Server-Side Enemy Loot Drops

### Add WorldItem Management
**MODIFY:** `include/ServerGameState.h`

Add private members:
```cpp
std::unordered_map<uint32_t, WorldItem> worldItems;
uint32_t nextWorldItemId;
```

Add methods:
```cpp
void checkEnemyLootDrops();
void spawnWorldItem(uint32_t itemId, float x, float y);
void processItemPickupRequest(ENetPeer* peer, const uint8_t* data, size_t size);
```

### Implement Loot System
**MODIFY:** `src/ServerGameState.cpp`

Constructor: Initialize `nextWorldItemId(1)`

In onUpdate() (after enemySystem->update):
```cpp
checkEnemyLootDrops();  // Check for enemy deaths and spawn loot
```

Implement checkEnemyLootDrops():
```cpp
void ServerGameState::checkEnemyLootDrops() {
  if (!enemySystem) return;

  const auto& deaths = enemySystem->getDiedThisFrame();
  for (const auto& death : deaths) {
    uint32_t lootItemId = 1;  // Health Potion

    const auto& enemies = enemySystem->getEnemies();
    auto enemyIt = enemies.find(death.enemyId);
    if (enemyIt != enemies.end()) {
      spawnWorldItem(lootItemId, enemyIt->second.x, enemyIt->second.y);
    }
  }
}
```

Implement spawnWorldItem():
```cpp
void ServerGameState::spawnWorldItem(uint32_t itemId, float x, float y) {
  uint32_t worldItemId = nextWorldItemId++;
  WorldItem worldItem(worldItemId, itemId, x, y, static_cast<float>(serverTick));
  worldItems[worldItemId] = worldItem;

  // Broadcast to all clients
  ItemSpawnedPacket packet;
  packet.worldItemId = worldItemId;
  packet.itemId = itemId;
  packet.x = x;
  packet.y = y;
  server->broadcastPacket(serialize(packet));
}
```

---

## Phase 3: Client-Side World Item Rendering

### Track World Items
**MODIFY:** `include/ClientPrediction.h`

Add private member:
```cpp
std::unordered_map<uint32_t, WorldItem> worldItems;
```

Add public accessor:
```cpp
const std::unordered_map<uint32_t, WorldItem>& getWorldItems() const {
  return worldItems;
}
```

**MODIFY:** `src/ClientPrediction.cpp`

In onNetworkPacketReceived() (after InventoryUpdate handler):
```cpp
else if (type == PacketType::ItemSpawned) {
  ItemSpawnedPacket packet = deserializeItemSpawned(e.data, e.size);
  WorldItem worldItem(packet.worldItemId, packet.itemId, packet.x, packet.y, 0.0f);
  worldItems[packet.worldItemId] = worldItem;
}
else if (type == PacketType::ItemPickedUp) {
  ItemPickedUpPacket packet = deserializeItemPickedUp(e.data, e.size);
  worldItems.erase(packet.worldItemId);
}
```

### Render World Items
**MODIFY:** `src/RenderSystem.cpp`

In onRender(), add world items to depth sorting:
```cpp
const auto& worldItems = clientPrediction->getWorldItems();

// Add WorldItem to EntityToRender union
for (const auto& [id, worldItem] : worldItems) {
  EntityToRender entity;
  entity.depth = worldItem.x + worldItem.y;
  entity.type = EntityToRender::WorldItem;
  entity.worldItem = &worldItem;
  entitiesToRender.push_back(entity);
}

// In render loop:
if (entity.type == EntityToRender::WorldItem) {
  renderWorldItem(*entity.worldItem);
}
```

Add renderWorldItem() method:
```cpp
void RenderSystem::renderWorldItem(const WorldItem& worldItem) {
  int screenX, screenY;
  camera->worldToScreen(worldItem.x, worldItem.y, screenX, screenY);

  // Render as 16x16 gold square (placeholder)
  glm::vec4 color(1.0f, 0.84f, 0.0f, 1.0f);  // Gold
  float size = 16.0f;

  spriteRenderer->renderSprite(whitePixelTexture.get(),
                               static_cast<float>(screenX - size/2),
                               static_cast<float>(screenY - size/2),
                               size, size, 0.0f, 0.0f, 1.0f, 1.0f, color);
}
```

---

## Phase 4: Auto-Pickup System

### Client-Side Proximity Detection
**MODIFY:** `src/client_main.cpp`

In UpdateEvent subscription (after client.run()):
```cpp
// Auto-pickup detection
GameState currentState = GameStateManager::instance().getCurrentState();
if (currentState == GameState::Playing) {
  const Player& localPlayer = clientPrediction.getLocalPlayer();

  if (localPlayer.isAlive()) {
    const auto& worldItems = clientPrediction.getWorldItems();
    constexpr float PICKUP_RADIUS = 32.0f;

    for (const auto& [worldItemId, worldItem] : worldItems) {
      float dx = localPlayer.x - worldItem.x;
      float dy = localPlayer.y - worldItem.y;
      float distanceSquared = dx * dx + dy * dy;

      if (distanceSquared <= PICKUP_RADIUS * PICKUP_RADIUS) {
        ItemPickupRequestPacket packet;
        packet.worldItemId = worldItemId;
        client.send(serialize(packet));
        break;  // Only one item per frame
      }
    }
  }
}
```

### Server-Side Pickup Validation
**MODIFY:** `src/ServerGameState.cpp`

In onNetworkPacketReceived():
```cpp
case PacketType::ItemPickupRequest:
  processItemPickupRequest(e.peer, e.data, e.size);
  break;
```

Implement processItemPickupRequest():
```cpp
void ServerGameState::processItemPickupRequest(ENetPeer* peer, const uint8_t* data, size_t size) {
  ItemPickupRequestPacket packet = deserializeItemPickupRequest(data, size);

  // Get player
  uint32_t playerId = peerToPlayerId[peer];
  Player& player = players[playerId];

  // Validate world item exists
  auto itemIt = worldItems.find(packet.worldItemId);
  if (itemIt == worldItems.end()) return;  // Already picked up

  const WorldItem& worldItem = itemIt->second;

  // Validate distance (anti-cheat)
  constexpr float PICKUP_RADIUS = 32.0f;
  float dx = player.x - worldItem.x;
  float dy = player.y - worldItem.y;
  if (dx*dx + dy*dy > PICKUP_RADIUS * PICKUP_RADIUS) return;

  // Validate inventory space
  int emptySlot = player.findEmptySlot();
  if (emptySlot == -1) return;  // Inventory full

  const ItemDefinition* itemDef = ItemRegistry::instance().getItem(worldItem.itemId);

  // Handle stacking for consumables
  if (itemDef->maxStackSize > 1) {
    int existingSlot = player.findItemStack(worldItem.itemId);
    if (existingSlot != -1 && player.inventory[existingSlot].quantity < itemDef->maxStackSize) {
      player.inventory[existingSlot].quantity++;
      worldItems.erase(packet.worldItemId);

      ItemPickedUpPacket pickupPacket;
      pickupPacket.worldItemId = packet.worldItemId;
      pickupPacket.playerId = playerId;
      server->broadcastPacket(serialize(pickupPacket));
      broadcastInventoryUpdate(playerId);
      return;
    }
  }

  // Add to inventory
  player.inventory[emptySlot] = ItemStack(worldItem.itemId, 1);
  worldItems.erase(packet.worldItemId);

  ItemPickedUpPacket pickupPacket;
  pickupPacket.worldItemId = packet.worldItemId;
  pickupPacket.playerId = playerId;
  server->broadcastPacket(serialize(pickupPacket));
  broadcastInventoryUpdate(playerId);
}
```

---

## Phase 5: UI Click Handlers

### Add NetworkClient to UISystem
**MODIFY:** `include/UISystem.h`

```cpp
UISystem(Window* window, ClientPrediction* clientPrediction, NetworkClient* client);

private:
  NetworkClient* client;  // NEW

  void handleInventorySlotClick(int slotIndex, const ItemDefinition& item);
  void handleEquipmentSlotClick(int equipmentSlotIndex);
  void handleConsumableUse(int slotIndex, const ItemDefinition& item);
```

**MODIFY:** `src/UISystem.cpp` - Update constructor
**MODIFY:** `src/client_main.cpp` (line 94) - Pass &client to UISystem

### Click-to-Equip from Inventory
**MODIFY:** `src/UISystem.cpp` (lines 280-290)

```cpp
if (!stack.isEmpty()) {
  const ItemDefinition* item = registry.getItem(stack.itemId);
  if (item) {
    std::string label = item->name + "\nx" + std::to_string(stack.quantity);
    bool clicked = ImGui::Button(label.c_str(), ImVec2(slotSize, slotSize));

    if (clicked) {
      handleInventorySlotClick(slotIndex, *item);
    }

    // Double-click consumables
    if (item->type == ItemType::Consumable &&
        ImGui::IsItemHovered() &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      handleConsumableUse(slotIndex, *item);
    }

    // Tooltip...
  }
}
```

Implement handlers:
```cpp
void UISystem::handleInventorySlotClick(int slotIndex, const ItemDefinition& item) {
  uint8_t equipmentSlot = 255;

  if (item.type == ItemType::Weapon) {
    equipmentSlot = EQUIPMENT_WEAPON_SLOT;
  } else if (item.type == ItemType::Armor) {
    equipmentSlot = EQUIPMENT_ARMOR_SLOT;
  } else {
    return;  // Consumables use double-click
  }

  EquipItemPacket packet;
  packet.inventorySlot = static_cast<uint8_t>(slotIndex);
  packet.equipmentSlot = equipmentSlot;
  client->send(serialize(packet));
}

void UISystem::handleConsumableUse(int slotIndex, const ItemDefinition& item) {
  UseItemPacket packet;
  packet.slotIndex = static_cast<uint8_t>(slotIndex);
  client->send(serialize(packet));
}
```

### Click-to-Unequip from Equipment Slots
**MODIFY:** `src/UISystem.cpp` (lines 356-395)

```cpp
bool clicked = ImGui::Button(weapon->name.c_str(), ImVec2(200, 80));
if (clicked) {
  handleEquipmentSlotClick(EQUIPMENT_WEAPON_SLOT);
}
```

Implement:
```cpp
void UISystem::handleEquipmentSlotClick(int equipmentSlotIndex) {
  const Player& localPlayer = clientPrediction->getLocalPlayer();
  const ItemStack& equipSlot = localPlayer.equipment[equipmentSlotIndex];

  if (equipSlot.isEmpty() || localPlayer.findEmptySlot() == -1) {
    return;  // Nothing equipped or inventory full
  }

  EquipItemPacket packet;
  packet.inventorySlot = static_cast<uint8_t>(equipmentSlotIndex);
  packet.equipmentSlot = 255;  // Unequip mode
  client->send(serialize(packet));
}
```

---

## Critical Files Summary

### NEW FILES (1):
- `include/WorldItem.h` - WorldItem entity struct

### MODIFIED FILES (10):
- `include/NetworkProtocol.h` - 3 packet types + structs
- `src/NetworkProtocol.cpp` - Serialization for 3 packets
- `include/ServerGameState.h` - worldItems map + methods
- `src/ServerGameState.cpp` - Loot drops + pickup validation (~200 lines)
- `include/ClientPrediction.h` - worldItems map + accessor
- `src/ClientPrediction.cpp` - Packet handlers for spawn/pickup
- `src/RenderSystem.cpp` - renderWorldItem() + depth sorting
- `src/client_main.cpp` - Auto-pickup proximity detection
- `include/UISystem.h` - NetworkClient + handler methods
- `src/UISystem.cpp` - Click/double-click handlers

---

## Testing Checklist

✅ Kill enemy → item spawns at death location
✅ Walk near item → auto-pickup to inventory
✅ Click weapon/armor → equips to slot
✅ Click equipped item → unequips to inventory
✅ Double-click potion → uses immediately, health increases
✅ Multiple players see same spawns/pickups
✅ Inventory full → pickup fails gracefully
✅ Server validates all operations
✅ Race condition handling (2 players pickup same item)

---

## Implementation Notes

- **Server authority**: All operations validated server-side (distance, inventory space, item type)
- **Tiger Style**: Simple implementations, aggressive assertions, fail fast
- **Network efficiency**: <100 bytes/second typical gameplay
- **Auto-stacking**: Consumables stack up to maxStackSize
- **Race conditions**: First pickup wins, second fails gracefully (item already removed)
