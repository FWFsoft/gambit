#pragma once

#include <cstdint>
#include <unordered_map>

#include "Effect.h"
#include "EffectInstance.h"

// Forward declarations
struct Player;
struct Enemy;
class EnemySystem;

// Server-authoritative effect manager
class EffectManager {
 public:
  EffectManager();

  // Stat modifiers calculated from active effects
  struct StatModifiers {
    float movementSpeedMultiplier;  // 1.0 = normal speed
    float damageDealtMultiplier;    // 1.0 = normal damage
    float damageTakenMultiplier;    // 1.0 = normal damage taken
    bool canMove;                   // Can the entity move?
    bool canUseAbilities;  // Can the entity use abilities? (not silenced)
    bool canAct;           // Can the entity act at all? (not stunned)

    StatModifiers()
        : movementSpeedMultiplier(1.0f),
          damageDealtMultiplier(1.0f),
          damageTakenMultiplier(1.0f),
          canMove(true),
          canUseAbilities(true),
          canAct(true) {}
  };

  // Update all active effects (tick durations, apply DoT/HoT)
  void update(float deltaTime, std::unordered_map<uint32_t, Player>& players,
              std::unordered_map<uint32_t, Enemy>& enemies,
              EnemySystem* enemySystem = nullptr);

  // Apply effect to player
  void applyEffect(uint32_t playerId, EffectType type, uint8_t stacks,
                   float durationMs, uint32_t sourceId,
                   std::unordered_map<uint32_t, Player>& players);

  // Apply effect to enemy
  void applyEffect(uint32_t enemyId, EffectType type, uint8_t stacks,
                   float durationMs, uint32_t sourceId,
                   std::unordered_map<uint32_t, Enemy>& enemies);

  // Cleanse debuff from player (remove specific debuff)
  void cleanseDebuff(uint32_t playerId, EffectType type);

  // Purge buffs from player (remove specific buff)
  void purgeBuff(uint32_t playerId, EffectType type);

  // Get active effects for an entity (for network synchronization)
  const ActiveEffects& getPlayerEffects(uint32_t playerId) const;
  const ActiveEffects& getEnemyEffects(uint32_t enemyId) const;

  // Calculate stat modifiers from active effects
  StatModifiers calculateModifiers(uint32_t playerId) const;
  StatModifiers calculateModifiers(uint32_t enemyId, bool isEnemy) const;

  // Consume Expose/Guard stacks when damage is taken
  // Modifies incomingDamage based on active effects
  void consumeOnDamage(uint32_t targetId, bool isEnemy, float& incomingDamage);

 private:
  // Active effects per entity
  std::unordered_map<uint32_t, ActiveEffects> playerEffects;
  std::unordered_map<uint32_t, ActiveEffects> enemyEffects;

  // Accumulated time for DoT/HoT ticking
  float accumulatedTime;

  // Internal helper methods
  void applyEffectInternal(ActiveEffects& activeEffects, EffectType type,
                           uint8_t stacks, float durationMs, uint32_t sourceId,
                           bool isPlayer);

  void updateEntityEffects(uint32_t entityId, ActiveEffects& activeEffects,
                           float& health, float maxHealth, float deltaTime,
                           float entityX, float entityY);

  bool canApplyEffect(const ActiveEffects& activeEffects,
                      EffectType type) const;

  void handleOppositeEffect(ActiveEffects& activeEffects, EffectType newType);

  void applySecondaryEffects(ActiveEffects& activeEffects,
                             EffectType primaryType, uint32_t sourceId,
                             bool isPlayer);

  // DoT/HoT tick logic (for Wound and Mend)
  void tickWoundMend(EffectInstance& instance, float deltaTime, float& health,
                     float maxHealth, float entityX, float entityY);
};
