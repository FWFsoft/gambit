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
  ObjectiveInteract = 22,  // Client → Server (request to interact)
  ShipLocation = 23,       // Server → Client (ship/home base position)
  GameEvent = 24,          // Server → Client (ability casts, VFX, damage)
  MatchState = 25,         // Server → Client (round, zone, ship, scores)
  InfusionSlot = 26,       // Client → Server (slot an infusion)
  PlayerReady = 27,        // Client → Server (ready up in lobby/intermission)
  DebugCommand = 28        // Client → Server (playtest shortcuts)
};

// Ability buttons carried in ClientInputPacket::buttons (held state).
namespace InputButtons {
constexpr uint8_t Primary = 0x01;    // LMB
constexpr uint8_t Secondary = 0x02;  // RMB
constexpr uint8_t Special = 0x04;    // Q
constexpr uint8_t Mobility = 0x08;   // LSHIFT
constexpr uint8_t Core = 0x10;       // E
}  // namespace InputButtons

// PlayerState::flags
namespace PlayerFlags {
constexpr uint8_t Dead = 0x01;
constexpr uint8_t MoveLocked = 0x02;  // dash / forced motion: do not predict
constexpr uint8_t Rooted = 0x04;      // stunned / snared: input moves nothing
constexpr uint8_t Charging = 0x08;    // holding a charge or channel
constexpr uint8_t Hidden = 0x10;      // stealth / intangible / blade form
constexpr uint8_t Stunned = 0x20;
}  // namespace PlayerFlags

// NetworkEnemyState::flags
namespace EnemyFlags {
constexpr uint8_t Stunned = 0x01;
constexpr uint8_t Confused = 0x02;
constexpr uint8_t Elite = 0x04;
constexpr uint8_t AttackingShip = 0x08;
}  // namespace EnemyFlags

struct ClientInputPacket {
  PacketType type = PacketType::ClientInput;
  uint32_t inputSequence;
  bool moveLeft;
  bool moveRight;
  bool moveUp;
  bool moveDown;
  uint8_t buttons = 0;  // InputButtons bitmask (held state)
  float aimX = 0.0f;    // cursor position in world space
  float aimY = 0.0f;
};
// Size: 15 bytes (1 + 4 + 1 + 1 + 4 + 4)

struct PlayerState {
  uint32_t playerId;
  float x, y;
  float vx, vy;
  float health;
  uint8_t r, g, b;
  uint32_t lastInputSequence;  // Last input sequence processed by server

  // Arena combat state
  float maxHealth = 0.0f;
  float shield = 0.0f;
  uint8_t characterId = 0;
  uint8_t flags = 0;        // PlayerFlags bitmask
  float speedMult = 1.0f;   // movement multiplier the client must predict with
  float aimX = 0.0f;        // cursor world position (drives facing)
  float aimY = 0.0f;
  uint8_t charge = 0;       // 0-255 charge progress
  uint8_t chargeSlot = 255;  // slot being charged/channelled (255 = none)
  uint16_t cooldowns[5] = {0, 0, 0, 0, 0};  // remaining, in 10ms units
  uint8_t resource = 0;     // kit-specific meter, 0-255
};
constexpr size_t PLAYER_STATE_BYTES = 66;

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
  uint8_t flags = 0;   // EnemyFlags bitmask
  uint8_t windup = 0;  // 0-255 progress of the current attack telegraph
};
constexpr size_t ENEMY_STATE_BYTES = 32;

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
  float depositX, depositY;  // Deposit point for AlienScrapyard
};
// Size: 39 bytes (1 + 4 + 1 + 1 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4)

struct ObjectiveInteractPacket {
  PacketType type = PacketType::ObjectiveInteract;
  uint32_t objectiveId;  // Which objective to interact with (0 = nearest)
};
// Size: 5 bytes (1 + 4)

struct ShipLocationPacket {
  PacketType type = PacketType::ShipLocation;
  float x, y;  // World position of the ship (home base)
};
// Size: 9 bytes (1 + 4 + 4)

// ---------------------------------------------------------------------------
// Arena packets
// ---------------------------------------------------------------------------

// Everything the client needs to draw combat: the server resolves abilities
// and tells clients what happened. The meaning of x/y/x2/y2/value/value2
// depends on `kind` (see comments).
enum class GameEventKind : uint8_t {
  // sourceId = caster. param = slot, param2 = theme. x2,y2 = aim point.
  // value = animation lock in ms.
  AbilityCast = 1,
  // Melee arc: x,y origin; x2,y2 direction; value radius; value2 arc degrees.
  Slash = 2,
  // Filled sector: same fields as Slash.
  Cone = 3,
  // Filled circle flash at x,y; value radius.
  Burst = 4,
  // Expanding ring at x,y; value radius.
  Ring = 5,
  // Line from x,y to x2,y2; value width; value2 duration ms.
  Beam = 6,
  // extraId = projectile id. x,y origin; x2,y2 velocity (px/s); value radius;
  // value2 lifetime ms. param2 = theme, param = sprite variant.
  ProjectileSpawn = 7,
  // extraId = projectile id; x,y = where it ended.
  ProjectileEnd = 8,
  // Arcing lob from x,y to x2,y2; value radius; value2 flight time ms.
  Lob = 9,
  // extraId = zone id; x,y centre; value radius; value2 duration ms.
  ZoneSpawn = 10,
  ZoneEnd = 11,
  // Warning marker: circle at x,y radius value, fills over value2 ms.
  TelegraphCircle = 12,
  // Warning marker: sector, origin x,y, direction x2,y2, value radius,
  // value2 duration ms, param = half angle in degrees.
  TelegraphCone = 13,
  // Trail from x,y to x2,y2.
  DashTrail = 14,
  // Damage number at x,y: value amount. param: 1 = crit, 2 = to a player.
  // sourceId/sourceIsEnemy identify the victim (for hit flash).
  Damage = 15,
  // Heal number at x,y: value amount. sourceId = healed player.
  Heal = 16,
  // Floating status text at x,y. param = EffectType.
  EffectText = 17,
  // Enemy starts an attack animation. sourceId = enemy. value = windup ms.
  EnemyAttack = 18,
  // Tether between x,y and x2,y2 for value2 ms.
  Tether = 19,
  // Screen banner. param = Announcement.
  Announcement = 20,
  // Enemy spawn puff at x,y.
  SpawnPuff = 21,
  // Shield bubble on player sourceId for value2 ms.
  ShieldBubble = 22
};

// Colour themes for VFX (GameEventPacket::param2).
enum class VfxTheme : uint8_t {
  Neutral = 0,
  Brew = 1,     // amber
  Steel = 2,    // white/silver
  Holy = 3,     // gold
  Wind = 4,     // cyan
  Poison = 5,   // green
  Psychic = 6,  // magenta
  Ice = 7,      // pale blue
  Fire = 8,     // orange
  Blood = 9,    // red
  Shadow = 10,  // violet
  Heal = 11,    // mint
  Danger = 12   // enemy telegraph red
};

enum class Announcement : uint8_t {
  RoundStart = 1,
  ZoneShrinking = 2,
  ReturnToShip = 3,
  ShipUnderAttack = 4,
  ShipTurretsOnline = 5,
  Intermission = 6,
  BossIncoming = 7,
  Victory = 8,
  Defeat = 9,
  PlayerDown = 10,
  PlayerRevived = 11,
  ScrapDelivered = 12,
  ObjectiveComplete = 13
};

struct GameEventPacket {
  PacketType type = PacketType::GameEvent;
  GameEventKind kind = GameEventKind::Burst;
  uint32_t sourceId = 0;
  bool sourceIsEnemy = false;
  float x = 0.0f, y = 0.0f;
  float x2 = 0.0f, y2 = 0.0f;
  float value = 0.0f;
  float value2 = 0.0f;
  uint8_t param = 0;
  uint8_t param2 = 0;
  uint32_t extraId = 0;
};
// Size: 37 bytes

constexpr int MATCH_MAX_PLAYERS = 8;

struct MatchPlayerState {
  uint32_t playerId = 0;
  uint8_t characterId = 0;
  int32_t score = 0;
  uint16_t kills = 0;
  uint8_t deaths = 0;
  uint8_t ready = 0;
  uint8_t coreGenus = 0;            // Arena::Genus
  uint8_t treeGenus[4] = {0, 0, 0, 0};  // per InfusionTree
  uint8_t treeLevel[4] = {0, 0, 0, 0};
  uint8_t inventory[4] = {0, 0, 0, 0};  // unslotted infusions per genus
  uint8_t carryingScrap = 0;
  float reviveProgress = 0.0f;  // 0-1 while being revived
};

struct MatchStatePacket {
  PacketType type = PacketType::MatchState;
  uint8_t phase = 0;  // Arena::MatchPhase
  uint8_t round = 0;  // 1-based
  uint8_t roundCount = 0;
  float timeRemaining = 0.0f;  // seconds left in the current phase
  float shipHealth = 0.0f;
  float shipMaxHealth = 0.0f;
  float shipX = 0.0f, shipY = 0.0f;
  float zoneRadius = 0.0f;
  uint16_t scrapBanked = 0;
  uint8_t turretsOnline = 0;
  uint32_t bossId = 0;
  std::vector<MatchPlayerState> players;
};

struct InfusionSlotPacket {
  PacketType type = PacketType::InfusionSlot;
  uint8_t genus = 0;  // Arena::Genus
  uint8_t slot = 0;   // 0 = core, 1-4 = InfusionTree + 1
};

struct PlayerReadyPacket {
  PacketType type = PacketType::PlayerReady;
  uint8_t ready = 1;
};

enum class DebugCommandKind : uint8_t {
  SkipPhase = 1,       // jump to the next phase
  GiveInfusions = 2,   // one of each genus
  SpawnWave = 3,       // spawn a pack around the player
  ToggleGodMode = 4,
  SpawnBoss = 5,
  KillAll = 6
};

struct DebugCommandPacket {
  PacketType type = PacketType::DebugCommand;
  uint8_t command = 0;
  uint8_t arg = 0;
};

// Serialization functions
std::vector<uint8_t> serialize(const GameEventPacket& packet);
std::vector<uint8_t> serialize(const MatchStatePacket& packet);
std::vector<uint8_t> serialize(const InfusionSlotPacket& packet);
std::vector<uint8_t> serialize(const PlayerReadyPacket& packet);
std::vector<uint8_t> serialize(const DebugCommandPacket& packet);
GameEventPacket deserializeGameEvent(const uint8_t* data, size_t size);
MatchStatePacket deserializeMatchState(const uint8_t* data, size_t size);
InfusionSlotPacket deserializeInfusionSlot(const uint8_t* data, size_t size);
PlayerReadyPacket deserializePlayerReady(const uint8_t* data, size_t size);
DebugCommandPacket deserializeDebugCommand(const uint8_t* data, size_t size);

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
std::vector<uint8_t> serialize(const ShipLocationPacket& packet);

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
ShipLocationPacket deserializeShipLocation(const uint8_t* data, size_t size);

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
