#pragma once

#include <unordered_map>
#include <vector>

#include "Enemy.h"
#include "EnemySpawn.h"
#include "EventBus.h"
#include "Player.h"

class EffectManager;

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
  void update(float deltaTime, std::unordered_map<uint32_t, Player>& players,
              EffectManager* effectManager);

  // Combat: Apply damage to enemy
  void damageEnemy(uint32_t enemyId, float damage, uint32_t attackerId);

  // Get all active enemies (for network broadcast and effects)
  std::unordered_map<uint32_t, Enemy>& getEnemies() { return enemies; }
  const std::unordered_map<uint32_t, Enemy>& getEnemies() const {
    return enemies;
  }

  // Enemy death tracking (for broadcasting death events)
  struct EnemyDeath {
    uint32_t enemyId;
    uint32_t killerId;
  };

  // Get enemies that died this frame (cleared each update)
  const std::vector<EnemyDeath>& getDiedThisFrame() const {
    return diedThisFrame;
  }

  // Record an enemy death (for DoT and other non-combat deaths)
  void recordDeath(uint32_t enemyId, uint32_t killerId) {
    diedThisFrame.push_back({enemyId, killerId});
  }

 private:
  const std::vector<EnemySpawn>& spawns;
  std::unordered_map<uint32_t, Enemy> enemies;  // enemyId -> Enemy
  uint32_t nextEnemyId;
  std::vector<EnemyDeath> diedThisFrame;  // Cleared each update
  float accumulatedTime;                  // Milliseconds since server start

  // AI behavior methods
  void updateEnemyAI(Enemy& enemy,
                     std::unordered_map<uint32_t, Player>& players,
                     float deltaTime, EffectManager* effectManager);
  void updateIdleState(Enemy& enemy,
                       const std::unordered_map<uint32_t, Player>& players);
  void updateChaseState(Enemy& enemy,
                        const std::unordered_map<uint32_t, Player>& players,
                        float deltaTime, EffectManager* effectManager);
  void updateAttackState(Enemy& enemy,
                         std::unordered_map<uint32_t, Player>& players,
                         float deltaTime, EffectManager* effectManager);

  // Helper: Find nearest player within range
  const Player* findNearestPlayer(
      const Enemy& enemy, const std::unordered_map<uint32_t, Player>& players,
      float maxRange) const;

  // Helper: Calculate distance between two points
  float distance(float x1, float y1, float x2, float y2) const;
};
