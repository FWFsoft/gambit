#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

enum class PacketType : uint8_t {
  ClientInput = 1,
  StateUpdate = 2,
  PlayerJoined = 3,
  PlayerLeft = 4,
  EnemyStateUpdate = 5,
  EnemyDamaged = 6,
  EnemyDied = 7,
  AttackEnemy = 8,
  PlayerDied = 9,
  PlayerRespawned = 10,
  InventoryUpdate = 11,
  UseItem = 12,
  EquipItem = 13,
  ItemSpawned = 14,        // Server → Client (broadcast)
  ItemPickupRequest = 15,  // Client → Server
  ItemPickedUp = 16,       // Server → Client (broadcast)
  EffectApplied = 17,      // Server → Client (broadcast)
  EffectRemoved = 18,      // Server → Client (broadcast)
  EffectUpdate = 19,       // Server → Client (broadcast)
  CharacterSelected = 20,  // Client → Server
  ObjectiveState = 21,     // Server → Client (broadcast objective status)
  ObjectiveInteract = 22   // Client → Server (request to interact)
};

struct ClientInputPacket {
  PacketType type = PacketType::ClientInput;
  uint32_t inputSequence;
  bool moveLeft;
  bool moveRight;
  bool moveUp;
  bool moveDown;
};

struct PlayerState {
  uint32_t playerId;
  float x, y;
  float vx, vy;
  float health;
  uint8_t r, g, b;
  uint32_t lastInputSequence;  // Last input sequence processed by server
};

struct StateUpdatePacket {
  PacketType type = PacketType::StateUpdate;
  uint32_t serverTick;
  std::vector<PlayerState> players;
};

struct PlayerJoinedPacket {
  PacketType type = PacketType::PlayerJoined;
  uint32_t playerId;
  uint8_t r, g, b;
};

struct PlayerLeftPacket {
  PacketType type = PacketType::PlayerLeft;
  uint32_t playerId;
};

struct PlayerDiedPacket {
  PacketType type = PacketType::PlayerDied;
  uint32_t playerId;
};

struct PlayerRespawnedPacket {
  PacketType type = PacketType::PlayerRespawned;
  uint32_t playerId;
  float x, y;  // Respawn position
};

// Enemy-related packets

struct NetworkEnemyState {
  uint32_t id;
  uint8_t type;   // EnemyType enum
  uint8_t state;  // EnemyState enum
  float x, y;
  float vx, vy;
  float health;
  float maxHealth;
};

struct EnemyStateUpdatePacket {
  PacketType type = PacketType::EnemyStateUpdate;
  std::vector<NetworkEnemyState> enemies;
};

struct AttackEnemyPacket {
  PacketType type = PacketType::AttackEnemy;
  uint32_t enemyId;
  float damage;
};

// Dead code - EnemyDamagedPacket is never used
// TODO: Remove entirely if combat damage system doesn't need it
// struct EnemyDamagedPacket {
//   PacketType type = PacketType::EnemyDamaged;
//   uint32_t enemyId;
//   float newHealth;
//   uint32_t attackerId;
// };

struct EnemyDiedPacket {
  PacketType type = PacketType::EnemyDied;
  uint32_t enemyId;
  uint32_t killerId;
};

// Inventory-related packets

struct NetworkItemStack {
  uint32_t itemId;
  int32_t quantity;
};

struct InventoryUpdatePacket {
  PacketType type = PacketType::InventoryUpdate;
  uint32_t playerId;
  NetworkItemStack inventory[20];  // Fixed size inventory
  NetworkItemStack equipment[2];   // Weapon and armor slots
};

struct UseItemPacket {
  PacketType type = PacketType::UseItem;
  uint8_t slotIndex;  // Inventory slot to use
};

struct EquipItemPacket {
  PacketType type = PacketType::EquipItem;
  uint8_t inventorySlot;  // Source inventory slot
  uint8_t equipmentSlot;  // 0 = weapon, 1 = armor (255 = unequip)
};

// World item packets

struct ItemSpawnedPacket {
  PacketType type = PacketType::ItemSpawned;
  uint32_t worldItemId;  // Unique ID for this world item instance
  uint32_t itemId;       // ItemDefinition ID
  float x, y;            // World position
};

struct ItemPickupRequestPacket {
  PacketType type = PacketType::ItemPickupRequest;
  uint32_t worldItemId;  // Which world item to pick up
};

struct ItemPickedUpPacket {
  PacketType type = PacketType::ItemPickedUp;
  uint32_t worldItemId;  // Item that was picked up
  uint32_t playerId;     // Who picked it up
};

// Effect-related packets

struct EffectAppliedPacket {
  PacketType type = PacketType::EffectApplied;
  uint32_t targetId;   // Player or Enemy ID
  bool isEnemy;        // true = enemy, false = player
  uint8_t effectType;  // EffectType enum
  uint8_t stacks;
  float remainingDuration;  // Milliseconds
  uint32_t sourceId;        // Who applied it
};
// Size: 16 bytes (1 + 4 + 1 + 1 + 1 + 4 + 4)

struct EffectRemovedPacket {
  PacketType type = PacketType::EffectRemoved;
  uint32_t targetId;
  bool isEnemy;
  uint8_t effectType;
};
// Size: 7 bytes (1 + 4 + 1 + 1)

struct NetworkEffect {
  uint8_t effectType;
  uint8_t stacks;
  float remainingDuration;
};

struct EffectUpdatePacket {
  PacketType type = PacketType::EffectUpdate;
  uint32_t targetId;
  bool isEnemy;
  std::vector<NetworkEffect> effects;
};
// Size: Variable (1 + 4 + 1 + 2 + effectCount * 6)

// Character selection packet
struct CharacterSelectedPacket {
  PacketType type = PacketType::CharacterSelected;
  uint32_t characterId;  // Character ID (1-19)
};
// Size: 5 bytes (1 + 4)

// Objective-related packets

struct ObjectiveStatePacket {
  PacketType type = PacketType::ObjectiveState;
  uint32_t objectiveId;
  uint8_t objectiveType;   // ObjectiveType enum
  uint8_t objectiveState;  // ObjectiveState enum
  float x, y;              // Position
  float radius;            // Interaction radius
  float progress;          // 0.0 - 1.0
  int32_t enemiesRequired;
  int32_t enemiesKilled;
};
// Size: 30 bytes (1 + 4 + 1 + 1 + 4 + 4 + 4 + 4 + 4 + 4)

struct ObjectiveInteractPacket {
  PacketType type = PacketType::ObjectiveInteract;
  uint32_t objectiveId;  // Which objective to interact with (0 = nearest)
};
// Size: 5 bytes (1 + 4)

// Serialization functions
std::vector<uint8_t> serialize(const ClientInputPacket& packet);
std::vector<uint8_t> serialize(const StateUpdatePacket& packet);
std::vector<uint8_t> serialize(const PlayerJoinedPacket& packet);
std::vector<uint8_t> serialize(const PlayerLeftPacket& packet);
std::vector<uint8_t> serialize(const PlayerDiedPacket& packet);
std::vector<uint8_t> serialize(const PlayerRespawnedPacket& packet);
std::vector<uint8_t> serialize(const EnemyStateUpdatePacket& packet);
std::vector<uint8_t> serialize(const AttackEnemyPacket& packet);
// std::vector<uint8_t> serialize(const EnemyDamagedPacket& packet);  // Dead
// code
std::vector<uint8_t> serialize(const EnemyDiedPacket& packet);
std::vector<uint8_t> serialize(const InventoryUpdatePacket& packet);
std::vector<uint8_t> serialize(const UseItemPacket& packet);
std::vector<uint8_t> serialize(const EquipItemPacket& packet);
std::vector<uint8_t> serialize(const ItemSpawnedPacket& packet);
std::vector<uint8_t> serialize(const ItemPickupRequestPacket& packet);
std::vector<uint8_t> serialize(const ItemPickedUpPacket& packet);
std::vector<uint8_t> serialize(const EffectAppliedPacket& packet);
std::vector<uint8_t> serialize(const EffectRemovedPacket& packet);
std::vector<uint8_t> serialize(const EffectUpdatePacket& packet);
std::vector<uint8_t> serialize(const CharacterSelectedPacket& packet);
std::vector<uint8_t> serialize(const ObjectiveStatePacket& packet);
std::vector<uint8_t> serialize(const ObjectiveInteractPacket& packet);

// Deserialization functions
ClientInputPacket deserializeClientInput(const uint8_t* data, size_t size);
StateUpdatePacket deserializeStateUpdate(const uint8_t* data, size_t size);
PlayerDiedPacket deserializePlayerDied(const uint8_t* data, size_t size);
PlayerRespawnedPacket deserializePlayerRespawned(const uint8_t* data,
                                                 size_t size);
PlayerJoinedPacket deserializePlayerJoined(const uint8_t* data, size_t size);
PlayerLeftPacket deserializePlayerLeft(const uint8_t* data, size_t size);
EnemyStateUpdatePacket deserializeEnemyStateUpdate(const uint8_t* data,
                                                   size_t size);
AttackEnemyPacket deserializeAttackEnemy(const uint8_t* data, size_t size);
// EnemyDamagedPacket deserializeEnemyDamaged(const uint8_t* data, size_t size);
// // Dead code
EnemyDiedPacket deserializeEnemyDied(const uint8_t* data, size_t size);
InventoryUpdatePacket deserializeInventoryUpdate(const uint8_t* data,
                                                 size_t size);
UseItemPacket deserializeUseItem(const uint8_t* data, size_t size);
EquipItemPacket deserializeEquipItem(const uint8_t* data, size_t size);
ItemSpawnedPacket deserializeItemSpawned(const uint8_t* data, size_t size);
ItemPickupRequestPacket deserializeItemPickupRequest(const uint8_t* data,
                                                     size_t size);
ItemPickedUpPacket deserializeItemPickedUp(const uint8_t* data, size_t size);
EffectAppliedPacket deserializeEffectApplied(const uint8_t* data, size_t size);
EffectRemovedPacket deserializeEffectRemoved(const uint8_t* data, size_t size);
EffectUpdatePacket deserializeEffectUpdate(const uint8_t* data, size_t size);
CharacterSelectedPacket deserializeCharacterSelected(const uint8_t* data,
                                                     size_t size);
ObjectiveStatePacket deserializeObjectiveState(const uint8_t* data,
                                               size_t size);
ObjectiveInteractPacket deserializeObjectiveInteract(const uint8_t* data,
                                                     size_t size);

// Helper functions for binary I/O
void writeUint32(std::vector<uint8_t>& buffer, uint32_t value);
void writeInt32(std::vector<uint8_t>& buffer, int32_t value);
void writeUint16(std::vector<uint8_t>& buffer, uint16_t value);
void writeFloat(std::vector<uint8_t>& buffer, float value);
void writeUint8(std::vector<uint8_t>& buffer, uint8_t value);

uint32_t readUint32(const uint8_t* data);
int32_t readInt32(const uint8_t* data);
uint16_t readUint16(const uint8_t* data);
float readFloat(const uint8_t* data);
uint8_t readUint8(const uint8_t* data);
