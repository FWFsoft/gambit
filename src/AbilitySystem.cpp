#include "AbilitySystem.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "CollisionSystem.h"
#include "EffectManager.h"
#include "EnemySystem.h"
#include "Infusions.h"
#include "Logger.h"

namespace {

constexpr float DEG_TO_RAD = 3.14159265f / 180.0f;
constexpr float CHARGE_MOVE_MULT = 0.55f;
constexpr float SCORE_PER_DAMAGE = 0.12f;
constexpr float SCORE_PER_HEAL = 0.25f;
constexpr float KIT_HOOK_RANGE = 450.0f;  // "nearby" for passive hooks

bool isHardCrowdControl(EffectType type) {
  return type == EffectType::Stunned || type == EffectType::Snared ||
         type == EffectType::Confused;
}

bool isControlEffect(EffectType type) {
  return type == EffectType::Stunned || type == EffectType::Snared ||
         type == EffectType::Slow || type == EffectType::Silenced ||
         type == EffectType::Confused || type == EffectType::Grappled;
}

float length(float x, float y) { return std::sqrt(x * x + y * y); }

}  // namespace

AbilitySystem::AbilitySystem(std::unordered_map<uint32_t, Player>& players,
                             EnemySystem* enemySystem,
                             EffectManager* effectManager,
                             const CollisionSystem* collisionSystem,
                             float worldWidth, float worldHeight, EmitFn emitFn)
    : players(players),
      enemies(enemySystem),
      effectManager(effectManager),
      collision(collisionSystem),
      worldWidth(worldWidth),
      worldHeight(worldHeight),
      emitFn(std::move(emitFn)) {
  assert(enemies != nullptr && effectManager != nullptr);
}

AbilitySystem::~AbilitySystem() = default;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void AbilitySystem::update(float dtMs) {
  timeMs += dtMs;
  runTimers();
  for (auto& [id, player] : players) {
    updatePlayer(player, dtMs);
  }
  updateProjectiles(dtMs);
  updateZones(dtMs);
}

void AbilitySystem::setInput(Player& player, uint8_t buttons, float aimX,
                             float aimY) {
  player.pressedLatch |= buttons & ~player.lastPacketButtons;
  player.lastPacketButtons = buttons;
  player.buttons = buttons;
  player.aimX = aimX;
  player.aimY = aimY;
}

Kit* AbilitySystem::kitFor(const Player& player) {
  auto it = kits.find(player.characterId);
  if (it == kits.end()) {
    it = kits.emplace(player.characterId, makeKit(player.characterId)).first;
  }
  return it->second.get();
}

void AbilitySystem::applyKitStats(Player& player) {
  const Arena::KitInfo* info = Arena::kitInfo(player.characterId);
  if (info) {
    player.maxHealth = info->maxHealth;
    player.moveSpeed = info->moveSpeed;
  } else {
    player.maxHealth = Config::Player::MAX_HEALTH;
    player.moveSpeed = Config::Player::SPEED;
  }
  player.health = player.maxHealth;
  resetPlayerForRound(player);
}

void AbilitySystem::resetPlayerForRound(Player& player) {
  for (float& cooldown : player.cooldownMs) cooldown = 0.0f;
  player.chargeSlot = -1;
  player.chargeMs = 0.0f;
  player.channelTimerMs = 0.0f;
  player.actionLockMs = 0.0f;
  player.forcedMs = 0.0f;
  player.intangibleMs = 0.0f;
  player.hiddenMs = 0.0f;
  player.shield = 0.0f;
  player.shieldMs = 0.0f;
  player.resource = 0;
  player.coreTimerMs = 0.0f;
  for (float& t : player.kitTimer) t = 0.0f;
  for (int& c : player.kitCounter) c = 0;
  player.kitTarget = 0;
  for (float& t : player.infusionTimer) t = 0.0f;
  player.reviveProgress = 0.0f;
  effectManager->clearEntity(player.id, false);
  if (Kit* kit = kitFor(player)) kit->onRoundStart(player);
}

void AbilitySystem::clearTransient() {
  for (auto& projectile : projectiles) {
    GameEventPacket end;
    end.kind = GameEventKind::ProjectileEnd;
    end.extraId = projectile.id;
    end.x = projectile.x;
    end.y = projectile.y;
    emit(end);
  }
  for (auto& zone : zones) {
    GameEventPacket end;
    end.kind = GameEventKind::ZoneEnd;
    end.extraId = zone.id;
    emit(end);
  }
  projectiles.clear();
  zones.clear();
  timers.clear();
}

void AbilitySystem::onEnemyDied(const Enemy& enemy, uint32_t killerId) {
  effectManager->clearEntity(enemy.id, true);
  for (auto& [id, player] : players) {
    if (player.isDead()) continue;
    float d = length(player.x - enemy.x, player.y - enemy.y);
    if (d <= KIT_HOOK_RANGE) {
      if (Kit* kit = kitFor(player)) kit->onEnemyDiedNearby(*this, player, enemy);
    }
  }
  if (Player* killer = findPlayer(killerId)) {
    Infusions::onKill(*this, *killer, enemy);
  }
}

bool AbilitySystem::isTargetable(const Player& player) const {
  return player.characterId != 0 && player.isAlive() &&
         player.hiddenMs <= 0.0f;
}

// ---------------------------------------------------------------------------
// Per-player update
// ---------------------------------------------------------------------------

void AbilitySystem::updatePlayer(Player& player, float dtMs) {
  if (player.characterId == 0) {
    // Still picking a character: not in the world yet.
    player.stateFlags = PlayerFlags::Hidden;
    player.prevButtons = player.buttons;
    return;
  }
  if (player.isDead()) {
    player.chargeSlot = -1;
    player.forcedMs = 0.0f;
    player.stateFlags = PlayerFlags::Dead;
    player.prevButtons = player.buttons;
    return;
  }

  Kit* kit = kitFor(player);
  auto mods = effectManager->calculateModifiers(player.id, false);
  const ActiveEffects& active = effectManager->getPlayerEffects(player.id);

  // Haste / Slow also change how fast abilities recharge.
  float rechargeRate = 1.0f + 0.15f * active.getStacks(EffectType::Haste) -
                       0.15f * active.getStacks(EffectType::Slow);
  rechargeRate = std::max(0.4f, rechargeRate);
  for (float& cooldown : player.cooldownMs) {
    cooldown = std::max(0.0f, cooldown - dtMs * rechargeRate);
  }
  player.actionLockMs = std::max(0.0f, player.actionLockMs - dtMs);
  player.intangibleMs = std::max(0.0f, player.intangibleMs - dtMs);
  player.hiddenMs = std::max(0.0f, player.hiddenMs - dtMs);
  if (player.shieldMs > 0.0f) {
    player.shieldMs -= dtMs;
    if (player.shieldMs <= 0.0f) player.shield = 0.0f;
  }

  if (kit) kit->tick(*this, player, dtMs);
  Infusions::tick(*this, player, dtMs);

  float speed = mods.movementSpeedMultiplier;
  if (kit) speed *= kit->speedMultiplier(player);
  speed *= Infusions::speedMultiplier(player);
  if (player.chargeSlot >= 0) speed *= CHARGE_MOVE_MULT;
  player.speedMult = std::max(0.0f, speed);

  updateForcedMotion(player, dtMs);

  bool canAct = mods.canAct && player.forcedMs <= 0.0f && combatEnabled;
  if (canAct) {
    updateAbilityInput(player, dtMs);
  } else if (player.chargeSlot >= 0) {
    // Interrupted: the charge is lost but the cooldown is not spent.
    player.chargeSlot = -1;
    player.chargeMs = 0.0f;
  }
  // Presses made while stunned or mid-dash are dropped, not queued.
  player.pressedLatch = 0;
  player.prevButtons = player.buttons;

  uint8_t flags = 0;
  if (!mods.canMove) flags |= PlayerFlags::Rooted;
  if (!mods.canAct) flags |= PlayerFlags::Stunned;
  if (player.forcedMs > 0.0f) flags |= PlayerFlags::MoveLocked;
  if (player.chargeSlot >= 0) flags |= PlayerFlags::Charging;
  if (player.hiddenMs > 0.0f || player.intangibleMs > 0.0f) {
    flags |= PlayerFlags::Hidden;
  }
  player.stateFlags = flags;
}

AbilityContext AbilitySystem::makeContext(Player& player, float charge) {
  float dx = player.aimX - player.x;
  float dy = player.aimY - player.y;
  float d = length(dx, dy);
  float dirX = d > 0.001f ? dx / d : 1.0f;
  float dirY = d > 0.001f ? dy / d : 0.0f;
  return AbilityContext{*this, player, player.aimX, player.aimY, dirX, dirY,
                        charge};
}

void AbilitySystem::updateAbilityInput(Player& player, float dtMs) {
  Kit* kit = kitFor(player);
  const Arena::KitInfo* info = Arena::kitInfo(player.characterId);
  if (!kit || !info) return;

  bool silenced =
      !effectManager->calculateModifiers(player.id, false).canUseAbilities;
  // A latched press counts as held for this tick even if it was already
  // released (tap shorter than a tick).
  uint8_t pressed = (player.buttons & ~player.prevButtons) | player.pressedLatch;
  uint8_t held = player.buttons | player.pressedLatch;
  player.pressedLatch = 0;

  for (int slot = 0; slot < Arena::SLOT_COUNT; slot++) {
    uint8_t bit = static_cast<uint8_t>(1u << slot);
    bool isCore = slot == static_cast<int>(Arena::AbilitySlot::Core);
    if (isCore && player.coreGenus == Arena::Genus::None) continue;
    // Silence allows the basic attack only.
    if (silenced && slot != 0) {
      if (player.chargeSlot == slot) player.chargeSlot = -1;
      continue;
    }

    const Arena::AbilityInfo& ability =
        isCore ? Arena::coreAbility(player.coreGenus) : info->abilities[slot];
    bool ready =
        player.cooldownMs[slot] <= 0.0f && player.actionLockMs <= 0.0f;

    switch (ability.mode) {
      case Arena::AbilityMode::Tap: {
        bool wants = (pressed & bit) != 0;
        // The basic attack repeats while the button is held.
        if (slot == 0 && (held & bit) != 0) wants = true;
        if (!wants || player.chargeSlot >= 0) break;
        bool recast = (pressed & bit) != 0 &&
                      (isCore ? Infusions::canRecastCore(player)
                              : kit->canRecast(player, slot));
        if (ready || recast) fire(player, slot, 0.0f);
        break;
      }
      case Arena::AbilityMode::Charge: {
        if (player.chargeSlot < 0 && (pressed & bit) != 0 && ready) {
          player.chargeSlot = slot;
          player.chargeMs = 0.0f;
        }
        if (player.chargeSlot != slot) break;
        if ((held & bit) != 0) {
          player.chargeMs += dtMs;
          float charge = std::min(1.0f, player.chargeMs / ability.maxChargeMs);
          AbilityContext ctx = makeContext(player, charge);
          kit->chargeTick(ctx, slot, dtMs);
          // Holding well past full charge releases automatically.
          if (player.chargeMs >= ability.maxChargeMs + 1500.0f) {
            player.chargeSlot = -1;
            fire(player, slot, 1.0f);
          }
        } else {
          float charge = std::min(1.0f, player.chargeMs / ability.maxChargeMs);
          player.chargeSlot = -1;
          fire(player, slot, charge);
        }
        break;
      }
      case Arena::AbilityMode::Channel: {
        bool channelling = player.chargeSlot == slot;
        if (!channelling && (pressed & bit) != 0 && ready &&
            player.chargeSlot < 0) {
          player.chargeSlot = slot;
          player.chargeMs = 0.0f;
          player.channelTimerMs = 0.0f;
          channelling = true;
        }
        if (!channelling) break;
        if ((held & bit) != 0) {
          player.chargeMs += dtMs;
          player.channelTimerMs -= dtMs;
          if (player.channelTimerMs <= 0.0f) {
            int haste = effectManager->getPlayerEffects(player.id).getStacks(
                EffectType::Haste);
            player.channelTimerMs = ability.maxChargeMs / (1.0f + 0.2f * haste);
            AbilityContext ctx = makeContext(player, 0.0f);
            GameEventPacket cast;
            cast.kind = GameEventKind::AbilityCast;
            cast.sourceId = player.id;
            cast.x = player.x;
            cast.y = player.y;
            cast.x2 = player.aimX;
            cast.y2 = player.aimY;
            cast.value = 120.0f;
            cast.param = static_cast<uint8_t>(slot);
            // The kit picks the form (which hand) while casting, so the
            // event goes out after it.
            player.castVariant = 0;
            kit->cast(ctx, slot);
            cast.extraId = player.castVariant;
            emit(cast);
          }
        } else {
          player.chargeSlot = -1;
          player.cooldownMs[slot] = ability.cooldownMs;
          kit->channelEnd(*this, player, slot);
        }
        break;
      }
    }
  }
}

void AbilitySystem::fire(Player& player, int slot, float charge) {
  Kit* kit = kitFor(player);
  const Arena::KitInfo* info = Arena::kitInfo(player.characterId);
  if (!kit || !info) return;
  bool isCore = slot == static_cast<int>(Arena::AbilitySlot::Core);
  const Arena::AbilityInfo& ability =
      isCore ? Arena::coreAbility(player.coreGenus) : info->abilities[slot];

  bool wasReady = player.cooldownMs[slot] <= 0.0f;
  if (wasReady) player.cooldownMs[slot] = ability.cooldownMs;
  player.actionLockMs = slot == 0 ? 80.0f : 160.0f;

  GameEventPacket cast;
  cast.kind = GameEventKind::AbilityCast;
  cast.sourceId = player.id;
  cast.x = player.x;
  cast.y = player.y;
  cast.x2 = player.aimX;
  cast.y2 = player.aimY;
  cast.value = slot == 0 ? 180.0f : 280.0f;
  cast.param = static_cast<uint8_t>(slot);

  AbilityContext ctx = makeContext(player, charge);
  player.castVariant = 0;
  if (isCore) {
    Infusions::castCore(ctx);
  } else {
    kit->cast(ctx, slot);
  }
  // Sent after the cast so it can carry the form the kit chose.
  cast.extraId = player.castVariant;
  emit(cast);
}

void AbilitySystem::updateForcedMotion(Player& player, float dtMs) {
  if (player.forcedMs <= 0.0f) return;

  float step = std::min(player.forcedMs, dtMs) / 1000.0f;
  float oldX = player.x;
  float oldY = player.y;
  float newX = player.x + player.forcedVx * step;
  float newY = player.y + player.forcedVy * step;
  if (collision) {
    collision->checkMovement(oldX, oldY, newX, newY,
                             Arena::Tuning::PLAYER_RADIUS);
  }
  clampToWorld(newX, newY);
  player.x = newX;
  player.y = newY;
  player.vx = player.forcedVx;
  player.vy = player.forcedVy;
  player.forcedMs -= dtMs;
  if (player.forcedMs <= 0.0f) {
    player.forcedMs = 0.0f;
    player.forcedGhost = false;
    player.vx = 0.0f;
    player.vy = 0.0f;
  }
}

// ---------------------------------------------------------------------------
// Damage pipeline
// ---------------------------------------------------------------------------

float AbilitySystem::damageEnemy(uint32_t attackerId, Enemy& enemy,
                                 float amount, uint32_t flags) {
  if (enemy.state == EnemyState::Dead || amount <= 0.0f) return 0.0f;

  Player* attacker = findPlayer(attackerId);
  float damage = amount;
  bool crit = false;

  if (attacker) {
    auto mods = effectManager->calculateModifiers(attackerId, false);
    damage *= mods.damageDealtMultiplier;

    if (!(flags & DamageFlag::Dot)) {
      const ActiveEffects& fx = effectManager->getPlayerEffects(attackerId);
      bool sharpened = fx.hasEffect(EffectType::Sharpened);
      bool dulled = fx.hasEffect(EffectType::Dulled);
      float chance = dulled ? 0.0f : (sharpened ? 0.40f : 0.05f);
      crit = (flags & DamageFlag::ForceCrit) != 0 || random01() < chance;
      if (crit) damage *= sharpened ? 2.0f : 1.75f;
    }
  }

  // Guard / Expose are consumed by real hits, not by damage over time.
  if (flags & DamageFlag::Dot) {
    damage *= effectManager->calculateModifiers(enemy.id, true)
                  .damageTakenMultiplier;
  } else {
    auto consumed = effectManager->consumeOnDamage(enemy.id, true, damage);
    if (consumed.guardPrevented > 0.0f) {
      notifyGuardLost(true, enemy.id, enemy.x, enemy.y,
                      consumed.guardPrevented);
    }
  }

  if (damage <= 0.0f) return 0.0f;
  float applied = std::min(damage, enemy.health);
  uint32_t enemyId = enemy.id;
  float ex = enemy.x, ey = enemy.y;
  enemies->damageEnemy(enemyId, damage, attackerId);

  GameEventPacket number;
  number.kind = GameEventKind::Damage;
  number.sourceId = enemyId;
  number.sourceIsEnemy = true;
  number.x = ex;
  number.y = ey;
  number.value = damage;
  number.param = crit ? 1 : 0;
  if (flags & DamageFlag::Dot) number.param |= 4;
  emit(number);

  if (attacker) {
    attacker->scoreAccum += applied * SCORE_PER_DAMAGE;
    if (!(flags & (DamageFlag::Proc | DamageFlag::Dot))) {
      // The enemy may have died; hooks still get to see the hit.
      if (Enemy* still = findEnemy(enemyId)) {
        if (Kit* kit = kitFor(*attacker)) {
          kit->onDealDamage(*this, *attacker, *still, damage, flags);
        }
        Infusions::onHitEnemy(*this, *attacker, *still, damage, flags);
      }
    }
  }
  return damage;
}

float AbilitySystem::damagePlayer(Player& player, float amount,
                                  uint32_t sourceEnemyId, uint32_t flags) {
  if (player.trainingDummy) return 0.0f;
  if (player.isDead() || player.godMode || amount <= 0.0f) return 0.0f;

  float damage = amount;
  Kit* kit = kitFor(player);
  if (kit && !(flags & DamageFlag::Dot)) {
    damage = kit->onTakeDamage(*this, player, damage, sourceEnemyId);
  }
  if (damage <= 0.0f || player.intangibleMs > 0.0f) return 0.0f;

  if (flags & DamageFlag::Dot) {
    damage *= effectManager->calculateModifiers(player.id, false)
                  .damageTakenMultiplier;
  } else {
    auto consumed = effectManager->consumeOnDamage(player.id, false, damage);
    if (consumed.guardPrevented > 0.0f) {
      notifyGuardLost(false, player.id, player.x, player.y,
                      consumed.guardPrevented);
    }
  }

  Enemy* source = findEnemy(sourceEnemyId);
  damage = Infusions::onPlayerDamaged(*this, player, damage, source, flags);
  if (damage <= 0.0f) return 0.0f;

  float total = damage;
  if (player.shield > 0.0f) {
    float absorbed = std::min(player.shield, damage);
    player.shield -= absorbed;
    damage -= absorbed;
    if (player.shield <= 0.0f) player.shieldMs = 0.0f;
  }
  player.health -= damage;

  GameEventPacket number;
  number.kind = GameEventKind::Damage;
  number.sourceId = player.id;
  number.sourceIsEnemy = false;
  number.x = player.x;
  number.y = player.y;
  number.value = total;
  number.param = 2;
  if (flags & DamageFlag::Dot) number.param |= 4;
  emit(number);

  if (player.health <= 0.0f) {
    if (kit && kit->onLethal(*this, player)) {
      player.health = std::max(player.health, 1.0f);
    } else {
      player.health = 0.0f;
    }
  }
  return total;
}

float AbilitySystem::healPlayer(uint32_t healerId, Player& target, float amount,
                                bool allowEcho) {
  if (target.isDead() || amount <= 0.0f) return 0.0f;
  float applied = std::min(target.maxHealth - target.health, amount);
  target.health += applied;

  if (applied >= 0.5f) {
    GameEventPacket number;
    number.kind = GameEventKind::Heal;
    number.sourceId = target.id;
    number.x = target.x;
    number.y = target.y;
    number.value = applied;
    emit(number);
  }

  Player* healer = findPlayer(healerId);
  if (healer) {
    // Healing someone else is worth more than topping yourself up.
    float rate = healer->id == target.id ? SCORE_PER_HEAL * 0.3f : SCORE_PER_HEAL;
    healer->scoreAccum += applied * rate;
    if (allowEcho) {
      if (Kit* kit = kitFor(*healer)) {
        kit->onHealDone(*this, *healer, target, amount);
      }
    }
  }
  return applied;
}

void AbilitySystem::applyToEnemy(Enemy& enemy, EffectType type, int stacks,
                                 uint32_t sourceId, float durationMs) {
  if (enemy.state == EnemyState::Dead || stacks <= 0) return;
  const EffectDefinition& def = EffectRegistry::get(type);
  float duration = durationMs >= 0.0f ? durationMs : def.baseDuration;

  // The boss cannot be turned on its own brood and shrugs off most control.
  if (enemy.type == Arena::EnemyKind::Boss) {
    if (type == EffectType::Confused || type == EffectType::Grappled) return;
    if (type == EffectType::Stunned || type == EffectType::Snared) {
      duration *= 0.3f;
    }
  }

  bool had = effectManager->getEnemyEffects(enemy.id).hasEffect(type);
  effectManager->applyEffect(enemy.id, type,
                             static_cast<uint8_t>(std::min(stacks, 255)),
                             duration, sourceId, enemies->getEnemies());
  bool has = effectManager->getEnemyEffects(enemy.id).hasEffect(type);

  if (has && !had && isControlEffect(type) && type != EffectType::Slow) {
    fxEffectText(enemy.x, enemy.y - enemy.radius * 2.0f, type);
  }
  if (has && isHardCrowdControl(type)) {
    if (Player* source = findPlayer(sourceId)) {
      Infusions::onHardCrowdControl(*this, *source, enemy);
    }
  }
}

void AbilitySystem::applyToPlayer(Player& player, EffectType type, int stacks,
                                  uint32_t sourceId, float durationMs) {
  if (player.isDead() || stacks <= 0) return;
  const EffectDefinition& def = EffectRegistry::get(type);
  if (def.category == EffectCategory::Debuff) {
    if (player.godMode || player.intangibleMs > 0.0f) return;
    if (isControlEffect(type) && Infusions::blocksCrowdControl(player)) return;
  }
  float duration = durationMs >= 0.0f ? durationMs : def.baseDuration;

  bool had = effectManager->getPlayerEffects(player.id).hasEffect(type);
  effectManager->applyEffect(player.id, type,
                             static_cast<uint8_t>(std::min(stacks, 255)),
                             duration, sourceId, players);
  bool has = effectManager->getPlayerEffects(player.id).hasEffect(type);
  if (has && !had &&
      (isControlEffect(type) || type == EffectType::Berserk ||
       type == EffectType::Stealth)) {
    fxEffectText(player.x, player.y - 60.0f, type);
  }
}

// ---------------------------------------------------------------------------
// Effect verbs
// ---------------------------------------------------------------------------

namespace {
float entityX(AbilitySystem& sys, uint32_t id, bool isEnemy, float* y) {
  if (isEnemy) {
    if (Enemy* e = sys.findEnemy(id)) {
      *y = e->y;
      return e->x;
    }
  } else if (Player* p = sys.findPlayer(id)) {
    *y = p->y;
    return p->x;
  }
  *y = 0.0f;
  return 0.0f;
}
}  // namespace

int AbilitySystem::amplifyEffect(uint32_t targetId, bool isEnemy,
                                 EffectType type, float factor) {
  ActiveEffects& fx = effectManager->mutableEffects(targetId, isEnemy);
  EffectInstance* effect = fx.findEffect(type);
  if (!effect) return 0;
  const EffectDefinition& def = EffectRegistry::get(type);
  int before = effect->stacks;
  int after = static_cast<int>(std::ceil(before * factor));
  if (def.maxStacks > 0) after = std::min<int>(after, def.maxStacks);
  after = std::min(after, 255);
  effect->stacks = static_cast<uint8_t>(after);
  effect->remainingDuration = std::max(effect->remainingDuration,
                                       def.baseDuration);
  float y = 0.0f;
  float x = entityX(*this, targetId, isEnemy, &y);
  fxEffectVerb(EffectVerb::Amplify, type, targetId, isEnemy, x, y,
               static_cast<float>(before), static_cast<float>(after));
  return after - before;
}

int AbilitySystem::spreadEffect(Enemy& from, EffectType type, float radius,
                                float fraction) {
  const EffectInstance* effect =
      effectManager->getEnemyEffects(from.id).findEffect(type);
  if (!effect) return 0;
  int give = std::max(1, static_cast<int>(effect->stacks * fraction));
  uint32_t sourceId = effect->sourceId;
  uint32_t fromId = from.id;
  float fx = from.x, fy = from.y;
  int reached = 0;
  enemiesInCircle(fx, fy, radius, [&](Enemy& other) {
    if (other.id == fromId) return;
    int before = effectManager->getEnemyEffects(other.id).getStacks(type);
    applyToEnemy(other, type, give, sourceId);
    Enemy* still = findEnemy(other.id);
    if (!still) return;
    int after = effectManager->getEnemyEffects(other.id).getStacks(type);
    if (after <= before) return;  // immune, or already at the cap
    fxEffectVerb(EffectVerb::Spread, type, other.id, true, still->x,
                 still->y, static_cast<float>(before),
                 static_cast<float>(after), fromId, fx, fy);
    reached++;
  });
  return reached;
}

int AbilitySystem::spreadEffectToAllies(Player& from, EffectType type,
                                        float radius, float fraction) {
  const EffectInstance* effect =
      effectManager->getPlayerEffects(from.id).findEffect(type);
  if (!effect) return 0;
  int give = std::max(1, static_cast<int>(effect->stacks * fraction));
  int reached = 0;
  playersInCircle(from.x, from.y, radius, [&](Player& ally) {
    if (ally.id == from.id || ally.isDead()) return;
    int before = effectManager->getPlayerEffects(ally.id).getStacks(type);
    applyToPlayer(ally, type, give, from.id);
    int after = effectManager->getPlayerEffects(ally.id).getStacks(type);
    if (after <= before) return;
    fxEffectVerb(EffectVerb::Spread, type, ally.id, false, ally.x, ally.y,
                 static_cast<float>(before), static_cast<float>(after),
                 from.id, from.x, from.y);
    reached++;
  });
  return reached;
}

int AbilitySystem::detonateEffect(uint32_t attackerId, Enemy& on,
                                  EffectType type, float damagePerStack) {
  ActiveEffects& fx = effectManager->mutableEffects(on.id, true);
  const EffectInstance* effect = fx.findEffect(type);
  if (!effect) return 0;
  int stacks = effect->stacks;
  fx.removeEffect(type);
  float radius = 60.0f + std::min(stacks, 40) * 5.0f;
  float damage = stacks * damagePerStack;
  float x = on.x, y = on.y;
  uint32_t onId = on.id;
  fxEffectVerb(EffectVerb::Detonate, type, onId, true, x, y,
               static_cast<float>(stacks), radius);
  enemiesInCircle(x, y, radius, [&](Enemy& enemy) {
    // The carrier takes the full blast, the crowd a little less.
    float scale = enemy.id == onId ? 1.0f : 0.7f;
    damageEnemy(attackerId, enemy, damage * scale, DamageFlag::Explosion);
  });
  return stacks;
}

namespace {
// Flip every effect of one category that has a mirror into that mirror.
int flipEffects(AbilitySystem& sys, EffectManager& manager, uint32_t sourceId,
                uint32_t targetId, bool isEnemy, EffectCategory from,
                EffectVerb verb) {
  struct Flip {
    EffectType was, becomes;
    int stacks;
  };
  std::vector<Flip> flips;
  for (const EffectInstance& effect :
       manager.mutableEffects(targetId, isEnemy).effects) {
    const EffectDefinition& def = EffectRegistry::get(effect.type);
    if (def.category != from || !def.hasOpposite) continue;
    EffectType mirror = def.oppositeEffect;
    if (EffectRegistry::getCategory(mirror) == from) continue;
    flips.push_back({effect.type, mirror, effect.stacks});
  }
  int flipped = 0;
  for (const Flip& flip : flips) {
    manager.removeEffect(targetId, isEnemy, flip.was);
    float x = 0.0f, y = 0.0f;
    int after = 0;
    if (isEnemy) {
      Enemy* enemy = sys.findEnemy(targetId);
      if (!enemy) break;
      sys.applyToEnemy(*enemy, flip.becomes, flip.stacks, sourceId);
      x = enemy->x;
      y = enemy->y;
      after = manager.getEnemyEffects(targetId).getStacks(flip.becomes);
    } else {
      Player* player = sys.findPlayer(targetId);
      if (!player) break;
      sys.applyToPlayer(*player, flip.becomes, flip.stacks, sourceId);
      x = player->x;
      y = player->y;
      after = manager.getPlayerEffects(targetId).getStacks(flip.becomes);
    }
    sys.fxEffectVerb(verb, flip.becomes, targetId, isEnemy, x, y,
                     static_cast<float>(flip.stacks),
                     static_cast<float>(after),
                     static_cast<uint32_t>(flip.was));
    flipped += flip.stacks;
  }
  return flipped;
}
}  // namespace

int AbilitySystem::corruptBuffs(uint32_t sourceId, uint32_t targetId,
                                bool isEnemy) {
  return flipEffects(*this, *effectManager, sourceId, targetId, isEnemy,
                     EffectCategory::Buff, EffectVerb::Corrupt);
}

int AbilitySystem::purifyDebuffs(uint32_t sourceId, uint32_t targetId,
                                 bool isEnemy) {
  return flipEffects(*this, *effectManager, sourceId, targetId, isEnemy,
                     EffectCategory::Debuff, EffectVerb::Purify);
}

void AbilitySystem::applyFrom(const Player& caster, Enemy& enemy,
                              EffectType type, int stacks) {
  int before = effectManager->getEnemyEffects(enemy.id).getStacks(type);
  applyToEnemy(enemy, type, stacks, caster.id);
  Enemy* still = findEnemy(enemy.id);
  if (!still) return;
  int after = effectManager->getEnemyEffects(enemy.id).getStacks(type);
  if (after == before) return;
  fxEffectVerb(EffectVerb::Apply, type, enemy.id, true, still->x, still->y,
               static_cast<float>(before), static_cast<float>(after),
               caster.id, caster.x, caster.y);
}

void AbilitySystem::applyFrom(const Player& caster, Player& ally,
                              EffectType type, int stacks) {
  int before = effectManager->getPlayerEffects(ally.id).getStacks(type);
  applyToPlayer(ally, type, stacks, caster.id);
  int after = effectManager->getPlayerEffects(ally.id).getStacks(type);
  if (after == before) return;
  fxEffectVerb(EffectVerb::Apply, type, ally.id, false, ally.x, ally.y,
               static_cast<float>(before), static_cast<float>(after),
               caster.id, caster.x, caster.y);
}

void AbilitySystem::grantShield(Player& player, float amount,
                                float durationMs) {
  if (player.isDead()) return;
  player.shield = std::max(player.shield, amount);
  player.shieldMs = std::max(player.shieldMs, durationMs);

  GameEventPacket bubble;
  bubble.kind = GameEventKind::ShieldBubble;
  bubble.sourceId = player.id;
  bubble.x = player.x;
  bubble.y = player.y;
  bubble.value = amount;
  bubble.value2 = durationMs;
  emit(bubble);
}

void AbilitySystem::notifyGuardLost(bool targetIsEnemy, uint32_t targetId,
                                    float x, float y, float prevented) {
  for (auto& [id, player] : players) {
    if (player.isDead()) continue;
    if (length(player.x - x, player.y - y) > KIT_HOOK_RANGE) continue;
    if (Kit* kit = kitFor(player)) {
      kit->onGuardLostNearby(*this, player, targetIsEnemy, targetId, prevented);
    }
  }
}

// ---------------------------------------------------------------------------
// Motion
// ---------------------------------------------------------------------------

void AbilitySystem::dashPlayer(Player& player, float dirX, float dirY,
                               float distance, float durationMs, bool ghost) {
  if (durationMs < 16.0f) durationMs = 16.0f;
  float speed = distance / (durationMs / 1000.0f);
  player.forcedVx = dirX * speed;
  player.forcedVy = dirY * speed;
  player.forcedMs = durationMs;
  player.forcedGhost = ghost;
  float endX = player.x + dirX * distance;
  float endY = player.y + dirY * distance;
  clampToWorld(endX, endY);
  Infusions::onDash(*this, player, player.x, player.y, endX, endY);
}

void AbilitySystem::blinkPlayer(Player& player, float x, float y) {
  float fromX = player.x, fromY = player.y;
  clampToWorld(x, y);
  if (collision &&
      !collision->isPositionValid(x, y, Arena::Tuning::PLAYER_RADIUS)) {
    // Blocked destination: stay put rather than land inside a wall.
    return;
  }
  player.x = x;
  player.y = y;
  player.vx = 0.0f;
  player.vy = 0.0f;
  // One tick of forced motion tells the client not to predict over the jump.
  player.forcedVx = 0.0f;
  player.forcedVy = 0.0f;
  player.forcedMs = 50.0f;
  Infusions::onDash(*this, player, fromX, fromY, x, y);
}

void AbilitySystem::knockbackEnemy(Enemy& enemy, float fromX, float fromY,
                                   float distance) {
  float dx = enemy.x - fromX;
  float dy = enemy.y - fromY;
  float d = length(dx, dy);
  if (d < 0.001f) {
    dx = 1.0f;
    dy = 0.0f;
    d = 1.0f;
  }
  enemies->pushEnemy(enemy, dx / d, dy / d, distance, 170.0f);
}

void AbilitySystem::pullEnemy(Enemy& enemy, float toX, float toY,
                              float maxDistance, float stopShort) {
  float dx = toX - enemy.x;
  float dy = toY - enemy.y;
  float d = length(dx, dy);
  float move = std::min(maxDistance, std::max(0.0f, d - stopShort));
  if (move <= 1.0f || d < 0.001f) return;
  enemies->pushEnemy(enemy, dx / d, dy / d, move, 240.0f);
}

void AbilitySystem::clampToWorld(float& x, float& y) const {
  float halfWidth = worldWidth / 2.0f;
  float halfHeight = worldHeight / 2.0f;
  if (halfWidth <= 0.0f || halfHeight <= 0.0f) return;
  float norm = std::abs(x) / halfWidth + std::abs(y) / halfHeight;
  if (norm > 1.0f) {
    x /= norm;
    y /= norm;
  }
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

std::vector<uint32_t> AbilitySystem::enemyIdsNear(float x, float y,
                                                  float radius) const {
  std::vector<uint32_t> ids;
  for (const auto& [id, enemy] : enemies->getEnemies()) {
    if (enemy.state == EnemyState::Dead) continue;
    float reach = radius + enemy.radius;
    float dx = enemy.x - x;
    float dy = enemy.y - y;
    if (dx * dx + dy * dy <= reach * reach) ids.push_back(id);
  }
  return ids;
}

void AbilitySystem::enemiesInCircle(float x, float y, float radius,
                                    const std::function<void(Enemy&)>& fn) {
  for (uint32_t id : enemyIdsNear(x, y, radius)) {
    Enemy* enemy = findEnemy(id);
    if (enemy && enemy->state != EnemyState::Dead) fn(*enemy);
  }
}

void AbilitySystem::enemiesInCone(float x, float y, float dirX, float dirY,
                                  float range, float halfAngleDeg,
                                  const std::function<void(Enemy&)>& fn) {
  float cosLimit = std::cos(halfAngleDeg * DEG_TO_RAD);
  for (uint32_t id : enemyIdsNear(x, y, range)) {
    Enemy* enemy = findEnemy(id);
    if (!enemy || enemy->state == EnemyState::Dead) continue;
    float dx = enemy->x - x;
    float dy = enemy->y - y;
    float d = length(dx, dy);
    // Anything overlapping the caster is always in the cone.
    bool inside = d <= enemy->radius + 14.0f;
    if (!inside) {
      // Widen the cone by the enemy's angular size so edges feel fair.
      float slack = std::atan2(enemy->radius, d);
      float cosAngle = (dx * dirX + dy * dirY) / d;
      float angle = std::acos(std::max(-1.0f, std::min(1.0f, cosAngle)));
      inside = angle - slack <= halfAngleDeg * DEG_TO_RAD;
      (void)cosLimit;
    }
    if (inside) fn(*enemy);
  }
}

void AbilitySystem::enemiesInLine(float x0, float y0, float x1, float y1,
                                  float halfWidth,
                                  const std::function<void(Enemy&)>& fn) {
  float sx = x1 - x0;
  float sy = y1 - y0;
  float segLen2 = sx * sx + sy * sy;
  float midX = (x0 + x1) / 2.0f;
  float midY = (y0 + y1) / 2.0f;
  float reach = std::sqrt(segLen2) / 2.0f + halfWidth;
  for (uint32_t id : enemyIdsNear(midX, midY, reach)) {
    Enemy* enemy = findEnemy(id);
    if (!enemy || enemy->state == EnemyState::Dead) continue;
    float t = 0.0f;
    if (segLen2 > 0.001f) {
      t = ((enemy->x - x0) * sx + (enemy->y - y0) * sy) / segLen2;
      t = std::max(0.0f, std::min(1.0f, t));
    }
    float px = x0 + sx * t;
    float py = y0 + sy * t;
    float d = length(enemy->x - px, enemy->y - py);
    if (d <= halfWidth + enemy->radius) fn(*enemy);
  }
}

void AbilitySystem::playersInCircle(float x, float y, float radius,
                                    const std::function<void(Player&)>& fn) {
  for (auto& [id, player] : players) {
    if (player.isDead() || player.characterId == 0) continue;
    float reach = radius + Arena::Tuning::PLAYER_RADIUS;
    float dx = player.x - x;
    float dy = player.y - y;
    if (dx * dx + dy * dy <= reach * reach) fn(player);
  }
}

void AbilitySystem::playersInCone(float x, float y, float dirX, float dirY,
                                  float range, float halfAngleDeg,
                                  const std::function<void(Player&)>& fn) {
  for (auto& [id, player] : players) {
    if (player.isDead() || player.characterId == 0) continue;
    float dx = player.x - x;
    float dy = player.y - y;
    float d = length(dx, dy);
    if (d > range + Arena::Tuning::PLAYER_RADIUS) continue;
    bool inside = d <= Arena::Tuning::PLAYER_RADIUS + 10.0f;
    if (!inside) {
      float slack = std::atan2(Arena::Tuning::PLAYER_RADIUS, d);
      float cosAngle = (dx * dirX + dy * dirY) / d;
      float angle = std::acos(std::max(-1.0f, std::min(1.0f, cosAngle)));
      inside = angle - slack <= halfAngleDeg * DEG_TO_RAD;
    }
    if (inside) fn(player);
  }
}

Player* AbilitySystem::nearestAlly(const Player& self, float x, float y,
                                   float maxDistance, bool includeSelf) {
  Player* best = nullptr;
  float bestDist = maxDistance;
  for (auto& [id, player] : players) {
    if (player.isDead() || player.characterId == 0) continue;
    if (!includeSelf && id == self.id) continue;
    float d = length(player.x - x, player.y - y);
    if (d <= bestDist) {
      bestDist = d;
      best = &player;
    }
  }
  return best;
}

Enemy* AbilitySystem::nearestEnemy(float x, float y, float maxDistance) {
  Enemy* best = nullptr;
  float bestDist = maxDistance;
  for (auto& [id, enemy] : enemies->getEnemies()) {
    if (enemy.state == EnemyState::Dead) continue;
    float d = length(enemy.x - x, enemy.y - y) - enemy.radius;
    if (d <= bestDist) {
      bestDist = d;
      best = &enemy;
    }
  }
  return best;
}

Player* AbilitySystem::findPlayer(uint32_t playerId) {
  if (playerId == 0) return nullptr;
  auto it = players.find(playerId);
  return it == players.end() ? nullptr : &it->second;
}

Enemy* AbilitySystem::findEnemy(uint32_t enemyId) {
  if (enemyId == 0) return nullptr;
  return enemies->find(enemyId);
}

int AbilitySystem::countEnemies(
    float x, float y, float radius,
    const std::function<bool(const Enemy&)>& predicate) {
  int count = 0;
  for (uint32_t id : enemyIdsNear(x, y, radius)) {
    Enemy* enemy = findEnemy(id);
    if (enemy && predicate(*enemy)) count++;
  }
  return count;
}

// ---------------------------------------------------------------------------
// Projectiles, zones, timers
// ---------------------------------------------------------------------------

uint32_t AbilitySystem::spawnProjectile(Projectile spec) {
  spec.id = nextSpawnId++;

  GameEventPacket spawn;
  spawn.kind = GameEventKind::ProjectileSpawn;
  spawn.sourceId = spec.ownerId;
  spawn.sourceIsEnemy = spec.fromEnemy;
  spawn.x = spec.x;
  spawn.y = spec.y;
  spawn.x2 = spec.vx;
  spawn.y2 = spec.vy;
  spawn.value = spec.radius;
  spawn.value2 = spec.lifeMs;
  spawn.param = spec.sprite;
  spawn.param2 = static_cast<uint8_t>(spec.theme);
  spawn.extraId = spec.id;
  emit(spawn);

  uint32_t id = spec.id;
  projectiles.push_back(std::move(spec));
  return id;
}

uint32_t AbilitySystem::spawnZone(Zone spec) {
  spec.id = nextSpawnId++;
  // First tick lands quickly so walking through a zone always registers.
  spec.tickTimer = std::min(spec.tickMs, 120.0f);

  GameEventPacket spawn;
  spawn.kind = GameEventKind::ZoneSpawn;
  spawn.sourceId = spec.ownerId;
  spawn.sourceIsEnemy = spec.fromEnemy;
  spawn.x = spec.x;
  spawn.y = spec.y;
  spawn.value = spec.radius;
  spawn.value2 = spec.lifeMs;
  spawn.param = spec.variant;
  spawn.param2 = static_cast<uint8_t>(spec.theme);
  spawn.extraId = spec.id;
  emit(spawn);

  uint32_t id = spec.id;
  zones.push_back(std::move(spec));
  return id;
}

void AbilitySystem::schedule(float delayMs, std::function<void()> fn) {
  timers.push_back({timeMs + delayMs, std::move(fn)});
}

void AbilitySystem::runTimers() {
  if (timers.empty()) return;
  // Callbacks may schedule more timers: pull the due ones out first.
  std::vector<Timer> due;
  for (size_t i = 0; i < timers.size();) {
    if (timers[i].atMs <= timeMs) {
      due.push_back(std::move(timers[i]));
      timers[i] = std::move(timers.back());
      timers.pop_back();
    } else {
      i++;
    }
  }
  std::sort(due.begin(), due.end(),
            [](const Timer& a, const Timer& b) { return a.atMs < b.atMs; });
  for (auto& timer : due) timer.fn();
}

void AbilitySystem::updateProjectiles(float dtMs) {
  if (projectiles.empty()) return;
  float dt = dtMs / 1000.0f;

  // Callbacks can spawn projectiles, so work on a detached batch and merge
  // the survivors back afterwards.
  std::vector<Projectile> batch;
  batch.swap(projectiles);
  std::vector<Projectile> survivors;
  survivors.reserve(batch.size());

  for (Projectile& p : batch) {
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    p.lifeMs -= dtMs;
    bool done = p.lifeMs <= 0.0f;

    float halfWidth = worldWidth / 2.0f;
    float halfHeight = worldHeight / 2.0f;
    if (halfWidth > 0.0f && halfHeight > 0.0f &&
        std::abs(p.x) / halfWidth + std::abs(p.y) / halfHeight > 1.05f) {
      done = true;
    }

    if (!done) {
      if (p.fromEnemy) {
        for (auto& [id, player] : players) {
          // Intangible players still "catch" shots (damagePlayer ignores
          // them) so dodge windows like Wade's Slip can react to projectiles.
          if (player.isDead() || player.characterId == 0) continue;
          float reach = p.radius + Arena::Tuning::PLAYER_RADIUS;
          // All combat geometry lives on the ground plane; the client
          // draws projectiles lifted to chest height.
          float dx = player.x - p.x;
          float dy = player.y - p.y;
          if (dx * dx + dy * dy > reach * reach) continue;
          if (p.onHitPlayer) p.onHitPlayer(p, player);
          done = true;
          break;
        }
      } else {
        for (uint32_t id : enemyIdsNear(p.x, p.y, p.radius)) {
          if (std::find(p.alreadyHit.begin(), p.alreadyHit.end(), id) !=
              p.alreadyHit.end()) {
            continue;
          }
          Enemy* enemy = findEnemy(id);
          if (!enemy || enemy->state == EnemyState::Dead) continue;
          p.alreadyHit.push_back(id);
          if (p.onHitEnemy) p.onHitEnemy(p, *enemy);
          if (!p.pierce) {
            done = true;
            break;
          }
        }
      }
    }

    if (done) {
      if (p.onEnd) p.onEnd(p);
      GameEventPacket end;
      end.kind = GameEventKind::ProjectileEnd;
      end.extraId = p.id;
      end.x = p.x;
      end.y = p.y;
      end.param2 = static_cast<uint8_t>(p.theme);
      emit(end);
    } else {
      survivors.push_back(std::move(p));
    }
  }

  // Anything spawned during callbacks is already in `projectiles`.
  for (Projectile& p : survivors) projectiles.push_back(std::move(p));
}

void AbilitySystem::updateZones(float dtMs) {
  if (zones.empty()) return;

  std::vector<Zone> batch;
  batch.swap(zones);
  std::vector<Zone> survivors;
  survivors.reserve(batch.size());

  for (Zone& zone : batch) {
    zone.lifeMs -= dtMs;
    zone.tickTimer -= dtMs;
    if (zone.tickTimer <= 0.0f) {
      zone.tickTimer += zone.tickMs;
      if (zone.onEnemyTick) {
        for (uint32_t id : enemyIdsNear(zone.x, zone.y, zone.radius)) {
          Enemy* enemy = findEnemy(id);
          if (enemy && enemy->state != EnemyState::Dead) {
            zone.onEnemyTick(zone, *enemy);
          }
        }
      }
      if (zone.onPlayerTick) {
        for (auto& [id, player] : players) {
          if (player.isDead()) continue;
          float reach = zone.radius + Arena::Tuning::PLAYER_RADIUS * 0.5f;
          float dx = player.x - zone.x;
          float dy = player.y - zone.y;
          if (dx * dx + dy * dy <= reach * reach) zone.onPlayerTick(zone, player);
        }
      }
    }

    if (zone.lifeMs <= 0.0f) {
      if (zone.onEnd) zone.onEnd(zone);
      GameEventPacket end;
      end.kind = GameEventKind::ZoneEnd;
      end.extraId = zone.id;
      emit(end);
    } else {
      survivors.push_back(std::move(zone));
    }
  }

  for (Zone& zone : survivors) zones.push_back(std::move(zone));
}

// ---------------------------------------------------------------------------
// VFX
// ---------------------------------------------------------------------------

void AbilitySystem::emit(const GameEventPacket& packet) {
  if (emitFn) emitFn(packet);
}

void AbilitySystem::fxSlash(float x, float y, float dirX, float dirY,
                            float radius, float arcDeg, VfxTheme theme) {
  GameEventPacket p;
  p.kind = GameEventKind::Slash;
  p.x = x;
  p.y = y;
  p.x2 = dirX;
  p.y2 = dirY;
  p.value = radius;
  p.value2 = arcDeg;
  p.param2 = static_cast<uint8_t>(theme);
  emit(p);
}

void AbilitySystem::fxCone(float x, float y, float dirX, float dirY,
                           float radius, float arcDeg, VfxTheme theme) {
  GameEventPacket p;
  p.kind = GameEventKind::Cone;
  p.x = x;
  p.y = y;
  p.x2 = dirX;
  p.y2 = dirY;
  p.value = radius;
  p.value2 = arcDeg;
  p.param2 = static_cast<uint8_t>(theme);
  emit(p);
}

void AbilitySystem::fxBurst(float x, float y, float radius, VfxTheme theme) {
  GameEventPacket p;
  p.kind = GameEventKind::Burst;
  p.x = x;
  p.y = y;
  p.value = radius;
  p.param2 = static_cast<uint8_t>(theme);
  emit(p);
}

void AbilitySystem::fxRing(float x, float y, float radius, VfxTheme theme) {
  GameEventPacket p;
  p.kind = GameEventKind::Ring;
  p.x = x;
  p.y = y;
  p.value = radius;
  p.param2 = static_cast<uint8_t>(theme);
  emit(p);
}

void AbilitySystem::fxBeam(float x0, float y0, float x1, float y1, float width,
                           float durationMs, VfxTheme theme) {
  GameEventPacket p;
  p.kind = GameEventKind::Beam;
  p.x = x0;
  p.y = y0;
  p.x2 = x1;
  p.y2 = y1;
  p.value = width;
  p.value2 = durationMs;
  p.param2 = static_cast<uint8_t>(theme);
  emit(p);
}

void AbilitySystem::fxLob(float x0, float y0, float x1, float y1, float radius,
                          float flightMs, VfxTheme theme, uint8_t sprite) {
  GameEventPacket p;
  p.kind = GameEventKind::Lob;
  p.x = x0;
  p.y = y0;
  p.x2 = x1;
  p.y2 = y1;
  p.value = radius;
  p.value2 = flightMs;
  p.param = sprite;
  p.param2 = static_cast<uint8_t>(theme);
  emit(p);
}

void AbilitySystem::fxTelegraphCircle(float x, float y, float radius,
                                      float durationMs, VfxTheme theme) {
  GameEventPacket p;
  p.kind = GameEventKind::TelegraphCircle;
  p.x = x;
  p.y = y;
  p.value = radius;
  p.value2 = durationMs;
  p.param2 = static_cast<uint8_t>(theme);
  emit(p);
}

void AbilitySystem::fxTelegraphCone(float x, float y, float dirX, float dirY,
                                    float radius, float halfAngleDeg,
                                    float durationMs) {
  GameEventPacket p;
  p.kind = GameEventKind::TelegraphCone;
  p.x = x;
  p.y = y;
  p.x2 = dirX;
  p.y2 = dirY;
  p.value = radius;
  p.value2 = durationMs;
  p.param = static_cast<uint8_t>(std::min(180.0f, halfAngleDeg));
  p.param2 = static_cast<uint8_t>(VfxTheme::Danger);
  emit(p);
}

void AbilitySystem::fxDashTrail(float x0, float y0, float x1, float y1,
                                VfxTheme theme) {
  GameEventPacket p;
  p.kind = GameEventKind::DashTrail;
  p.x = x0;
  p.y = y0;
  p.x2 = x1;
  p.y2 = y1;
  p.param2 = static_cast<uint8_t>(theme);
  emit(p);
}

void AbilitySystem::fxTether(float x0, float y0, float x1, float y1,
                             float durationMs, VfxTheme theme) {
  GameEventPacket p;
  p.kind = GameEventKind::Tether;
  p.x = x0;
  p.y = y0;
  p.x2 = x1;
  p.y2 = y1;
  p.value2 = durationMs;
  p.param2 = static_cast<uint8_t>(theme);
  emit(p);
}

void AbilitySystem::fxEffectText(float x, float y, EffectType type) {
  GameEventPacket p;
  p.kind = GameEventKind::EffectText;
  p.x = x;
  p.y = y;
  p.param = static_cast<uint8_t>(type);
  emit(p);
}

void AbilitySystem::fxEffectVerb(EffectVerb verb, EffectType type,
                                 uint32_t targetId, bool isEnemy, float x,
                                 float y, float before, float after,
                                 uint32_t extraId, float x2, float y2) {
  GameEventPacket p;
  p.kind = GameEventKind::EffectVerb;
  p.sourceId = targetId;
  p.sourceIsEnemy = isEnemy;
  p.x = x;
  p.y = y;
  p.x2 = x2;
  p.y2 = y2;
  p.value = before;
  p.value2 = after;
  p.param = static_cast<uint8_t>(verb);
  p.param2 = static_cast<uint8_t>(type);
  p.extraId = extraId;
  emit(p);
}

void AbilitySystem::fxAnnounce(Announcement announcement) {
  GameEventPacket p;
  p.kind = GameEventKind::Announcement;
  p.param = static_cast<uint8_t>(announcement);
  emit(p);
}

float AbilitySystem::random01() {
  return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
}

float AbilitySystem::randomRange(float lo, float hi) {
  return std::uniform_real_distribution<float>(lo, hi)(rng);
}
