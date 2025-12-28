# Enemies and Combat System Implementation Plan

## Goal

Implement a working enemies and combat system for Gambit that creates a playable demo with NPCs, basic AI, and combat mechanics. This builds on the existing Player/Animation/Network architecture to create the foundation for an ARPG game.

## User Requirements

From user: "UI + Enemies + Combat sounds better, I think a full game that's playable at least as a demo should exist before we try and separate things."

**Core Features:**
- **Enemies**: NPCs that spawn in the world and can move
- **Combat**: Players can damage and kill enemies
- **AI**: Basic enemy behavior (idle, chase, attack)
- **Visual Feedback**: Enemies render with animations and health bars
- **Playable Demo**: 4-player co-op fighting enemies

**Deferred to Future:**
- UI System (ImGui) - tracked but not implemented yet
- Advanced AI (pathfinding, formations, abilities)
- Loot/rewards system
- Enemy variety (just one enemy type for POC)

## Architecture Overview

### Design Principles

Following the existing Gambit patterns:

1. **Server-Authoritative**: Enemies managed server-side, clients render
2. **Event-Driven**: EnemySystem subscribes to UpdateEvent like AnimationSystem
3. **Reuse Animatable Interface**: Enemies use same animation system as Player
4. **Network Protocol**: New packet types for enemy state (similar to PlayerState)
5. **Tiled Integration**: Spawn points defined as objects in Tiled map
6. **Tiger Style**: Assert on invalid states, crash early

### Key Architectural Decisions

**Decision 1: Entity Type System**

After exploring Player.h, there are three options:
- **Option A**: Create `Enemy` struct similar to Player (inherits Animatable)
- **Option B**: Create base `Entity` class, Player and Enemy inherit from it
- **Option C**: Use ECS (Entity Component System)

**Chosen: Option A (Enemy struct similar to Player)**

**Rationale:**
- Fastest to implement (mirrors Player pattern)
- No major refactoring of existing Player code
- Animation system already generic via Animatable interface
- Can refactor to inheritance later if needed (YAGNI principle)
- Tiger Style: Keep it simple, avoid premature abstraction

**Decision 2: Enemy AI Architecture**

**Chosen: Simple State Machine**

States:
- **Idle**: Enemy stands still, looks for players in range
- **Chase**: Enemy moves toward nearest player
- **Attack**: Enemy attacks player in melee range
- **Dead**: Enemy is killed, plays death animation, despawns

**Rationale:**
- POC scope: simple but playable
- State machine pattern is easy to debug
- Can extend later with more states (flee, patrol, etc.)

**Decision 3: Network Synchronization**

**Chosen: Server sends full enemy state snapshots (like PlayerState)**

- Server broadcasts `EnemyStatePacket` containing all enemies
- Clients interpolate enemy positions (reuse RemotePlayerInterpolation pattern)
- Combat events sent as separate packets (EnemyDamaged, EnemyDead)

**Rationale:**
- Matches existing network pattern
- Simple and reliable
- Interpolation handles latency smoothly

## Phase Breakdown

### Phase 1: Enemy Data Structure and Spawn System

**Goal**: Define Enemy struct and extract spawn points from Tiled map

#### 1.1 Create Enemy Struct

**New File: `include/Enemy.h`**

```cpp
#pragma once

#include <cstdint>
#include "Animatable.h"
#include "AnimationController.h"

enum class EnemyState : uint8_t {
  Idle = 0,
  Chase = 1,
  Attack = 2,
  Dead = 3
};

enum class EnemyType : uint8_t {
  Slime = 0,     // POC: Only implement this one
  Goblin = 1,    // Future
  Skeleton = 2   // Future
};

struct Enemy : public Animatable {
  uint32_t id;              // Unique enemy ID
  EnemyType type;           // Enemy type (determines sprite, stats, etc.)
  EnemyState state;         // Current AI state

  // Transform
  float x, y;               // World position
  float vx, vy;             // Velocity (for animation direction)

  // Combat stats
  float health;             // Current health
  float maxHealth;          // Maximum health
  float damage;             // Damage per attack
  float attackRange;        // Melee attack range
  float detectionRange;     // How far enemy can detect players
  float speed;              // Movement speed (pixels/second)

  // Target tracking
  uint32_t targetPlayerId;  // Player being chased/attacked (0 = no target)

  // Animation
  AnimationController animController;

  // Animatable interface (for AnimationSystem)
  AnimationController* getAnimationController() override {
    return &animController;
  }
  const AnimationController* getAnimationController() const override {
    return &animController;
  }
  float getVelocityX() const override { return vx; }
  float getVelocityY() const override { return vy; }

  // Constructor with defaults for Slime enemy type
  Enemy()
      : id(0), type(EnemyType::Slime), state(EnemyState::Idle),
        x(0.0f), y(0.0f), vx(0.0f), vy(0.0f),
        health(50.0f), maxHealth(50.0f), damage(10.0f),
        attackRange(40.0f), detectionRange(200.0f), speed(100.0f),
        targetPlayerId(0) {}
};
```

**Design Notes:**
- Inherits from Animatable (works with existing AnimationSystem)
- Similar to Player struct (position, velocity, health)
- Adds AI-specific fields (state, target, detection range)
- Only Slime type implemented for POC
- Default constructor with Slime stats

#### 1.2 Define Spawn Points in Tiled

**New File: `include/EnemySpawn.h`**

```cpp
#pragma once

#include <string>
#include "Enemy.h"

struct EnemySpawn {
  EnemyType type;      // Type of enemy to spawn
  float x, y;          // Spawn position
  std::string name;    // Spawn point name (for debugging)

  // Future: spawn rate, max count, respawn timer
};
```

**Modified File: `include/TiledMap.h`**

```cpp
#include "EnemySpawn.h"

class TiledMap {
 public:
  // ... existing methods ...
  const std::vector<EnemySpawn>& getEnemySpawns() const {
    return enemySpawns;
  }

 private:
  // ... existing fields ...
  std::vector<EnemySpawn> enemySpawns;

  void extractCollisionShapes();
  void extractMusicZones();
  void extractEnemySpawns();  // NEW
};
```

**Modified File: `src/TiledMap.cpp`**

Add extraction logic (similar to music zones):

```cpp
void TiledMap::extractEnemySpawns() {
  enemySpawns.clear();

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
          Logger::warn("Enemy spawn '" + object.getName() +
                       "' missing enemy_type property");
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
          Logger::warn("Unknown enemy_type: " + enemyTypeStr);
          continue;
        }

        // Create spawn point
        const auto& pos = object.getPosition();
        EnemySpawn spawn;
        spawn.type = type;
        spawn.x = pos.x;
        spawn.y = pos.y;
        spawn.name = object.getName();

        enemySpawns.push_back(spawn);

        Logger::info("Loaded enemy spawn: " + spawn.name +
                     " type=" + enemyTypeStr +
                     " at (" + std::to_string(spawn.x) + ", " +
                     std::to_string(spawn.y) + ")");
      }
    }
  }
}
```

**Call in TiledMap::load():**

```cpp
bool TiledMap::load(const std::string& filepath) {
  // ... existing code ...
  extractCollisionShapes();
  extractMusicZones();
  extractEnemySpawns();  // NEW
  // ...
}
```

**Update test_map.tmx:**

Add object layer "EnemySpawns" with point objects:
- Name: "spawn_1", enemy_type: "slime", position: (300, 300)
- Name: "spawn_2", enemy_type: "slime", position: (600, 600)
- Name: "spawn_3", enemy_type: "slime", position: (400, 200)

**Testing:**
- Build and run server
- Verify logs show "Loaded enemy spawn: spawn_1 type=slime at (300, 300)"
- No enemies spawned yet (Phase 2)

---

### Phase 2: Server-Side Enemy Management

**Goal**: Server spawns enemies, updates AI, manages enemy state

#### 2.1 Create EnemySystem (Server-Side)

**New File: `include/EnemySystem.h`**

```cpp
#pragma once

#include <unordered_map>
#include <vector>
#include "Enemy.h"
#include "EnemySpawn.h"
#include "Player.h"
#include "EventBus.h"

// Server-side enemy management system
// - Spawns enemies from spawn points
// - Updates enemy AI state machines
// - Handles combat (taking damage, death)
// - Broadcasts enemy state to clients
class EnemySystem {
 public:
  explicit EnemySystem(const std::vector<EnemySpawn>& spawns);

  // Spawn enemies at all spawn points
  void spawnAllEnemies();

  // Update all enemy AI (called every frame)
  void update(float deltaTime, const std::unordered_map<uint32_t, Player>& players);

  // Combat: Apply damage to enemy
  void damageEnemy(uint32_t enemyId, float damage, uint32_t attackerId);

  // Get all active enemies (for network broadcast)
  const std::unordered_map<uint32_t, Enemy>& getEnemies() const {
    return enemies;
  }

 private:
  const std::vector<EnemySpawn>& spawns;
  std::unordered_map<uint32_t, Enemy> enemies;  // enemyId -> Enemy
  uint32_t nextEnemyId;

  // AI behavior methods
  void updateEnemyAI(Enemy& enemy, const std::unordered_map<uint32_t, Player>& players, float deltaTime);
  void updateIdleState(Enemy& enemy, const std::unordered_map<uint32_t, Player>& players);
  void updateChaseState(Enemy& enemy, const std::unordered_map<uint32_t, Player>& players, float deltaTime);
  void updateAttackState(Enemy& enemy, const std::unordered_map<uint32_t, Player>& players, float deltaTime);

  // Helper: Find nearest player within range
  const Player* findNearestPlayer(const Enemy& enemy,
                                  const std::unordered_map<uint32_t, Player>& players,
                                  float maxRange) const;

  // Helper: Calculate distance between two points
  float distance(float x1, float y1, float x2, float y2) const;
};
```

**New File: `src/EnemySystem.cpp`**

```cpp
#include "EnemySystem.h"
#include "Logger.h"
#include <cmath>
#include <cassert>

EnemySystem::EnemySystem(const std::vector<EnemySpawn>& spawns)
    : spawns(spawns), nextEnemyId(1) {
  Logger::info("EnemySystem initialized with " +
               std::to_string(spawns.size()) + " spawn points");
}

void EnemySystem::spawnAllEnemies() {
  for (const auto& spawn : spawns) {
    Enemy enemy;
    enemy.id = nextEnemyId++;
    enemy.type = spawn.type;
    enemy.state = EnemyState::Idle;
    enemy.x = spawn.x;
    enemy.y = spawn.y;
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;

    // Set stats based on type (POC: only Slime)
    if (spawn.type == EnemyType::Slime) {
      enemy.maxHealth = 50.0f;
      enemy.health = 50.0f;
      enemy.damage = 10.0f;
      enemy.attackRange = 40.0f;
      enemy.detectionRange = 200.0f;
      enemy.speed = 100.0f;
    }

    enemies[enemy.id] = enemy;

    Logger::info("Spawned enemy ID=" + std::to_string(enemy.id) +
                 " type=" + std::to_string(static_cast<int>(enemy.type)) +
                 " at (" + std::to_string(enemy.x) + ", " +
                 std::to_string(enemy.y) + ")");
  }
}

void EnemySystem::update(float deltaTime,
                         const std::unordered_map<uint32_t, Player>& players) {
  for (auto& [id, enemy] : enemies) {
    // Skip dead enemies (they'll be cleaned up later)
    if (enemy.state == EnemyState::Dead) {
      continue;
    }

    updateEnemyAI(enemy, players, deltaTime);
  }

  // TODO: Clean up dead enemies after delay (Phase 3)
}

void EnemySystem::updateEnemyAI(Enemy& enemy,
                                const std::unordered_map<uint32_t, Player>& players,
                                float deltaTime) {
  switch (enemy.state) {
    case EnemyState::Idle:
      updateIdleState(enemy, players);
      break;

    case EnemyState::Chase:
      updateChaseState(enemy, players, deltaTime);
      break;

    case EnemyState::Attack:
      updateAttackState(enemy, players, deltaTime);
      break;

    case EnemyState::Dead:
      // Do nothing (already dead)
      break;
  }
}

void EnemySystem::updateIdleState(Enemy& enemy,
                                  const std::unordered_map<uint32_t, Player>& players) {
  // Look for nearest player within detection range
  const Player* nearestPlayer = findNearestPlayer(enemy, players,
                                                   enemy.detectionRange);

  if (nearestPlayer != nullptr) {
    // Found a target - transition to Chase
    enemy.targetPlayerId = nearestPlayer->id;
    enemy.state = EnemyState::Chase;

    Logger::debug("Enemy " + std::to_string(enemy.id) +
                  " detected player " + std::to_string(nearestPlayer->id) +
                  ", entering Chase state");
  } else {
    // No targets nearby, stay idle
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
  }
}

void EnemySystem::updateChaseState(Enemy& enemy,
                                   const std::unordered_map<uint32_t, Player>& players,
                                   float deltaTime) {
  // Find target player
  auto it = players.find(enemy.targetPlayerId);
  if (it == players.end()) {
    // Target player disconnected or died - return to Idle
    enemy.targetPlayerId = 0;
    enemy.state = EnemyState::Idle;
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
    return;
  }

  const Player& target = it->second;

  // Calculate distance to target
  float dist = distance(enemy.x, enemy.y, target.x, target.y);

  // Check if in attack range
  if (dist <= enemy.attackRange) {
    // Transition to Attack state
    enemy.state = EnemyState::Attack;
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
    return;
  }

  // Check if target escaped detection range
  if (dist > enemy.detectionRange * 1.2f) {  // Hysteresis: 120% of detection range
    // Lost target - return to Idle
    enemy.targetPlayerId = 0;
    enemy.state = EnemyState::Idle;
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
    return;
  }

  // Move toward target
  float dx = target.x - enemy.x;
  float dy = target.y - enemy.y;
  float dirLength = std::sqrt(dx * dx + dy * dy);

  if (dirLength > 0.001f) {
    // Normalize direction and apply speed
    float normX = dx / dirLength;
    float normY = dy / dirLength;

    enemy.vx = normX * enemy.speed;
    enemy.vy = normY * enemy.speed;

    // Update position
    enemy.x += enemy.vx * deltaTime;
    enemy.y += enemy.vy * deltaTime;
  }
}

void EnemySystem::updateAttackState(Enemy& enemy,
                                    const std::unordered_map<uint32_t, Player>& players,
                                    float deltaTime) {
  // Find target player
  auto it = players.find(enemy.targetPlayerId);
  if (it == players.end()) {
    // Target player disconnected or died - return to Idle
    enemy.targetPlayerId = 0;
    enemy.state = EnemyState::Idle;
    return;
  }

  const Player& target = it->second;

  // Calculate distance to target
  float dist = distance(enemy.x, enemy.y, target.x, target.y);

  // Check if target moved out of attack range
  if (dist > enemy.attackRange * 1.2f) {  // Hysteresis
    // Return to Chase state
    enemy.state = EnemyState::Chase;
    return;
  }

  // TODO: Implement actual attack logic (damage player)
  // For POC, enemy just stands still when in attack range
  // Attack damage will be handled in Phase 3
  enemy.vx = 0.0f;
  enemy.vy = 0.0f;
}

void EnemySystem::damageEnemy(uint32_t enemyId, float damage, uint32_t attackerId) {
  auto it = enemies.find(enemyId);
  if (it == enemies.end()) {
    Logger::warn("Attempted to damage non-existent enemy ID=" +
                 std::to_string(enemyId));
    return;
  }

  Enemy& enemy = it->second;

  // Skip if already dead
  if (enemy.state == EnemyState::Dead) {
    return;
  }

  // Apply damage
  enemy.health -= damage;

  Logger::debug("Enemy " + std::to_string(enemyId) +
                " took " + std::to_string(damage) + " damage, " +
                "health: " + std::to_string(enemy.health) + "/" +
                std::to_string(enemy.maxHealth));

  // Check if killed
  if (enemy.health <= 0.0f) {
    enemy.health = 0.0f;
    enemy.state = EnemyState::Dead;
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;

    Logger::info("Enemy " + std::to_string(enemyId) +
                 " killed by player " + std::to_string(attackerId));

    // TODO: Broadcast EnemyDiedEvent (Phase 3)
  } else {
    // TODO: Broadcast EnemyDamagedEvent (Phase 3)
  }
}

const Player* EnemySystem::findNearestPlayer(
    const Enemy& enemy,
    const std::unordered_map<uint32_t, Player>& players,
    float maxRange) const {

  const Player* nearest = nullptr;
  float nearestDist = maxRange;

  for (const auto& [id, player] : players) {
    float dist = distance(enemy.x, enemy.y, player.x, player.y);
    if (dist < nearestDist) {
      nearest = &player;
      nearestDist = dist;
    }
  }

  return nearest;
}

float EnemySystem::distance(float x1, float y1, float x2, float y2) const {
  float dx = x2 - x1;
  float dy = y2 - y1;
  return std::sqrt(dx * dx + dy * dy);
}
```

**Design Notes:**
- Simple state machine AI (Idle → Chase → Attack)
- Hysteresis prevents rapid state oscillation (120% threshold)
- Uses existing Player map from ServerGameState
- Enemies stored in unordered_map like players
- Only Slime type implemented for POC

#### 2.2 Integrate EnemySystem into Server

**Modified File: `include/ServerGameState.h`**

```cpp
#include "EnemySystem.h"

class ServerGameState {
 public:
  ServerGameState(TiledMap* map, CollisionSystem* collision);

  // ... existing methods ...

  EnemySystem* getEnemySystem() { return enemySystem.get(); }

 private:
  // ... existing fields ...
  std::unique_ptr<EnemySystem> enemySystem;  // NEW

  // ... existing methods ...
};
```

**Modified File: `src/ServerGameState.cpp`**

```cpp
#include "ServerGameState.h"

ServerGameState::ServerGameState(TiledMap* map, CollisionSystem* collision)
    : tiledMap(map), collisionSystem(collision), nextPlayerId(1) {

  // ... existing code ...

  // Initialize enemy system (NEW)
  enemySystem = std::make_unique<EnemySystem>(map->getEnemySpawns());
  enemySystem->spawnAllEnemies();

  Logger::info("ServerGameState initialized");
}
```

**Modified File: `src/ServerGameState.cpp` (update method)**

Add enemy update to the existing update logic:

```cpp
void ServerGameState::onUpdate(const UpdateEvent& e) {
  // ... existing player movement code ...

  // Update enemy AI (NEW)
  if (enemySystem) {
    enemySystem->update(e.deltaTime, players);
  }
}
```

**Testing:**
- Build and run server
- Verify logs show "Spawned enemy ID=1 type=0 at (300, 300)"
- Enemies spawn but not visible to clients yet (Phase 3)

---

### Phase 3: Network Protocol for Enemies

**Goal**: Add network packets to synchronize enemy state to clients

#### 3.1 Define Enemy Network Packets

**Modified File: `include/NetworkProtocol.h`**

Add new packet types:

```cpp
enum class PacketType : uint8_t {
  ClientInput = 0,
  StateUpdate = 1,
  PlayerJoined = 2,
  PlayerLeft = 3,
  EnemyStateUpdate = 4,   // NEW: Server → Client enemy positions
  EnemyDamaged = 5,       // NEW: Server → Client enemy took damage
  EnemyDied = 6,          // NEW: Server → Client enemy died
  AttackEnemy = 7         // NEW: Client → Server player attacked enemy
};

// Client → Server: Player attacked an enemy
struct AttackEnemyPacket {
  uint32_t enemyId;       // Which enemy was attacked
  float damage;           // How much damage (validated server-side)

  static constexpr size_t SIZE = 8;  // 4 + 4

  void serialize(uint8_t* buffer) const {
    buffer[0] = static_cast<uint8_t>(PacketType::AttackEnemy);
    serializeUint32(buffer + 1, enemyId);
    serializeFloat(buffer + 5, damage);
  }

  static AttackEnemyPacket deserialize(const uint8_t* buffer) {
    AttackEnemyPacket packet;
    packet.enemyId = deserializeUint32(buffer + 1);
    packet.damage = deserializeFloat(buffer + 5);
    return packet;
  }
};

// Server → Client: Enemy state (position, health, etc.)
struct EnemyState {
  uint32_t id;
  uint8_t type;           // EnemyType enum
  uint8_t state;          // EnemyState enum
  float x, y;
  float vx, vy;
  float health;
  float maxHealth;

  static constexpr size_t SIZE = 30;  // 4+1+1+4+4+4+4+4+4

  void serialize(uint8_t* buffer) const {
    serializeUint32(buffer, id);
    buffer[4] = type;
    buffer[5] = state;
    serializeFloat(buffer + 6, x);
    serializeFloat(buffer + 10, y);
    serializeFloat(buffer + 14, vx);
    serializeFloat(buffer + 18, vy);
    serializeFloat(buffer + 22, health);
    serializeFloat(buffer + 26, maxHealth);
  }

  static EnemyState deserialize(const uint8_t* buffer) {
    EnemyState state;
    state.id = deserializeUint32(buffer);
    state.type = buffer[4];
    state.state = buffer[5];
    state.x = deserializeFloat(buffer + 6);
    state.y = deserializeFloat(buffer + 10);
    state.vx = deserializeFloat(buffer + 14);
    state.vy = deserializeFloat(buffer + 18);
    state.health = deserializeFloat(buffer + 22);
    state.maxHealth = deserializeFloat(buffer + 26);
    return state;
  }
};

// Server → Client: Enemy state update packet
struct EnemyStateUpdatePacket {
  uint32_t enemyCount;
  std::vector<EnemyState> enemies;

  void serialize(std::vector<uint8_t>& buffer) const {
    buffer.resize(5 + enemyCount * EnemyState::SIZE);
    buffer[0] = static_cast<uint8_t>(PacketType::EnemyStateUpdate);
    serializeUint32(buffer.data() + 1, enemyCount);

    for (size_t i = 0; i < enemyCount; i++) {
      enemies[i].serialize(buffer.data() + 5 + i * EnemyState::SIZE);
    }
  }

  static EnemyStateUpdatePacket deserialize(const uint8_t* buffer, size_t length) {
    EnemyStateUpdatePacket packet;
    packet.enemyCount = deserializeUint32(buffer + 1);

    assert(length >= 5 + packet.enemyCount * EnemyState::SIZE &&
           "Invalid EnemyStateUpdate packet size");

    packet.enemies.reserve(packet.enemyCount);
    for (uint32_t i = 0; i < packet.enemyCount; i++) {
      packet.enemies.push_back(
        EnemyState::deserialize(buffer + 5 + i * EnemyState::SIZE)
      );
    }

    return packet;
  }
};

// Server → Client: Enemy was damaged
struct EnemyDamagedPacket {
  uint32_t enemyId;
  float newHealth;
  uint32_t attackerId;    // Player who dealt damage

  static constexpr size_t SIZE = 13;  // 1+4+4+4

  void serialize(uint8_t* buffer) const {
    buffer[0] = static_cast<uint8_t>(PacketType::EnemyDamaged);
    serializeUint32(buffer + 1, enemyId);
    serializeFloat(buffer + 5, newHealth);
    serializeUint32(buffer + 9, attackerId);
  }

  static EnemyDamagedPacket deserialize(const uint8_t* buffer) {
    EnemyDamagedPacket packet;
    packet.enemyId = deserializeUint32(buffer + 1);
    packet.newHealth = deserializeFloat(buffer + 5);
    packet.attackerId = deserializeUint32(buffer + 9);
    return packet;
  }
};

// Server → Client: Enemy died
struct EnemyDiedPacket {
  uint32_t enemyId;
  uint32_t killerId;      // Player who got the kill

  static constexpr size_t SIZE = 9;  // 1+4+4

  void serialize(uint8_t* buffer) const {
    buffer[0] = static_cast<uint8_t>(PacketType::EnemyDied);
    serializeUint32(buffer + 1, enemyId);
    serializeUint32(buffer + 5, killerId);
  }

  static EnemyDiedPacket deserialize(const uint8_t* buffer) {
    EnemyDiedPacket packet;
    packet.enemyId = deserializeUint32(buffer + 1);
    packet.killerId = deserializeUint32(buffer + 5);
    return packet;
  }
};
```

#### 3.2 Server Broadcasts Enemy State

**Modified File: `src/NetworkServer.cpp`**

Add enemy state broadcasting to existing state update loop:

```cpp
void NetworkServer::broadcastGameState() {
  // ... existing player state broadcast ...

  // Broadcast enemy state (NEW)
  if (gameState && gameState->getEnemySystem()) {
    const auto& enemies = gameState->getEnemySystem()->getEnemies();

    EnemyStateUpdatePacket enemyPacket;
    enemyPacket.enemyCount = static_cast<uint32_t>(enemies.size());

    for (const auto& [id, enemy] : enemies) {
      EnemyState state;
      state.id = enemy.id;
      state.type = static_cast<uint8_t>(enemy.type);
      state.state = static_cast<uint8_t>(enemy.state);
      state.x = enemy.x;
      state.y = enemy.y;
      state.vx = enemy.vx;
      state.vy = enemy.vy;
      state.health = enemy.health;
      state.maxHealth = enemy.maxHealth;

      enemyPacket.enemies.push_back(state);
    }

    std::vector<uint8_t> buffer;
    enemyPacket.serialize(buffer);

    // Broadcast to all clients
    for (const auto& [peerId, clientId] : peerToClient) {
      ENetPacket* packet = enet_packet_create(
        buffer.data(), buffer.size(), ENET_PACKET_FLAG_UNRELIABLE
      );
      enet_peer_send(peers[peerId], 0, packet);
    }
  }
}
```

**Call broadcastGameState() in update loop** (if not already called):

```cpp
void NetworkServer::onUpdate(const UpdateEvent& e) {
  // ... existing update logic ...

  // Broadcast state at 60 FPS
  broadcastGameState();
}
```

#### 3.3 Server Handles Attack Packets

**Modified File: `src/NetworkServer.cpp`**

Handle AttackEnemy packets from clients:

```cpp
void NetworkServer::onPacketReceived(ENetPeer* peer, const uint8_t* data, size_t length) {
  if (length < 1) return;

  PacketType type = static_cast<PacketType>(data[0]);

  switch (type) {
    case PacketType::ClientInput:
      // ... existing input handling ...
      break;

    case PacketType::AttackEnemy: {  // NEW
      if (length != AttackEnemyPacket::SIZE) {
        Logger::warn("Invalid AttackEnemy packet size");
        break;
      }

      AttackEnemyPacket attackPacket = AttackEnemyPacket::deserialize(data);

      // Get player ID from peer
      auto it = peerToClient.find(peer);
      if (it == peerToClient.end()) {
        break;
      }
      uint32_t playerId = it->second;

      // Validate and apply damage (server-authoritative)
      if (gameState && gameState->getEnemySystem()) {
        // TODO: Validate attack is legal (range check, cooldown, etc.)
        // For POC, just trust client damage value

        gameState->getEnemySystem()->damageEnemy(
          attackPacket.enemyId,
          attackPacket.damage,
          playerId
        );

        // Broadcast damage event to all clients
        // TODO: Send EnemyDamagedPacket (optional for POC)
      }

      break;
    }

    default:
      Logger::warn("Unknown packet type: " + std::to_string(static_cast<int>(type)));
      break;
  }
}
```

**Testing:**
- Build server and client
- Server broadcasts enemy state packets
- Clients receive packets (not processed yet - Phase 4)

---

### Phase 4: Client-Side Enemy Rendering

**Goal**: Clients receive enemy state, interpolate positions, render enemies

#### 4.1 Client Enemy Interpolation System

**New File: `include/EnemyInterpolation.h`**

```cpp
#pragma once

#include <unordered_map>
#include <deque>
#include "Enemy.h"
#include "AnimationSystem.h"
#include "NetworkProtocol.h"

struct EnemySnapshot {
  float x, y;
  float vx, vy;
  float health;
  uint8_t state;
  uint64_t timestamp;  // Milliseconds since epoch
};

// Client-side enemy state interpolation
// Mirrors RemotePlayerInterpolation pattern
class EnemyInterpolation {
 public:
  explicit EnemyInterpolation(AnimationSystem* animSystem);

  // Update enemy state from network packet
  void updateEnemyState(const EnemyState& state);

  // Remove enemy (when died or despawned)
  void removeEnemy(uint32_t enemyId);

  // Get interpolated enemy state for rendering
  bool getInterpolatedState(uint32_t enemyId, float interpolation, Enemy& outEnemy) const;

  // Get all enemy IDs
  std::vector<uint32_t> getEnemyIds() const;

 private:
  AnimationSystem* animationSystem;
  std::unordered_map<uint32_t, Enemy> enemies;
  std::unordered_map<uint32_t, std::deque<EnemySnapshot>> snapshots;

  static constexpr size_t MAX_SNAPSHOTS = 3;  // Same as RemotePlayerInterpolation
};
```

**New File: `src/EnemyInterpolation.cpp`**

```cpp
#include "EnemyInterpolation.h"
#include "Logger.h"
#include "AnimationAssetLoader.h"
#include <SDL2/SDL.h>

EnemyInterpolation::EnemyInterpolation(AnimationSystem* animSystem)
    : animationSystem(animSystem) {
  Logger::info("EnemyInterpolation initialized");
}

void EnemyInterpolation::updateEnemyState(const EnemyState& state) {
  uint32_t enemyId = state.id;

  // Create enemy if first time seeing it
  if (enemies.find(enemyId) == enemies.end()) {
    Enemy enemy;
    enemy.id = state.id;
    enemy.type = static_cast<EnemyType>(state.type);
    enemy.state = static_cast<EnemyState>(state.state);
    enemy.x = state.x;
    enemy.y = state.y;
    enemy.vx = state.vx;
    enemy.vy = state.vy;
    enemy.health = state.health;
    enemy.maxHealth = state.maxHealth;

    // Load animations (POC: same as player for now)
    AnimationAssetLoader::loadPlayerAnimations(enemy.animController,
                                                "assets/player_animated.png");

    // Register with animation system
    animationSystem->registerEntity(&enemies[enemyId]);

    enemies[enemyId] = enemy;

    Logger::info("Added new enemy ID=" + std::to_string(enemyId) +
                 " type=" + std::to_string(static_cast<int>(enemy.type)));
  }

  // Update enemy state
  Enemy& enemy = enemies[enemyId];
  enemy.state = static_cast<EnemyState>(state.state);
  enemy.health = state.health;
  enemy.maxHealth = state.maxHealth;

  // Add snapshot for interpolation
  EnemySnapshot snapshot;
  snapshot.x = state.x;
  snapshot.y = state.y;
  snapshot.vx = state.vx;
  snapshot.vy = state.vy;
  snapshot.health = state.health;
  snapshot.state = state.state;
  snapshot.timestamp = SDL_GetTicks64();

  auto& queue = snapshots[enemyId];
  queue.push_back(snapshot);

  if (queue.size() > MAX_SNAPSHOTS) {
    queue.pop_front();
  }
}

void EnemyInterpolation::removeEnemy(uint32_t enemyId) {
  auto it = enemies.find(enemyId);
  if (it != enemies.end()) {
    animationSystem->unregisterEntity(&it->second);
    enemies.erase(it);
  }
  snapshots.erase(enemyId);

  Logger::info("Removed enemy ID=" + std::to_string(enemyId));
}

bool EnemyInterpolation::getInterpolatedState(uint32_t enemyId,
                                              float interpolation,
                                              Enemy& outEnemy) const {
  auto it = enemies.find(enemyId);
  if (it == enemies.end()) {
    return false;
  }

  const Enemy& enemy = it->second;

  auto snapIt = snapshots.find(enemyId);
  if (snapIt == snapshots.end() || snapIt->second.size() < 2) {
    // Not enough snapshots, return current state
    outEnemy = enemy;
    return true;
  }

  // Interpolate between last two snapshots
  const auto& queue = snapIt->second;
  const EnemySnapshot& prev = queue[queue.size() - 2];
  const EnemySnapshot& curr = queue[queue.size() - 1];

  outEnemy = enemy;
  outEnemy.x = prev.x + (curr.x - prev.x) * interpolation;
  outEnemy.y = prev.y + (curr.y - prev.y) * interpolation;
  outEnemy.vx = curr.vx;
  outEnemy.vy = curr.vy;

  return true;
}

std::vector<uint32_t> EnemyInterpolation::getEnemyIds() const {
  std::vector<uint32_t> ids;
  ids.reserve(enemies.size());
  for (const auto& [id, enemy] : enemies) {
    ids.push_back(id);
  }
  return ids;
}
```

#### 4.2 Client Processes Enemy Packets

**Modified File: `src/NetworkClient.cpp`**

Handle incoming EnemyStateUpdate packets:

```cpp
void NetworkClient::onPacketReceived(const uint8_t* data, size_t length) {
  if (length < 1) return;

  PacketType type = static_cast<PacketType>(data[0]);

  switch (type) {
    case PacketType::StateUpdate:
      // ... existing player state handling ...
      break;

    case PacketType::EnemyStateUpdate: {  // NEW
      EnemyStateUpdatePacket packet = EnemyStateUpdatePacket::deserialize(data, length);

      for (const auto& enemyState : packet.enemies) {
        // Update interpolation system (set in client_main.cpp)
        if (enemyInterpolation) {
          enemyInterpolation->updateEnemyState(enemyState);
        }
      }

      break;
    }

    case PacketType::EnemyDied: {  // NEW
      EnemyDiedPacket packet = EnemyDiedPacket::deserialize(data);

      if (enemyInterpolation) {
        enemyInterpolation->removeEnemy(packet.enemyId);
      }

      Logger::info("Enemy " + std::to_string(packet.enemyId) +
                   " was killed by player " + std::to_string(packet.killerId));

      break;
    }

    default:
      // ... existing default case ...
      break;
  }
}
```

**Modified File: `include/NetworkClient.h`**

Add reference to EnemyInterpolation:

```cpp
#include "EnemyInterpolation.h"

class NetworkClient {
 public:
  // ... existing methods ...

  void setEnemyInterpolation(EnemyInterpolation* interp) {
    enemyInterpolation = interp;
  }

 private:
  // ... existing fields ...
  EnemyInterpolation* enemyInterpolation;  // NEW
};
```

#### 4.3 Render Enemies

**Modified File: `src/RenderSystem.cpp`**

Add enemy rendering to onRender():

```cpp
void RenderSystem::onRender(const RenderEvent& e) {
  // ... existing player rendering ...

  // Gather all players
  std::vector<Player> allPlayers;
  allPlayers.push_back(localPlayer);
  // ... add remote players ...

  // Gather all enemies (NEW)
  std::vector<Enemy> allEnemies;
  if (enemyInterpolation) {
    auto enemyIds = enemyInterpolation->getEnemyIds();
    for (uint32_t enemyId : enemyIds) {
      Enemy enemy;
      if (enemyInterpolation->getInterpolatedState(enemyId, e.interpolation, enemy)) {
        // Only render alive enemies
        if (enemy.state != EnemyState::Dead) {
          allEnemies.push_back(enemy);
        }
      }
    }
  }

  // Render tiles with depth-sorted players AND enemies
  tileRenderer->render(*tiledMap, [&](float minDepth, float maxDepth) {
    // Render players
    for (const Player& player : allPlayers) {
      float playerDepth = player.x + player.y;
      if (playerDepth >= minDepth && playerDepth < maxDepth) {
        drawPlayer(player);
      }
    }

    // Render enemies (NEW)
    for (const Enemy& enemy : allEnemies) {
      float enemyDepth = enemy.x + enemy.y;
      if (enemyDepth >= minDepth && enemyDepth < maxDepth) {
        drawEnemy(enemy);
      }
    }
  });

  // ... rest of rendering ...
}

void RenderSystem::drawEnemy(const Enemy& enemy) {
  int screenX, screenY;
  camera->worldToScreen(enemy.x, enemy.y, screenX, screenY);

  // Render enemy sprite (for POC, use player sprite with red tint)
  const AnimationController* controller = enemy.getAnimationController();
  if (controller && playerTexture) {
    int srcX, srcY, srcW, srcH;
    controller->getCurrentFrame(srcX, srcY, srcW, srcH);

    // Red tint for enemies
    spriteRenderer->drawRegion(
        *playerTexture, screenX - Config::Player::SIZE / 2.0f,
        screenY - Config::Player::SIZE / 2.0f, Config::Player::SIZE,
        Config::Player::SIZE, srcX, srcY, srcW, srcH,
        1.0f, 0.3f, 0.3f, 1.0f  // Red tint (R=1.0, G=0.3, B=0.3)
    );
  }

  // Render health bar (NEW)
  drawHealthBar(screenX, screenY - 20, enemy.health, enemy.maxHealth);
}

void RenderSystem::drawHealthBar(int x, int y, float health, float maxHealth) {
  constexpr int BAR_WIDTH = 32;
  constexpr int BAR_HEIGHT = 4;

  float healthPercent = health / maxHealth;
  int filledWidth = static_cast<int>(BAR_WIDTH * healthPercent);

  // Background (black)
  spriteRenderer->draw(
      *whitePixelTexture, x - BAR_WIDTH / 2, y,
      BAR_WIDTH, BAR_HEIGHT, 0.0f, 0.0f, 0.0f, 0.8f
  );

  // Foreground (green to red gradient based on health)
  float r = 1.0f - healthPercent;  // More red when low health
  float g = healthPercent;         // More green when high health

  spriteRenderer->draw(
      *whitePixelTexture, x - BAR_WIDTH / 2, y,
      filledWidth, BAR_HEIGHT, r, g, 0.0f, 1.0f
  );
}
```

**Modified File: `include/RenderSystem.h`**

```cpp
class RenderSystem {
 public:
  RenderSystem(Window* window, ClientPrediction* clientPrediction,
               RemotePlayerInterpolation* remoteInterpolation,
               EnemyInterpolation* enemyInterpolation,  // NEW
               Camera* camera, TiledMap* tiledMap,
               CollisionDebugRenderer* collisionDebugRenderer,
               MusicZoneDebugRenderer* musicZoneDebugRenderer);

 private:
  // ... existing fields ...
  EnemyInterpolation* enemyInterpolation;  // NEW

  void drawPlayer(const Player& player);
  void drawEnemy(const Enemy& enemy);      // NEW
  void drawHealthBar(int x, int y, float health, float maxHealth);  // NEW

  // ... rest of class ...
};
```

#### 4.4 Integrate into Client Main

**Modified File: `src/client_main.cpp`**

```cpp
#include "EnemyInterpolation.h"

int main() {
  // ... existing initialization ...

  // Create animation system
  AnimationSystem animationSystem;

  // Create enemy interpolation (NEW)
  EnemyInterpolation enemyInterpolation(&animationSystem);

  // Set enemy interpolation on network client (NEW)
  networkClient.setEnemyInterpolation(&enemyInterpolation);

  // Create remote player interpolation
  RemotePlayerInterpolation remoteInterpolation(localPlayerId, &animationSystem);

  // Create render system with enemy interpolation (NEW)
  RenderSystem renderSystem(&window, &clientPrediction, &remoteInterpolation,
                            &enemyInterpolation,  // NEW parameter
                            &camera, &tiledMap,
                            &collisionDebugRenderer, &musicZoneDebugRenderer);

  // ... rest of main() ...
}
```

**Testing:**
- Build and run `/dev` (1 server + 4 clients)
- Enemies spawn at spawn points
- Enemies render as red-tinted sprites
- Enemies have health bars
- Enemies chase nearby players (AI visible)

---

### Phase 5: Player Combat (Attack Enemies)

**Goal**: Players can attack and damage enemies

#### 5.1 Client Input for Attack

**Modified File: `src/InputSystem.cpp`**

Add attack input (e.g., Spacebar):

```cpp
void InputSystem::onKeyDown(const KeyDownEvent& e) {
  switch (e.key) {
    case SDLK_LEFT:
    case SDLK_a:
      leftPressed = true;
      break;
    // ... existing keys ...

    case SDLK_SPACE:  // NEW: Attack key
      attackPressed = true;
      break;
  }
}

void InputSystem::onUpdate(const UpdateEvent& e) {
  // ... existing input processing ...

  // Check for attack (NEW)
  if (attackPressed) {
    attackPressed = false;  // One-shot (no repeat)

    // Publish attack event
    EventBus::instance().publish(AttackInputEvent{});
  }
}
```

**New Event: `include/EventBus.h`**

```cpp
struct AttackInputEvent {};
```

**Modified File: `include/InputSystem.h`**

```cpp
class InputSystem {
 private:
  // ... existing fields ...
  bool attackPressed;  // NEW
};
```

#### 5.2 Client Sends Attack Packets

**New File: `include/CombatSystem.h`** (Client-side)

```cpp
#pragma once

#include "EventBus.h"
#include "NetworkClient.h"
#include "ClientPrediction.h"
#include "EnemyInterpolation.h"

// Client-side combat system
// Handles attack input and sends attack packets to server
class CombatSystem {
 public:
  CombatSystem(NetworkClient* netClient,
               ClientPrediction* prediction,
               EnemyInterpolation* enemyInterp);

 private:
  NetworkClient* networkClient;
  ClientPrediction* clientPrediction;
  EnemyInterpolation* enemyInterpolation;

  static constexpr float ATTACK_RANGE = 50.0f;
  static constexpr float ATTACK_DAMAGE = 25.0f;

  void onAttackInput(const AttackInputEvent& e);

  // Find nearest enemy within attack range
  uint32_t findNearestEnemy(float playerX, float playerY, float maxRange) const;

  float distance(float x1, float y1, float x2, float y2) const;
};
```

**New File: `src/CombatSystem.cpp`**

```cpp
#include "CombatSystem.h"
#include "Logger.h"
#include "Player.h"
#include <cmath>

CombatSystem::CombatSystem(NetworkClient* netClient,
                           ClientPrediction* prediction,
                           EnemyInterpolation* enemyInterp)
    : networkClient(netClient),
      clientPrediction(prediction),
      enemyInterpolation(enemyInterp) {

  // Subscribe to attack input
  EventBus::instance().subscribe<AttackInputEvent>(
      [this](const AttackInputEvent& e) { onAttackInput(e); });

  Logger::info("CombatSystem initialized");
}

void CombatSystem::onAttackInput(const AttackInputEvent& e) {
  const Player& player = clientPrediction->getLocalPlayer();

  // Find nearest enemy within attack range
  uint32_t enemyId = findNearestEnemy(player.x, player.y, ATTACK_RANGE);

  if (enemyId == 0) {
    // No enemies in range
    Logger::debug("Attack missed - no enemies in range");
    return;
  }

  // Send attack packet to server
  AttackEnemyPacket packet;
  packet.enemyId = enemyId;
  packet.damage = ATTACK_DAMAGE;

  uint8_t buffer[AttackEnemyPacket::SIZE];
  packet.serialize(buffer);

  networkClient->sendPacket(buffer, AttackEnemyPacket::SIZE);

  Logger::debug("Attacked enemy ID=" + std::to_string(enemyId) +
                " damage=" + std::to_string(ATTACK_DAMAGE));
}

uint32_t CombatSystem::findNearestEnemy(float playerX, float playerY,
                                        float maxRange) const {
  auto enemyIds = enemyInterpolation->getEnemyIds();

  uint32_t nearestId = 0;
  float nearestDist = maxRange;

  for (uint32_t id : enemyIds) {
    Enemy enemy;
    if (enemyInterpolation->getInterpolatedState(id, 0.0f, enemy)) {
      // Skip dead enemies
      if (enemy.state == EnemyState::Dead) {
        continue;
      }

      float dist = distance(playerX, playerY, enemy.x, enemy.y);
      if (dist < nearestDist) {
        nearestId = id;
        nearestDist = dist;
      }
    }
  }

  return nearestId;
}

float CombatSystem::distance(float x1, float y1, float x2, float y2) const {
  float dx = x2 - x1;
  float dy = y2 - y1;
  return std::sqrt(dx * dx + dy * dy);
}
```

**Modified File: `src/client_main.cpp`**

```cpp
#include "CombatSystem.h"

int main() {
  // ... existing initialization ...

  // Create combat system (after network client and enemy interpolation)
  CombatSystem combatSystem(&networkClient, &clientPrediction, &enemyInterpolation);

  // ... rest of main() ...
}
```

**Testing:**
- Build and run `/dev`
- Walk near enemy, press Spacebar
- Verify logs show "Attacked enemy ID=1 damage=25.0"
- Enemy health bar decreases
- Enemy dies after 2 hits (50 health / 25 damage)

---

### Phase 6: Polish and Testing

**Goal**: Add final polish, test all features, create playable demo

#### 6.1 Enemy Assets

**For POC:**
- Reuse player sprite with red tint (already implemented)
- Health bars render correctly

**Future:**
- Create slime_animated.png sprite sheet (32x32 frames, 8 directions)
- Update EnemyInterpolation to load enemy-specific sprites

#### 6.2 Test Map Updates

**Modified File: `assets/test_map.tmx`**

Add more enemy spawns for testing:
- 5-10 spawn points scattered around map
- Mix of isolated spawns and clusters

Example spawn configuration:
```
spawn_1: (300, 300) - Near starting area
spawn_2: (600, 600) - Far corner
spawn_3: (400, 200) - North area
spawn_4: (500, 500) - Center area
spawn_5: (700, 300) - East area
```

#### 6.3 Combat Balance Tuning

Create config file for easy balance tweaking:

**New File: `include/config/CombatConfig.h`**

```cpp
#pragma once

namespace Config {
namespace Combat {

// Player combat stats
constexpr float PLAYER_ATTACK_RANGE = 50.0f;
constexpr float PLAYER_ATTACK_DAMAGE = 25.0f;
constexpr float PLAYER_ATTACK_COOLDOWN = 0.5f;  // Seconds between attacks

// Enemy stats (Slime)
constexpr float SLIME_MAX_HEALTH = 50.0f;
constexpr float SLIME_DAMAGE = 10.0f;
constexpr float SLIME_ATTACK_RANGE = 40.0f;
constexpr float SLIME_DETECTION_RANGE = 200.0f;
constexpr float SLIME_SPEED = 100.0f;  // pixels/second
constexpr float SLIME_ATTACK_COOLDOWN = 1.0f;  // Seconds between attacks

}  // namespace Combat
}  // namespace Config
```

Update EnemySystem and CombatSystem to use these constants.

#### 6.4 Comprehensive Testing

**Unit Tests:**

**New File: `tests/test_enemy_system.cpp`**
- Test enemy AI state transitions
- Test damage calculation
- Test spawn point extraction

**New File: `tests/test_combat_system.cpp`**
- Test attack range detection
- Test nearest enemy finding
- Test combat packet serialization

**Integration Tests:**

1. **Single Player vs Enemy:**
   - Spawn 1 player, 1 enemy
   - Walk toward enemy (verify Chase state)
   - Attack enemy twice (verify death)
   - Verify enemy despawns

2. **Multiplayer Co-op:**
   - Spawn 4 players, 5 enemies
   - Players attack different enemies
   - Verify kill credit goes to correct player
   - Verify all clients see consistent state

3. **Performance Test:**
   - Spawn 20 enemies
   - All 4 players fighting simultaneously
   - Verify 60 FPS maintained
   - Check network bandwidth usage

4. **Edge Cases:**
   - Player disconnects mid-combat (enemy loses target)
   - Enemy killed by two players simultaneously (first hit wins)
   - Rapid attack spam (cooldown prevents)

---

## File Changes Summary

### New Files (14 total)

**Phase 1: Enemy Data**
1. `include/Enemy.h` - Enemy entity struct
2. `include/EnemySpawn.h` - Spawn point data

**Phase 2: Server Systems**
3. `include/EnemySystem.h` - Server-side enemy management
4. `src/EnemySystem.cpp` - AI and combat logic

**Phase 4: Client Systems**
5. `include/EnemyInterpolation.h` - Client-side enemy state
6. `src/EnemyInterpolation.cpp` - Position interpolation

**Phase 5: Combat**
7. `include/CombatSystem.h` - Client-side combat input
8. `src/CombatSystem.cpp` - Attack packet sending

**Phase 6: Config & Tests**
9. `include/config/CombatConfig.h` - Combat balance constants
10. `tests/test_enemy_system.cpp` - Enemy AI unit tests
11. `tests/test_combat_system.cpp` - Combat unit tests

**Assets:**
12. `assets/slime_animated.png` - Enemy sprite (future)
13. `assets/test_map.tmx` (updated) - Enemy spawn points

### Modified Files (12 total)

**Phase 1:**
1. `include/TiledMap.h` - Add getEnemySpawns()
2. `src/TiledMap.cpp` - Extract spawn points from Tiled

**Phase 2:**
3. `include/ServerGameState.h` - Add EnemySystem member
4. `src/ServerGameState.cpp` - Initialize and update enemies

**Phase 3:**
5. `include/NetworkProtocol.h` - Add enemy packet types
6. `src/NetworkServer.cpp` - Broadcast enemy state, handle attacks

**Phase 4:**
7. `include/NetworkClient.h` - Add EnemyInterpolation reference
8. `src/NetworkClient.cpp` - Process enemy packets
9. `include/RenderSystem.h` - Add drawEnemy(), drawHealthBar()
10. `src/RenderSystem.cpp` - Render enemies with health bars

**Phase 5:**
11. `src/InputSystem.cpp` - Add attack input (Spacebar)
12. `src/client_main.cpp` - Initialize CombatSystem, EnemyInterpolation

**Phase 6:**
13. `CMakeLists.txt` - Add new source files to build

---

## Network Protocol Summary

### New Packet Types (4 total)

| Packet Type | Direction | Size | Description |
|-------------|-----------|------|-------------|
| **AttackEnemy** | Client → Server | 9 bytes | Player attacked enemy |
| **EnemyStateUpdate** | Server → Client | 5 + 30N bytes | Enemy positions, health (all enemies) |
| **EnemyDamaged** | Server → Client | 13 bytes | Enemy took damage (optional for POC) |
| **EnemyDied** | Server → Client | 9 bytes | Enemy was killed |

### Bandwidth Estimate (60 FPS)

- **EnemyStateUpdate**: 5 + 30 * 10 = 305 bytes per packet
- **Frequency**: 60 packets/second
- **Bandwidth**: ~18 KB/s per client (10 enemies)
- **Total**: ~72 KB/s for 4 clients (acceptable for LAN/localhost)

Future optimization: Delta compression, only send changed enemies

---

## Performance Considerations

### Server Load

- **Enemy AI**: O(N * M) per frame (N = enemies, M = players)
  - 10 enemies, 4 players = 40 distance checks per frame
  - Cost: ~100 microseconds at 60 FPS (negligible)

- **Network**: Broadcasting full enemy state every frame
  - Optimization: Only send enemies near players (spatial partitioning)

### Client Load

- **Rendering**: 10 enemies + 4 players = 14 sprites per frame
  - Depth sorting: O(N log N) = ~4 microseconds
  - Rendering: ~200 microseconds (negligible)

- **Interpolation**: 3-snapshot buffer per enemy
  - Memory: 10 enemies * 3 snapshots * 32 bytes = ~1 KB (negligible)

---

## Success Criteria

**Phase 1:**
- ✅ Enemy struct defined with Animatable interface
- ✅ Spawn points extracted from Tiled map
- ✅ Logs show spawn points loaded

**Phase 2:**
- ✅ Enemies spawn at spawn points on server
- ✅ Enemy AI state machine works (Idle → Chase → Attack)
- ✅ Enemies chase nearest player
- ✅ Logs show AI state transitions

**Phase 3:**
- ✅ Server broadcasts enemy state packets
- ✅ Server handles AttackEnemy packets
- ✅ Server validates and applies damage
- ✅ Enemies die when health reaches 0

**Phase 4:**
- ✅ Clients receive and interpolate enemy state
- ✅ Enemies render with red tint
- ✅ Health bars show enemy health
- ✅ Smooth interpolation (no jittering)

**Phase 5:**
- ✅ Spacebar attacks nearest enemy
- ✅ Attack packets sent to server
- ✅ Enemy health decreases on hit
- ✅ Enemy dies after enough damage
- ✅ Dead enemies despawn

**Phase 6:**
- ✅ Playable 4-player co-op demo
- ✅ Multiple enemies spawned
- ✅ Players can fight enemies together
- ✅ 60 FPS maintained with 10+ enemies
- ✅ All unit tests passing

---

## Future Enhancements (Post-POC)

### Immediate Next Steps
1. **UI System (ImGui)** - Inventory, quest log, settings
2. **Loot System** - Enemies drop items on death
3. **Player Damage** - Enemies can hurt players (health system)
4. **Death & Respawn** - Players respawn at spawn point when killed

### Medium-Term Features
1. **More Enemy Types** - Goblin (ranged), Skeleton (fast melee)
2. **Enemy Abilities** - Special attacks, area effects
3. **Boss Enemies** - Large health pool, multi-phase fights
4. **Advanced AI** - Pathfinding (A*), formations, tactics

### Long-Term Features
1. **Procedural Spawning** - Spawn waves based on difficulty
2. **Quest System** - Kill X enemies, clear area objectives
3. **Particle Effects** - Hit effects, death animations
4. **Sound Effects** - Attack sounds, enemy death sounds
5. **Engine/Game Separation** - Extract reusable engine code

---

## Implementation Timeline Estimate

| Phase | Estimated Time | Cumulative |
|-------|----------------|------------|
| Phase 1: Enemy Data & Spawn | 2 hours | 2 hours |
| Phase 2: Server Enemy System | 3 hours | 5 hours |
| Phase 3: Network Protocol | 2 hours | 7 hours |
| Phase 4: Client Rendering | 3 hours | 10 hours |
| Phase 5: Combat System | 2 hours | 12 hours |
| Phase 6: Polish & Testing | 3 hours | 15 hours |

**Total Estimate**: ~15 hours of focused development

---

## Critical Files for Implementation

**Most Important (read first):**
- `/Users/mkornfield/dev/gambit/include/Player.h` - Pattern for Enemy struct
- `/Users/mkornfield/dev/gambit/src/AnimationSystem.cpp` - Pattern for EnemySystem
- `/Users/mkornfield/dev/gambit/src/RemotePlayerInterpolation.cpp` - Pattern for EnemyInterpolation
- `/Users/mkornfield/dev/gambit/include/NetworkProtocol.h` - Existing packet patterns
- `/Users/mkornfield/dev/gambit/src/TiledMap.cpp` - extractMusicZones() pattern for spawn extraction

**Server-Side:**
- `/Users/mkornfield/dev/gambit/include/ServerGameState.h` - Where to add EnemySystem
- `/Users/mkornfield/dev/gambit/src/NetworkServer.cpp` - Where to broadcast enemy state

**Client-Side:**
- `/Users/mkornfield/dev/gambit/src/client_main.cpp` - Initialization order
- `/Users/mkornfield/dev/gambit/src/RenderSystem.cpp` - Rendering pipeline
- `/Users/mkornfield/dev/gambit/src/NetworkClient.cpp` - Packet processing

---

Ready to implement! This plan creates a fully playable 4-player co-op demo with enemies, AI, and combat.
