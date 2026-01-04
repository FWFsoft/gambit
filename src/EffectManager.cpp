#include "EffectManager.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <random>

#include "Enemy.h"
#include "EnemySystem.h"
#include "GlobalModifiers.h"
#include "Logger.h"
#include "Player.h"
#include "config/PlayerConfig.h"

EffectManager::EffectManager() : accumulatedTime(0.0f) {
  Logger::info("EffectManager created");
}

void EffectManager::update(float deltaTime,
                           std::unordered_map<uint32_t, Player>& players,
                           std::unordered_map<uint32_t, Enemy>& enemies,
                           EnemySystem* enemySystem) {
  accumulatedTime += deltaTime;

  // Update player effects
  for (auto& [playerId, effects] : playerEffects) {
    auto playerIt = players.find(playerId);
    if (playerIt != players.end()) {
      Player& player = playerIt->second;
      updateEntityEffects(playerId, effects, player.health,
                          Config::Player::MAX_HEALTH, deltaTime, player.x,
                          player.y);

      // Check if player died from DoT
      if (player.health <= 0.0f && !player.isDead()) {
        player.health = 0.0f;
        Logger::info("💀 Player " + std::to_string(playerId) +
                     " died from DoT");
      }
    }
  }

  // Update enemy effects
  for (auto& [enemyId, effects] : enemyEffects) {
    auto enemyIt = enemies.find(enemyId);
    if (enemyIt != enemies.end()) {
      Enemy& enemy = enemyIt->second;
      float oldHealth = enemy.health;

      updateEntityEffects(enemyId, effects, enemy.health, enemy.maxHealth,
                          deltaTime, enemy.x, enemy.y);

      // Check if enemy died from DoT
      if (enemy.health <= 0.0f && enemy.state != EnemyState::Dead) {
        enemy.health = 0.0f;
        enemy.state = EnemyState::Dead;
        enemy.vx = 0.0f;
        enemy.vy = 0.0f;
        enemy.deathTime = accumulatedTime;

        // Set random respawn delay (5-10 seconds)
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(5000.0f, 10000.0f);
        enemy.respawnDelay = dist(gen);

        // Find Wound effect to credit the killer
        uint32_t killerId = 0;
        EffectInstance* woundEffect = effects.findEffect(EffectType::Wound);
        if (woundEffect) {
          killerId = woundEffect->sourceId;
        }

        Logger::info("💀 Enemy " + std::to_string(enemyId) +
                     " died from DoT (killed by player " +
                     std::to_string(killerId) + ", respawn in " +
                     std::to_string(enemy.respawnDelay / 1000.0f) + "s)");

        // Record death for broadcasting
        if (enemySystem) {
          enemySystem->recordDeath(enemyId, killerId);
        }
      }
    }
  }
}

void EffectManager::applyEffect(uint32_t playerId, EffectType type,
                                uint8_t stacks, float durationMs,
                                uint32_t sourceId,
                                std::unordered_map<uint32_t, Player>& players) {
  (void)players;  // Unused for now
  applyEffectInternal(playerEffects[playerId], type, stacks, durationMs,
                      sourceId, true);
}

void EffectManager::applyEffect(uint32_t enemyId, EffectType type,
                                uint8_t stacks, float durationMs,
                                uint32_t sourceId,
                                std::unordered_map<uint32_t, Enemy>& enemies) {
  (void)enemies;  // Unused for now
  applyEffectInternal(enemyEffects[enemyId], type, stacks, durationMs, sourceId,
                      false);
}

void EffectManager::cleanseDebuff(uint32_t playerId, EffectType type) {
  auto it = playerEffects.find(playerId);
  if (it != playerEffects.end()) {
    const EffectDefinition& def = EffectRegistry::get(type);
    if (def.category == EffectCategory::Debuff) {
      it->second.removeEffect(type);
      Logger::debug("Cleansed debuff " + std::string(def.name) +
                    " from player " + std::to_string(playerId));
    }
  }
}

void EffectManager::purgeBuff(uint32_t playerId, EffectType type) {
  auto it = playerEffects.find(playerId);
  if (it != playerEffects.end()) {
    const EffectDefinition& def = EffectRegistry::get(type);
    if (def.category == EffectCategory::Buff) {
      it->second.removeEffect(type);
      Logger::debug("Purged buff " + std::string(def.name) + " from player " +
                    std::to_string(playerId));
    }
  }
}

const ActiveEffects& EffectManager::getPlayerEffects(uint32_t playerId) const {
  static const ActiveEffects emptyEffects;
  auto it = playerEffects.find(playerId);
  return it != playerEffects.end() ? it->second : emptyEffects;
}

const ActiveEffects& EffectManager::getEnemyEffects(uint32_t enemyId) const {
  static const ActiveEffects emptyEffects;
  auto it = enemyEffects.find(enemyId);
  return it != enemyEffects.end() ? it->second : emptyEffects;
}

EffectManager::StatModifiers EffectManager::calculateModifiers(
    uint32_t playerId) const {
  return calculateModifiers(playerId, false);
}

EffectManager::StatModifiers EffectManager::calculateModifiers(
    uint32_t entityId, bool isEnemy) const {
  StatModifiers mods;

  const ActiveEffects& effects =
      isEnemy ? getEnemyEffects(entityId) : getPlayerEffects(entityId);

  // Movement speed modifiers
  mods.movementSpeedMultiplier = 1.0f;
  mods.movementSpeedMultiplier -=
      effects.getStacks(EffectType::Slow) *
      EffectRegistry::get(EffectType::Slow).baseIntensity;
  mods.movementSpeedMultiplier +=
      effects.getStacks(EffectType::Haste) *
      EffectRegistry::get(EffectType::Haste).baseIntensity;

  // Damage dealt modifiers
  mods.damageDealtMultiplier = 1.0f;
  mods.damageDealtMultiplier -=
      effects.getStacks(EffectType::Weakened) *
      EffectRegistry::get(EffectType::Weakened).baseIntensity;
  mods.damageDealtMultiplier +=
      effects.getStacks(EffectType::Empowered) *
      EffectRegistry::get(EffectType::Empowered).baseIntensity;

  // Damage taken modifiers (Vulnerable/Fortified, NOT Expose/Guard)
  mods.damageTakenMultiplier = 1.0f;
  mods.damageTakenMultiplier +=
      effects.getStacks(EffectType::Vulnerable) *
      EffectRegistry::get(EffectType::Vulnerable).baseIntensity;
  mods.damageTakenMultiplier -=
      effects.getStacks(EffectType::Fortified) *
      EffectRegistry::get(EffectType::Fortified).baseIntensity;

  // Clamp to reasonable values
  mods.movementSpeedMultiplier = std::max(0.0f, mods.movementSpeedMultiplier);
  mods.damageDealtMultiplier = std::max(0.0f, mods.damageDealtMultiplier);
  mods.damageTakenMultiplier = std::max(0.0f, mods.damageTakenMultiplier);

  // Movement flags
  mods.canMove = !effects.hasEffect(EffectType::Stunned) &&
                 !effects.hasEffect(EffectType::Snared) &&
                 !effects.hasEffect(EffectType::Grappled);

  mods.canAct = !effects.hasEffect(EffectType::Stunned);

  mods.canUseAbilities = !effects.hasEffect(EffectType::Silenced);

  return mods;
}

void EffectManager::consumeOnDamage(uint32_t targetId, bool isEnemy,
                                    float& incomingDamage) {
  ActiveEffects* effects =
      isEnemy ? &enemyEffects[targetId] : &playerEffects[targetId];

  // Guard reduces damage (consumed on use)
  if (EffectInstance* guard = effects->findEffect(EffectType::Guard)) {
    float reductionPerStack =
        EffectRegistry::get(EffectType::Guard).baseIntensity;
    float totalReduction = reductionPerStack * guard->stacks;
    incomingDamage *= (1.0f - totalReduction);

    // Consume one stack
    if (guard->stacks > 1) {
      guard->stacks--;
    } else {
      effects->removeEffect(EffectType::Guard);
    }

    Logger::debug("Guard consumed, damage reduced by " +
                  std::to_string(totalReduction * 100.0f) + "%");
  }

  // Expose increases damage (consumed on use)
  if (EffectInstance* expose = effects->findEffect(EffectType::Expose)) {
    float increasePerStack =
        EffectRegistry::get(EffectType::Expose).baseIntensity;
    float totalIncrease = increasePerStack * expose->stacks;
    incomingDamage *= (1.0f + totalIncrease);

    // Consume one stack
    if (expose->stacks > 1) {
      expose->stacks--;
    } else {
      effects->removeEffect(EffectType::Expose);
    }

    Logger::debug("Expose consumed, damage increased by " +
                  std::to_string(totalIncrease * 100.0f) + "%");
  }

  // Apply Vulnerable/Fortified (NOT consumed - applied via calculateModifiers)
  auto mods = calculateModifiers(targetId, isEnemy);
  incomingDamage *= mods.damageTakenMultiplier;
}

// Internal implementation

void EffectManager::applyEffectInternal(ActiveEffects& activeEffects,
                                        EffectType type, uint8_t stacks,
                                        float durationMs, uint32_t sourceId,
                                        bool isPlayer) {
  const EffectDefinition& def = EffectRegistry::get(type);

  // Check if effect can be applied (immunities)
  if (!canApplyEffect(activeEffects, type)) {
    Logger::debug("Cannot apply effect " + std::string(def.name) +
                  " (immune or blocked)");
    return;
  }

  // Handle opposite effects (cancel each other)
  handleOppositeEffect(activeEffects, type);

  // Find existing instance of this effect
  EffectInstance* existing = activeEffects.findEffect(type);

  // Apply global modifiers (placeholder for objectives system)
  float modifiedDuration =
      durationMs * GlobalModifiers::instance().getDurationMultiplier(type);

  if (existing) {
    // Effect already exists - apply stacking behavior
    switch (def.stackBehavior) {
      case StackBehavior::Stacks:
        // Add stacks (up to max), refresh all durations
        existing->stacks = static_cast<uint8_t>(std::min(
            static_cast<int>(existing->stacks) + static_cast<int>(stacks),
            static_cast<int>(def.maxStacks)));
        existing->remainingDuration = modifiedDuration;
        Logger::debug("Stacked effect " + std::string(def.name) + " to " +
                      std::to_string(existing->stacks) + " stacks");
        break;

      case StackBehavior::Extends:
        // Add to duration
        existing->remainingDuration += modifiedDuration;
        Logger::debug("Extended effect " + std::string(def.name) +
                      " duration to " +
                      std::to_string(existing->remainingDuration) + "ms");
        break;

      case StackBehavior::Overrides:
        // Reset duration
        existing->remainingDuration = modifiedDuration;
        Logger::debug("Overrode effect " + std::string(def.name) + " duration");
        break;
    }
  } else {
    // New effect
    EffectInstance instance(type, stacks, modifiedDuration, sourceId);
    activeEffects.effects.push_back(instance);
    Logger::info("✨ Applied new effect " + std::string(def.name) + " with " +
                 std::to_string(stacks) + " stacks, duration " +
                 std::to_string(modifiedDuration) + "ms");
  }

  // Apply secondary effects
  applySecondaryEffects(activeEffects, type, sourceId, isPlayer);
}

void EffectManager::updateEntityEffects(uint32_t entityId,
                                        ActiveEffects& activeEffects,
                                        float& health, float maxHealth,
                                        float deltaTime, float entityX,
                                        float entityY) {
  std::vector<EffectType> expiredEffects;

  for (auto& effect : activeEffects.effects) {
    // Skip consume-on-use effects (they don't tick duration)
    if (effect.isConsumeOnUse()) {
      continue;
    }

    // Tick duration
    effect.remainingDuration -= deltaTime;

    if (effect.isExpired()) {
      expiredEffects.push_back(effect.type);
      continue;
    }

    // Apply Wound/Mend DoT/HoT
    if (effect.type == EffectType::Wound || effect.type == EffectType::Mend) {
      tickWoundMend(effect, deltaTime, health, maxHealth, entityX, entityY);
    }
  }

  // Remove expired effects
  for (EffectType expiredType : expiredEffects) {
    activeEffects.removeEffect(expiredType);
    Logger::info("⏱️  Effect " +
                 std::string(EffectRegistry::getName(expiredType)) +
                 " expired on entity " + std::to_string(entityId));
  }
}

bool EffectManager::canApplyEffect(const ActiveEffects& activeEffects,
                                   EffectType type) const {
  const EffectDefinition& def = EffectRegistry::get(type);

  // Blessed blocks all debuffs
  if (def.category == EffectCategory::Debuff &&
      activeEffects.hasEffect(EffectType::Blessed)) {
    return false;
  }

  // Cursed blocks all buffs
  if (def.category == EffectCategory::Buff &&
      activeEffects.hasEffect(EffectType::Cursed)) {
    return false;
  }

  // Specific immunities
  if (type == EffectType::Stunned &&
      activeEffects.hasEffect(EffectType::Berserk)) {
    return false;
  }

  if ((type == EffectType::Slow || type == EffectType::Snared) &&
      activeEffects.hasEffect(EffectType::Unbounded)) {
    return false;
  }

  if ((type == EffectType::Slow || type == EffectType::Grappled) &&
      activeEffects.hasEffect(EffectType::Freed)) {
    return false;
  }

  if (type == EffectType::Confused &&
      activeEffects.hasEffect(EffectType::Focused)) {
    return false;
  }

  if (type == EffectType::Silenced &&
      activeEffects.hasEffect(EffectType::Inspired)) {
    return false;
  }

  return true;
}

void EffectManager::handleOppositeEffect(ActiveEffects& activeEffects,
                                         EffectType newType) {
  EffectType opposite = EffectRegistry::getOpposite(newType);

  if (opposite != newType && activeEffects.hasEffect(opposite)) {
    EffectInstance* oppositeEffect = activeEffects.findEffect(opposite);

    if (oppositeEffect) {
      // Remove one stack of opposite effect
      if (oppositeEffect->stacks > 1) {
        oppositeEffect->stacks--;
        Logger::debug("Removed 1 stack of opposite effect " +
                      std::string(EffectRegistry::getName(opposite)));
      } else {
        activeEffects.removeEffect(opposite);
        Logger::debug("Removed opposite effect " +
                      std::string(EffectRegistry::getName(opposite)));
      }
    }
  }
}

void EffectManager::applySecondaryEffects(ActiveEffects& activeEffects,
                                          EffectType primaryType,
                                          uint32_t sourceId, bool isPlayer) {
  const EffectDefinition& def = EffectRegistry::get(primaryType);

  for (uint8_t i = 0; i < def.secondaryEffectCount; i++) {
    const SecondaryEffect& secondary = def.secondaryEffects[i];
    const EffectDefinition& secondaryDef = EffectRegistry::get(secondary.type);

    // Apply secondary effect recursively (respects immunities)
    applyEffectInternal(activeEffects, secondary.type, secondary.stacks,
                        secondaryDef.baseDuration, sourceId, isPlayer);

    Logger::debug("Applied secondary effect " + std::string(secondaryDef.name) +
                  " from " + std::string(def.name));
  }
}

void EffectManager::tickWoundMend(EffectInstance& instance, float deltaTime,
                                  float& health, float maxHealth, float entityX,
                                  float entityY) {
  constexpr float TICK_INTERVAL = 1000.0f;  // 1 second per tick

  instance.lastTickTime += deltaTime;

  if (instance.lastTickTime >= TICK_INTERVAL) {
    instance.lastTickTime -= TICK_INTERVAL;

    const EffectDefinition& def = EffectRegistry::get(instance.type);
    float valuePerStack = def.baseIntensity;
    float totalValue = valuePerStack * instance.stacks;

    if (instance.type == EffectType::Wound) {
      health -= totalValue;
      health = std::max(0.0f, health);
      Logger::info("💉 Wound ticked for " + std::to_string(totalValue) +
                   " damage");
    } else {  // Mend
      health += totalValue;
      health = std::min(maxHealth, health);
      Logger::info("💚 Mend ticked for " + std::to_string(totalValue) +
                   " healing");
      // Note: Healing events are published client-side when health increase is
      // detected
    }
  }
}
