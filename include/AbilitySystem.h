#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

#include "ArenaData.h"
#include "Effect.h"
#include "Enemy.h"
#include "NetworkProtocol.h"
#include "Player.h"

class AbilitySystem;
class CollisionSystem;
class EffectManager;
class EnemySystem;

// Bit flags describing a hit. They gate on-hit procs so effects cannot
// trigger each other forever.
namespace DamageFlag {
constexpr uint32_t None = 0;
constexpr uint32_t Basic = 1 << 0;      // came from the primary attack
constexpr uint32_t Core = 1 << 1;       // came from a core (infusion) ability
constexpr uint32_t Dot = 1 << 2;        // damage over time tick
constexpr uint32_t Proc = 1 << 3;       // triggered by another hit: no procs
constexpr uint32_t ForceCrit = 1 << 4;  // guaranteed critical
constexpr uint32_t Melee = 1 << 5;      // attacker was adjacent
constexpr uint32_t Explosion = 1 << 6;
}  // namespace DamageFlag

// What a kit gets when one of its abilities fires.
struct AbilityContext {
  AbilitySystem& sys;
  Player& self;
  float aimX, aimY;  // cursor in world space
  float dirX, dirY;  // unit vector from the player to the cursor
  float charge;      // 0-1 for Charge abilities, 0 otherwise
};

// A character's behaviour. One instance per character type; all per-player
// state lives in Player::kitTimer / kitCounter / kitTarget.
class Kit {
 public:
  virtual ~Kit() = default;

  // Tap: on press. Charge: on release. Channel: every interval while held.
  virtual void cast(AbilityContext& ctx, int slot) = 0;
  // Called every tick while a Charge ability is being held.
  virtual void chargeTick(AbilityContext& ctx, int slot, float dtMs) {
    (void)ctx;
    (void)slot;
    (void)dtMs;
  }
  // Called when a Channel ability is released.
  virtual void channelEnd(AbilitySystem& sys, Player& self, int slot) {
    (void)sys;
    (void)self;
    (void)slot;
  }
  virtual void tick(AbilitySystem& sys, Player& self, float dtMs) {
    (void)sys;
    (void)self;
    (void)dtMs;
  }
  // Lets an ability be pressed again while on cooldown (recasts/toggles).
  virtual bool canRecast(const Player& self, int slot) const {
    (void)self;
    (void)slot;
    return false;
  }
  virtual void onDealDamage(AbilitySystem& sys, Player& self, Enemy& enemy,
                            float damage, uint32_t flags) {
    (void)sys;
    (void)self;
    (void)enemy;
    (void)damage;
    (void)flags;
  }
  // Return the damage that should actually be applied.
  virtual float onTakeDamage(AbilitySystem& sys, Player& self, float damage,
                             uint32_t sourceEnemyId) {
    (void)sys;
    (void)self;
    (void)sourceEnemyId;
    return damage;
  }
  // Return true if the kit prevented the death.
  virtual bool onLethal(AbilitySystem& sys, Player& self) {
    (void)sys;
    (void)self;
    return false;
  }
  virtual void onEnemyDiedNearby(AbilitySystem& sys, Player& self,
                                 const Enemy& enemy) {
    (void)sys;
    (void)self;
    (void)enemy;
  }
  virtual void onHealDone(AbilitySystem& sys, Player& self, Player& target,
                          float amount) {
    (void)sys;
    (void)self;
    (void)target;
    (void)amount;
  }
  // A creature near `self` lost Guard that prevented `prevented` damage.
  virtual void onGuardLostNearby(AbilitySystem& sys, Player& self,
                                 bool targetIsEnemy, uint32_t targetId,
                                 float prevented) {
    (void)sys;
    (void)self;
    (void)targetIsEnemy;
    (void)targetId;
    (void)prevented;
  }
  // Extra movement speed multiplier from the kit (1 = none).
  virtual float speedMultiplier(const Player& self) const {
    (void)self;
    return 1.0f;
  }
  virtual void onRoundStart(Player& self) { (void)self; }
};

// Defined in CharacterKits.cpp. nullptr for characters without a kit.
std::unique_ptr<Kit> makeKit(uint32_t characterId);

// Server-side combat: turns player input into abilities and owns the damage
// pipeline, projectiles, ground zones and timed callbacks. Kits, enemies and
// infusions all go through here so every hit obeys the same rules.
class AbilitySystem {
 public:
  using EmitFn = std::function<void(const GameEventPacket&)>;

  AbilitySystem(std::unordered_map<uint32_t, Player>& players,
                EnemySystem* enemySystem, EffectManager* effectManager,
                const CollisionSystem* collisionSystem, float worldWidth,
                float worldHeight, EmitFn emitFn);
  ~AbilitySystem();

  // ---- lifecycle ---------------------------------------------------------
  void update(float dtMs);
  void setInput(Player& player, uint8_t buttons, float aimX, float aimY);
  // Apply kit stats (health, speed) after a character is chosen.
  void applyKitStats(Player& player);
  // Clear cooldowns / transient state at the start of a round.
  void resetPlayerForRound(Player& player);
  // Remove projectiles, zones and pending timers (round end).
  void clearTransient();
  void onEnemyDied(const Enemy& enemy, uint32_t killerId);
  // When false, players cannot use abilities (lobby / intermission / end).
  void setCombatEnabled(bool enabled) { combatEnabled = enabled; }

  // ---- damage pipeline ---------------------------------------------------
  // All return the amount actually applied.
  float damageEnemy(uint32_t attackerId, Enemy& enemy, float amount,
                    uint32_t flags = DamageFlag::None);
  float damagePlayer(Player& player, float amount, uint32_t sourceEnemyId,
                     uint32_t flags = DamageFlag::None);
  // healerId == 0 for environment heals.
  float healPlayer(uint32_t healerId, Player& target, float amount,
                   bool allowEcho = true);
  // durationMs < 0 uses the effect's base duration.
  void applyToEnemy(Enemy& enemy, EffectType type, int stacks,
                    uint32_t sourceId, float durationMs = -1.0f);
  void applyToPlayer(Player& player, EffectType type, int stacks,
                     uint32_t sourceId, float durationMs = -1.0f);
  void grantShield(Player& player, float amount, float durationMs);

  // ---- motion ------------------------------------------------------------
  void dashPlayer(Player& player, float dirX, float dirY, float distance,
                  float durationMs, bool ghost);
  void blinkPlayer(Player& player, float x, float y);
  void knockbackEnemy(Enemy& enemy, float fromX, float fromY, float distance);
  void pullEnemy(Enemy& enemy, float toX, float toY, float maxDistance,
                 float stopShort);

  // ---- queries -----------------------------------------------------------
  // Callbacks may damage or kill; iteration is over a snapshot of ids.
  void enemiesInCircle(float x, float y, float radius,
                       const std::function<void(Enemy&)>& fn);
  void enemiesInCone(float x, float y, float dirX, float dirY, float range,
                     float halfAngleDeg,
                     const std::function<void(Enemy&)>& fn);
  void enemiesInLine(float x0, float y0, float x1, float y1, float halfWidth,
                     const std::function<void(Enemy&)>& fn);
  void playersInCircle(float x, float y, float radius,
                       const std::function<void(Player&)>& fn);
  void playersInCone(float x, float y, float dirX, float dirY, float range,
                     float halfAngleDeg,
                     const std::function<void(Player&)>& fn);
  // Living ally closest to (x, y) within maxDistance.
  Player* nearestAlly(const Player& self, float x, float y, float maxDistance,
                      bool includeSelf);
  Enemy* nearestEnemy(float x, float y, float maxDistance);
  Player* findPlayer(uint32_t playerId);
  Enemy* findEnemy(uint32_t enemyId);
  int countEnemies(float x, float y, float radius,
                   const std::function<bool(const Enemy&)>& predicate);

  // ---- spawned things ----------------------------------------------------
  struct Projectile {
    uint32_t id = 0;
    bool fromEnemy = false;
    uint32_t ownerId = 0;
    float x = 0, y = 0, vx = 0, vy = 0;
    float radius = 10.0f;
    float lifeMs = 1000.0f;
    bool pierce = false;
    VfxTheme theme = VfxTheme::Neutral;
    uint8_t sprite = 0;  // 0 orb, 1 crescent, 2 can, 3 ball
    std::function<void(Projectile&, Enemy&)> onHitEnemy;
    std::function<void(Projectile&, Player&)> onHitPlayer;
    std::function<void(Projectile&)> onEnd;  // expiry or first non-pierce hit
    std::vector<uint32_t> alreadyHit;
  };

  struct Zone {
    uint32_t id = 0;
    bool fromEnemy = false;
    uint32_t ownerId = 0;
    float x = 0, y = 0;
    float radius = 80.0f;
    float lifeMs = 3000.0f;
    float tickMs = 500.0f;
    float tickTimer = 0.0f;
    VfxTheme theme = VfxTheme::Neutral;
    uint8_t variant = 0;  // 0 cloud, 1 pool, 2 domain ring
    std::function<void(Zone&, Enemy&)> onEnemyTick;
    std::function<void(Zone&, Player&)> onPlayerTick;
    std::function<void(Zone&)> onEnd;
  };

  uint32_t spawnProjectile(Projectile spec);
  uint32_t spawnZone(Zone spec);
  void schedule(float delayMs, std::function<void()> fn);

  // ---- VFX (broadcast to clients) ----------------------------------------
  void emit(const GameEventPacket& packet);
  void fxSlash(float x, float y, float dirX, float dirY, float radius,
               float arcDeg, VfxTheme theme);
  void fxCone(float x, float y, float dirX, float dirY, float radius,
              float arcDeg, VfxTheme theme);
  void fxBurst(float x, float y, float radius, VfxTheme theme);
  void fxRing(float x, float y, float radius, VfxTheme theme);
  void fxBeam(float x0, float y0, float x1, float y1, float width,
              float durationMs, VfxTheme theme);
  void fxLob(float x0, float y0, float x1, float y1, float radius,
             float flightMs, VfxTheme theme, uint8_t sprite);
  void fxTelegraphCircle(float x, float y, float radius, float durationMs,
                         VfxTheme theme = VfxTheme::Danger);
  void fxTelegraphCone(float x, float y, float dirX, float dirY, float radius,
                       float halfAngleDeg, float durationMs);
  void fxDashTrail(float x0, float y0, float x1, float y1, VfxTheme theme);
  void fxTether(float x0, float y0, float x1, float y1, float durationMs,
                VfxTheme theme);
  void fxEffectText(float x, float y, EffectType type);
  void fxAnnounce(Announcement announcement);

  // ---- misc --------------------------------------------------------------
  EffectManager& effects() { return *effectManager; }
  EnemySystem& enemySystem() { return *enemies; }
  std::unordered_map<uint32_t, Player>& allPlayers() { return players; }
  Kit* kitFor(const Player& player);
  float nowMs() const { return timeMs; }
  float random01();
  float randomRange(float lo, float hi);
  // Clamp a point to the playable diamond.
  void clampToWorld(float& x, float& y) const;
  bool isTargetable(const Player& player) const;

 private:
  std::unordered_map<uint32_t, Player>& players;
  EnemySystem* enemies;
  EffectManager* effectManager;
  const CollisionSystem* collision;
  float worldWidth, worldHeight;
  EmitFn emitFn;
  bool combatEnabled = true;

  float timeMs = 0.0f;
  uint32_t nextSpawnId = 1;
  std::vector<Projectile> projectiles;
  std::vector<Zone> zones;
  struct Timer {
    float atMs;
    std::function<void()> fn;
  };
  std::vector<Timer> timers;
  std::unordered_map<uint32_t, std::unique_ptr<Kit>> kits;
  std::mt19937 rng{20240107};

  void updatePlayer(Player& player, float dtMs);
  void updateAbilityInput(Player& player, float dtMs);
  void fire(Player& player, int slot, float charge);
  void updateForcedMotion(Player& player, float dtMs);
  void updateProjectiles(float dtMs);
  void updateZones(float dtMs);
  void runTimers();
  AbilityContext makeContext(Player& player, float charge);
  void notifyGuardLost(bool targetIsEnemy, uint32_t targetId, float x,
                       float y, float prevented);
  std::vector<uint32_t> enemyIdsNear(float x, float y, float radius) const;
};
