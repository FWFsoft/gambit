#pragma once

#include <functional>
#include <random>
#include <unordered_map>
#include <vector>

#include "Enemy.h"
#include "EnemySpawn.h"
#include "EventBus.h"
#include "Player.h"

class AbilitySystem;
class EffectManager;

// Server-side enemy management.
// - Spawns enemies (static map spawns and on demand for the match director)
// - Runs the per-tier AI: swarmers bite, casters shoot, brutes slam, the boss
//   cycles through set pieces. Every attack is telegraphed with a windup so it
//   can be dodged.
// - Applies damage and tracks deaths for the rest of the server to react to.
class EnemySystem {
 public:
  explicit EnemySystem(const std::vector<EnemySpawn>& spawns);

  // Spawn enemies at all static map spawn points (camp guards).
  void spawnAllEnemies();

  struct SpawnOptions {
    bool targetShip = false;
    bool elite = false;
    bool campGuard = false;
    float healthScale = 1.0f;
    float damageScale = 1.0f;
  };

  // Spawn one enemy on demand. Returns its id.
  uint32_t spawn(Arena::EnemyKind kind, float x, float y,
                 const SpawnOptions& options);
  uint32_t spawn(Arena::EnemyKind kind, float x, float y) {
    return spawn(kind, x, y, SpawnOptions());
  }

  // Remove enemies without awarding kills (round end). Camp guards placed
  // by the map stay unless includeCampGuards is set.
  void despawnAll(bool includeCampGuards = false);

  // Wiring. The combat system resolves damage/effects; the ship callback lets
  // enemies chew on the ship.
  void setCombat(AbilitySystem* combatSystem) { combat = combatSystem; }
  void setShip(float x, float y, bool attackable) {
    shipX = x;
    shipY = y;
    shipAttackable = attackable;
  }
  void setShipDamageCallback(std::function<void(float)> callback) {
    onShipDamaged = std::move(callback);
  }

  // Update all enemy AI (called every frame). deltaTime in milliseconds.
  void update(float deltaTime, std::unordered_map<uint32_t, Player>& players,
              EffectManager* effectManager);

  // Combat: apply already-modified damage to an enemy.
  void damageEnemy(uint32_t enemyId, float damage, uint32_t attackerId);

  // Shove an enemy (knockback / pull). Distance in pixels over durationMs.
  void pushEnemy(Enemy& enemy, float dirX, float dirY, float distance,
                 float durationMs);

  std::unordered_map<uint32_t, Enemy>& getEnemies() { return enemies; }
  const std::unordered_map<uint32_t, Enemy>& getEnemies() const {
    return enemies;
  }
  Enemy* find(uint32_t enemyId);
  int livingCount() const;

  struct EnemyDeath {
    uint32_t enemyId;
    uint32_t killerId;
  };

  // Deaths since the last call (damage, DoT, anything). The caller owns the
  // reaction: score, loot, objectives, broadcasting.
  std::vector<EnemyDeath> takeDeaths();

  // Deaths recorded during the current frame (kept for existing callers).
  const std::vector<EnemyDeath>& getDiedThisFrame() const {
    return pendingDeaths;
  }

  // Record an enemy death (for DoT and other non-combat deaths)
  void recordDeath(uint32_t enemyId, uint32_t killerId) {
    pendingDeaths.push_back({enemyId, killerId});
  }

  // Disable enemy spawns within a radius (for CaptureOutpost completion)
  void disableSpawnsInRadius(float x, float y, float radius);

  float nowMs() const { return accumulatedTime; }

 private:
  struct DisabledZone {
    float x, y, radius;
  };
  std::vector<DisabledZone> disabledZones;

  const std::vector<EnemySpawn>& spawns;
  std::unordered_map<uint32_t, Enemy> enemies;  // enemyId -> Enemy
  uint32_t nextEnemyId;
  std::vector<EnemyDeath> pendingDeaths;
  float accumulatedTime;  // Milliseconds since server start

  AbilitySystem* combat = nullptr;
  float shipX = 0.0f, shipY = 0.0f;
  bool shipAttackable = false;
  std::function<void(float)> onShipDamaged;
  std::mt19937 rng{1337};

  struct Target {
    bool valid = false;
    bool isShip = false;
    bool isEnemy = false;  // confused enemies fight each other
    uint32_t id = 0;
    float x = 0.0f, y = 0.0f;
    float radius = 0.0f;
  };

  void updateEnemy(Enemy& enemy, std::unordered_map<uint32_t, Player>& players,
                   float deltaTime, EffectManager* effectManager);
  Target pickTarget(const Enemy& enemy,
                    const std::unordered_map<uint32_t, Player>& players,
                    bool confused) const;
  void beginAttack(Enemy& enemy, const Target& target);
  void resolveAttack(Enemy& enemy,
                     std::unordered_map<uint32_t, Player>& players,
                     EffectManager* effectManager);
  void resolveBossAttack(Enemy& enemy,
                         std::unordered_map<uint32_t, Player>& players);
  void separate(Enemy& enemy);
  void kill(Enemy& enemy, uint32_t killerId);

  float distance(float x1, float y1, float x2, float y2) const;
};
