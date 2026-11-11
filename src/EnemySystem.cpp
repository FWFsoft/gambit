#include "EnemySystem.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "AbilitySystem.h"
#include "EffectManager.h"
#include "Logger.h"

using Arena::EnemyKind;
using Arena::EnemyTier;
using Arena::Genus;

namespace {

constexpr float DEAD_LINGER_MS = 1200.0f;  // corpse stays for the death anim
constexpr float AGGRO_RANGE = 620.0f;
constexpr float CAMP_AGGRO_RANGE = 300.0f;
constexpr float SHIP_REACH = 60.0f;
constexpr float PI = 3.14159265f;

VfxTheme themeFor(Genus genus) {
  switch (genus) {
    case Genus::Venoxarid:
      return VfxTheme::Poison;
    case Genus::Cerebrian:
      return VfxTheme::Psychic;
    case Genus::Frigidariant:
      return VfxTheme::Ice;
    case Genus::Magnarok:
      return VfxTheme::Fire;
    default:
      return VfxTheme::Shadow;
  }
}

}  // namespace

EnemySystem::EnemySystem(const std::vector<EnemySpawn>& spawns)
    : spawns(spawns), nextEnemyId(1), accumulatedTime(0.0f) {
  Logger::info("EnemySystem initialized with " + std::to_string(spawns.size()) +
               " spawn points");
}

void EnemySystem::spawnAllEnemies() {
  for (size_t i = 0; i < spawns.size(); ++i) {
    const auto& spawnPoint = spawns[i];
    SpawnOptions options;
    options.campGuard = true;
    uint32_t id = spawn(spawnPoint.type, spawnPoint.x, spawnPoint.y, options);
    enemies[id].spawnIndex = static_cast<uint32_t>(i);
  }
}

uint32_t EnemySystem::spawn(EnemyKind kind, float x, float y,
                            const SpawnOptions& options) {
  const Arena::EnemyKindDef& def = Arena::enemyDef(kind);

  Enemy enemy;
  enemy.id = nextEnemyId++;
  enemy.type = kind;
  enemy.state = EnemyState::Idle;
  enemy.x = x;
  enemy.y = y;
  enemy.homeX = x;
  enemy.homeY = y;
  enemy.maxHealth = def.maxHealth * options.healthScale;
  enemy.damage = def.damage * options.damageScale;
  enemy.speed = def.speed;
  enemy.radius = def.radius;
  enemy.attackRange = def.attackRange;
  enemy.detectionRange = AGGRO_RANGE;
  enemy.targetShip = options.targetShip;
  enemy.campGuard = options.campGuard;
  enemy.elite = options.elite;
  if (options.elite) {
    enemy.maxHealth *= 2.2f;
    enemy.damage *= 1.3f;
    enemy.radius *= 1.15f;
  }
  enemy.health = enemy.maxHealth;
  // Stagger first attacks so a pack does not strike on the same frame.
  std::uniform_real_distribution<float> stagger(0.0f, 500.0f);
  enemy.cooldownMs = 300.0f + stagger(rng);

  uint32_t id = enemy.id;
  enemies[id] = enemy;
  return id;
}

void EnemySystem::despawnAll(bool includeCampGuards) {
  for (auto it = enemies.begin(); it != enemies.end();) {
    if (includeCampGuards || !it->second.campGuard) {
      it = enemies.erase(it);
    } else {
      ++it;
    }
  }
  pendingDeaths.clear();
}

Enemy* EnemySystem::find(uint32_t enemyId) {
  auto it = enemies.find(enemyId);
  return it == enemies.end() ? nullptr : &it->second;
}

int EnemySystem::livingCount() const {
  int count = 0;
  for (const auto& [id, enemy] : enemies) {
    if (enemy.state != EnemyState::Dead) count++;
  }
  return count;
}

std::vector<EnemySystem::EnemyDeath> EnemySystem::takeDeaths() {
  std::vector<EnemyDeath> out;
  out.swap(pendingDeaths);
  return out;
}

void EnemySystem::update(float deltaTime,
                         std::unordered_map<uint32_t, Player>& players,
                         EffectManager* effectManager) {
  accumulatedTime += deltaTime;

  // Corpses linger briefly so clients can play the death animation.
  for (auto it = enemies.begin(); it != enemies.end();) {
    Enemy& enemy = it->second;
    if (enemy.state == EnemyState::Dead) {
      enemy.deadMs += deltaTime;
      if (enemy.deadMs >= DEAD_LINGER_MS) {
        it = enemies.erase(it);
        continue;
      }
    }
    ++it;
  }

  // Attacks can spawn enemies (boss summons), so iterate a snapshot of ids.
  std::vector<uint32_t> ids;
  ids.reserve(enemies.size());
  for (const auto& [id, enemy] : enemies) {
    if (enemy.state != EnemyState::Dead) ids.push_back(id);
  }
  for (uint32_t id : ids) {
    auto it = enemies.find(id);
    if (it == enemies.end() || it->second.state == EnemyState::Dead) continue;
    updateEnemy(it->second, players, deltaTime, effectManager);
  }
}

EnemySystem::Target EnemySystem::pickTarget(
    const Enemy& enemy, const std::unordered_map<uint32_t, Player>& players,
    bool confused) const {
  Target target;

  if (confused) {
    // Confused: attack the nearest other enemy.
    float best = 400.0f;
    for (const auto& [id, other] : enemies) {
      if (id == enemy.id || other.state == EnemyState::Dead) continue;
      float d = distance(enemy.x, enemy.y, other.x, other.y);
      if (d < best) {
        best = d;
        target.valid = true;
        target.isEnemy = true;
        target.id = id;
        target.x = other.x;
        target.y = other.y;
        target.radius = other.radius;
      }
    }
    if (target.valid) return target;
  }

  // Nearest living, visible player. Marked players pull aggro from afar.
  float aggro = enemy.campGuard ? CAMP_AGGRO_RANGE : enemy.detectionRange;
  if (enemy.type == EnemyKind::Boss) aggro = 4000.0f;
  // Anything that has been hit knows where the fight is.
  if (enemy.lastHitBy != 0) aggro = std::max(aggro, 900.0f);

  const Player* bestPlayer = nullptr;
  float bestDist = aggro;
  for (const auto& [id, player] : players) {
    if (player.isDead()) continue;
    if (combat && !combat->isTargetable(player)) continue;
    float d = distance(enemy.x, enemy.y, player.x, player.y);
    if (combat && combat->effects().getPlayerEffects(id).hasEffect(
                      EffectType::Marked)) {
      d *= 0.4f;  // marked targets look much closer
    }
    if (d < bestDist) {
      bestDist = d;
      bestPlayer = &player;
    }
  }

  if (enemy.targetShip && shipAttackable) {
    // Ship raiders only peel off for players standing in their way.
    float shipDist = distance(enemy.x, enemy.y, shipX, shipY);
    if (bestPlayer == nullptr || bestDist > 170.0f || shipDist < 140.0f) {
      target.valid = true;
      target.isShip = true;
      target.x = shipX;
      target.y = shipY;
      target.radius = SHIP_REACH;
      return target;
    }
  }

  if (bestPlayer != nullptr) {
    target.valid = true;
    target.id = bestPlayer->id;
    target.x = bestPlayer->x;
    target.y = bestPlayer->y;
    target.radius = Arena::Tuning::PLAYER_RADIUS;
  }
  return target;
}

void EnemySystem::updateEnemy(Enemy& enemy,
                              std::unordered_map<uint32_t, Player>& players,
                              float deltaTime, EffectManager* effectManager) {
  const Arena::EnemyKindDef& def = Arena::enemyDef(enemy.type);
  float dt = deltaTime / 1000.0f;

  EffectManager::StatModifiers mods;
  bool confused = false;
  if (effectManager) {
    mods = effectManager->calculateModifiers(enemy.id, true);
    confused =
        effectManager->getEnemyEffects(enemy.id).hasEffect(EffectType::Confused);
  }

  enemy.netFlags = 0;
  if (!mods.canAct) enemy.netFlags |= EnemyFlags::Stunned;
  if (confused) enemy.netFlags |= EnemyFlags::Confused;
  if (enemy.elite) enemy.netFlags |= EnemyFlags::Elite;
  if (enemy.targetShip) enemy.netFlags |= EnemyFlags::AttackingShip;
  if (effectManager &&
      effectManager->getEnemyEffects(enemy.id).hasEffect(EffectType::Grappled)) {
    enemy.netFlags |= EnemyFlags::Grappled;
    enemy.netFlags &= ~EnemyFlags::Stunned;
    if (enemy.heldLow) enemy.netFlags |= EnemyFlags::HeldLow;
  }

  enemy.cooldownMs = std::max(0.0f, enemy.cooldownMs - deltaTime);

  // Knockback / pull overrides everything else.
  if (enemy.knockMs > 0.0f) {
    float step = std::min(enemy.knockMs, deltaTime) / 1000.0f;
    enemy.x += enemy.knockVx * step;
    enemy.y += enemy.knockVy * step;
    enemy.knockMs -= deltaTime;
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
    if (combat) combat->clampToWorld(enemy.x, enemy.y);
    // Being thrown around interrupts a windup (bosses shrug it off).
    if (enemy.state == EnemyState::Attack && enemy.type != EnemyKind::Boss) {
      enemy.state = EnemyState::Chase;
      enemy.windupMs = 0.0f;
      enemy.cooldownMs = std::max(enemy.cooldownMs, 400.0f);
    }
    return;
  }

  // Stunned: drop whatever it was doing.
  if (!mods.canAct) {
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
    if (enemy.state == EnemyState::Attack) {
      enemy.state = EnemyState::Chase;
      enemy.windupMs = 0.0f;
      enemy.cooldownMs = std::max(enemy.cooldownMs, 500.0f);
    }
    return;
  }

  // Mid-windup: stand still until the hit lands.
  if (enemy.state == EnemyState::Attack) {
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
    enemy.windupMs -= deltaTime;
    if (enemy.windupMs <= 0.0f) {
      resolveAttack(enemy, players, effectManager);
      // resolveAttack can kill this enemy (reflected damage, traps).
      if (enemy.state != EnemyState::Dead) {
        enemy.state = EnemyState::Chase;
        enemy.cooldownMs = def.cooldownMs;
      }
    }
    return;
  }

  if (enemy.passive) {
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
    return;
  }

  Target target = pickTarget(enemy, players, confused);
  if (!target.valid) {
    enemy.state = EnemyState::Idle;
    enemy.targetPlayerId = 0;
    // Drift home (camp guards) or toward the ship (everything else).
    float goalX = enemy.campGuard ? enemy.homeX : shipX;
    float goalY = enemy.campGuard ? enemy.homeY : shipY;
    float dx = goalX - enemy.x;
    float dy = goalY - enemy.y;
    float d = std::sqrt(dx * dx + dy * dy);
    float stopAt = enemy.campGuard ? 20.0f : 260.0f;
    if (d > stopAt && mods.canMove) {
      float speed = enemy.speed * 0.6f * mods.movementSpeedMultiplier;
      enemy.vx = dx / d * speed;
      enemy.vy = dy / d * speed;
      enemy.x += enemy.vx * dt;
      enemy.y += enemy.vy * dt;
    } else {
      enemy.vx = 0.0f;
      enemy.vy = 0.0f;
    }
    separate(enemy);
    return;
  }

  enemy.state = EnemyState::Chase;
  enemy.targetPlayerId = (target.isShip || target.isEnemy) ? 0 : target.id;

  float dx = target.x - enemy.x;
  float dy = target.y - enemy.y;
  float dist = std::sqrt(dx * dx + dy * dy);
  float reach = enemy.attackRange + target.radius * 0.5f;
  if (target.isShip) reach = SHIP_REACH + enemy.attackRange;

  if (dist <= reach && enemy.cooldownMs <= 0.0f) {
    beginAttack(enemy, target);
    return;
  }

  // Movement. Casters hold their range and back off if crowded.
  float moveX = 0.0f, moveY = 0.0f;
  if (dist > 0.001f) {
    moveX = dx / dist;
    moveY = dy / dist;
  }
  bool hold = false;
  if (def.tier == EnemyTier::Caster && !target.isShip) {
    if (dist < enemy.attackRange * 0.45f) {
      moveX = -moveX;
      moveY = -moveY;
    } else if (dist <= reach) {
      hold = true;
    }
  } else if (dist <= reach * 0.8f) {
    hold = true;  // in range, waiting on cooldown
  }

  if (hold || !mods.canMove) {
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
  } else {
    float speed = enemy.speed * mods.movementSpeedMultiplier;
    enemy.vx = moveX * speed;
    enemy.vy = moveY * speed;
    enemy.x += enemy.vx * dt;
    enemy.y += enemy.vy * dt;
  }
  separate(enemy);
  if (combat) combat->clampToWorld(enemy.x, enemy.y);
}

void EnemySystem::separate(Enemy& enemy) {
  // Cheap crowd separation so packs surround instead of stacking.
  for (auto& [id, other] : enemies) {
    if (id == enemy.id || other.state == EnemyState::Dead) continue;
    float dx = enemy.x - other.x;
    float dy = enemy.y - other.y;
    float minDist = (enemy.radius + other.radius) * 0.85f;
    float d2 = dx * dx + dy * dy;
    if (d2 >= minDist * minDist) continue;
    float d = std::sqrt(d2);
    if (d < 0.01f) {
      // Perfectly stacked: nudge along a stable direction.
      dx = (enemy.id % 2 == 0) ? 1.0f : -1.0f;
      dy = (enemy.id % 3 == 0) ? 1.0f : -0.5f;
      d = std::sqrt(dx * dx + dy * dy);
    }
    float push = (minDist - d) * 0.5f;
    // Heavier enemies give way less.
    float share = other.radius / (enemy.radius + other.radius);
    enemy.x += dx / d * push * share * 2.0f;
    enemy.y += dy / d * push * share * 2.0f;
  }
}

void EnemySystem::beginAttack(Enemy& enemy, const Target& target) {
  const Arena::EnemyKindDef& def = Arena::enemyDef(enemy.type);

  enemy.state = EnemyState::Attack;
  enemy.windupTotalMs = def.windupMs;
  enemy.windupMs = def.windupMs;
  enemy.aimX = target.x;
  enemy.aimY = target.y;
  enemy.vx = 0.0f;
  enemy.vy = 0.0f;

  if (!combat) return;

  float dx = target.x - enemy.x;
  float dy = target.y - enemy.y;
  float d = std::sqrt(dx * dx + dy * dy);
  float dirX = d > 0.001f ? dx / d : 1.0f;
  float dirY = d > 0.001f ? dy / d : 0.0f;

  GameEventPacket attack;
  attack.kind = GameEventKind::EnemyAttack;
  attack.sourceId = enemy.id;
  attack.sourceIsEnemy = true;
  attack.x = enemy.x;
  attack.y = enemy.y;
  attack.x2 = target.x;
  attack.y2 = target.y;
  attack.value = def.windupMs;
  combat->emit(attack);

  switch (def.tier) {
    case EnemyTier::Swarmer:
      // Quick bite: a small cone so the direction is readable.
      combat->fxTelegraphCone(enemy.x, enemy.y, dirX, dirY,
                              enemy.attackRange + 26.0f, 40.0f, def.windupMs);
      break;
    case EnemyTier::Caster:
      // The shot itself is the tell; a thin cone shows the firing line.
      combat->fxTelegraphCone(enemy.x, enemy.y, dirX, dirY, 90.0f, 12.0f,
                              def.windupMs);
      break;
    case EnemyTier::Brute: {
      // Slam lands a little in front of the brute.
      enemy.aimX = enemy.x + dirX * 55.0f;
      enemy.aimY = enemy.y + dirY * 55.0f;
      combat->fxTelegraphCircle(enemy.aimX, enemy.aimY, 100.0f, def.windupMs);
      break;
    }
    case EnemyTier::Boss: {
      bool enraged = enemy.health < enemy.maxHealth * 0.35f;
      enemy.bossMove = (enemy.bossMove + 1) % 4;
      float windup = enraged ? def.windupMs * 0.75f : def.windupMs;
      enemy.windupMs = enemy.windupTotalMs = windup;
      if (enemy.bossMove == 0) {
        // Slam on the target's position.
        combat->fxTelegraphCircle(enemy.aimX, enemy.aimY, 190.0f, windup);
      } else if (enemy.bossMove == 1) {
        // Nova: ring of projectiles from the boss.
        combat->fxTelegraphCircle(enemy.x, enemy.y, 120.0f, windup);
      } else if (enemy.bossMove == 2) {
        // Sweep: wide cone in front.
        combat->fxTelegraphCone(enemy.x, enemy.y, dirX, dirY, 330.0f, 50.0f,
                                windup);
      } else {
        // Summon: brood erupts around the boss.
        combat->fxTelegraphCircle(enemy.x, enemy.y, 240.0f, windup,
                                  VfxTheme::Shadow);
      }
      break;
    }
  }
}

void EnemySystem::resolveAttack(Enemy& enemy,
                                std::unordered_map<uint32_t, Player>& players,
                                EffectManager* effectManager) {
  if (!combat) return;
  const Arena::EnemyKindDef& def = Arena::enemyDef(enemy.type);
  Genus genus = def.genus;
  VfxTheme theme = themeFor(genus);
  uint32_t selfId = enemy.id;

  float damage = enemy.damage;
  bool confused = false;
  if (effectManager) {
    auto mods = effectManager->calculateModifiers(enemy.id, true);
    damage *= mods.damageDealtMultiplier;
    confused = effectManager->getEnemyEffects(enemy.id)
                   .hasEffect(EffectType::Confused);
  }

  float dx = enemy.aimX - enemy.x;
  float dy = enemy.aimY - enemy.y;
  float d = std::sqrt(dx * dx + dy * dy);
  float dirX = d > 0.001f ? dx / d : 1.0f;
  float dirY = d > 0.001f ? dy / d : 0.0f;

  // Genus flavour applied to whoever gets hit.
  auto afflict = [this, genus, selfId](Player& victim, bool heavy) {
    switch (genus) {
      case Genus::Venoxarid:
        combat->applyToPlayer(victim, EffectType::Wound, heavy ? 2 : 1, selfId);
        break;
      case Genus::Cerebrian:
        combat->applyToPlayer(victim, EffectType::Weakened, 1, selfId);
        if (heavy) {
          combat->applyToPlayer(victim, EffectType::Silenced, 1, selfId,
                                1500.0f);
        }
        break;
      case Genus::Frigidariant:
        combat->applyToPlayer(victim, EffectType::Slow, heavy ? 2 : 1, selfId);
        if (heavy) {
          combat->applyToPlayer(victim, EffectType::Snared, 1, selfId, 900.0f);
        }
        break;
      case Genus::Magnarok:
        if (heavy) combat->applyToPlayer(victim, EffectType::Wound, 2, selfId);
        break;
      default:
        break;
    }
  };

  if (def.tier == EnemyTier::Boss) {
    resolveBossAttack(enemy, players);
    return;
  }

  // Confused enemies hit their neighbours instead of players.
  if (confused) {
    combat->fxBurst(enemy.aimX, enemy.aimY, 50.0f, VfxTheme::Psychic);
    std::vector<uint32_t> victims;
    for (auto& [id, other] : enemies) {
      if (id == selfId || other.state == EnemyState::Dead) continue;
      if (distance(enemy.aimX, enemy.aimY, other.x, other.y) <=
          60.0f + other.radius) {
        victims.push_back(id);
      }
    }
    for (uint32_t id : victims) {
      if (Enemy* other = find(id)) {
        combat->damageEnemy(0, *other, damage * 2.0f, DamageFlag::Proc);
      }
    }
    return;
  }

  // Raiders chewing on the ship.
  if (enemy.targetShip && shipAttackable &&
      distance(enemy.x, enemy.y, shipX, shipY) <=
          SHIP_REACH + enemy.attackRange + 30.0f &&
      distance(enemy.aimX, enemy.aimY, shipX, shipY) < 5.0f) {
    combat->fxBurst(shipX + dirX * -20.0f, shipY + dirY * -20.0f, 40.0f, theme);
    if (onShipDamaged) onShipDamaged(damage);
    return;
  }

  switch (def.tier) {
    case EnemyTier::Swarmer: {
      float reach = enemy.attackRange + 26.0f;
      combat->fxSlash(enemy.x, enemy.y, dirX, dirY, reach, 80.0f, theme);
      combat->playersInCone(enemy.x, enemy.y, dirX, dirY, reach, 45.0f,
                            [&](Player& victim) {
                              float dealt = combat->damagePlayer(
                                  victim, damage, selfId, DamageFlag::Melee);
                              if (dealt > 0.0f) afflict(victim, false);
                            });
      break;
    }
    case EnemyTier::Caster: {
      AbilitySystem::Projectile shot;
      shot.fromEnemy = true;
      shot.ownerId = selfId;
      shot.x = enemy.x + dirX * 20.0f;
      shot.y = enemy.y + dirY * 20.0f;
      const float speed = 380.0f;
      shot.vx = dirX * speed;
      shot.vy = dirY * speed;
      shot.radius = 11.0f;
      shot.lifeMs = 1500.0f;
      shot.theme = theme;
      AbilitySystem* sys = combat;
      shot.onHitPlayer = [sys, damage, selfId, afflict](
                             AbilitySystem::Projectile&, Player& victim) {
        float dealt = sys->damagePlayer(victim, damage, selfId);
        if (dealt > 0.0f) afflict(victim, false);
      };
      // Poison and fire shots leave a hazard where they land.
      if (genus == Genus::Venoxarid || genus == Genus::Magnarok) {
        shot.onEnd = [sys, genus, theme, selfId](AbilitySystem::Projectile& p) {
          AbilitySystem::Zone pool;
          pool.fromEnemy = true;
          pool.ownerId = selfId;
          pool.x = p.x;
          pool.y = p.y;
          pool.radius = 55.0f;
          pool.lifeMs = 3000.0f;
          pool.tickMs = 600.0f;
          pool.theme = theme;
          pool.variant = 1;
          pool.onPlayerTick = [sys, genus, selfId](AbilitySystem::Zone&,
                                                   Player& victim) {
            if (genus == Genus::Venoxarid) {
              sys->applyToPlayer(victim, EffectType::Wound, 1, selfId);
            } else {
              sys->damagePlayer(victim, 6.0f, selfId, DamageFlag::Dot);
            }
          };
          sys->spawnZone(pool);
        };
      }
      combat->spawnProjectile(shot);
      break;
    }
    case EnemyTier::Brute: {
      const float radius = 100.0f;
      combat->fxBurst(enemy.aimX, enemy.aimY, radius, theme);
      combat->fxRing(enemy.aimX, enemy.aimY, radius + 20.0f, theme);
      combat->playersInCircle(enemy.aimX, enemy.aimY, radius,
                              [&](Player& victim) {
                                float dealt = combat->damagePlayer(
                                    victim, damage, selfId, DamageFlag::Melee);
                                if (dealt > 0.0f) afflict(victim, true);
                              });
      if (genus == Genus::Magnarok || genus == Genus::Venoxarid) {
        AbilitySystem* sys = combat;
        AbilitySystem::Zone pool;
        pool.fromEnemy = true;
        pool.ownerId = selfId;
        pool.x = enemy.aimX;
        pool.y = enemy.aimY;
        pool.radius = 80.0f;
        pool.lifeMs = 3500.0f;
        pool.tickMs = 600.0f;
        pool.theme = theme;
        pool.variant = 1;
        pool.onPlayerTick = [sys, genus, selfId](AbilitySystem::Zone&,
                                                 Player& victim) {
          if (genus == Genus::Venoxarid) {
            sys->applyToPlayer(victim, EffectType::Wound, 1, selfId);
          } else {
            sys->damagePlayer(victim, 8.0f, selfId, DamageFlag::Dot);
          }
        };
        combat->spawnZone(pool);
      }
      break;
    }
    default:
      break;
  }
}

void EnemySystem::resolveBossAttack(
    Enemy& enemy, std::unordered_map<uint32_t, Player>& players) {
  (void)players;
  uint32_t selfId = enemy.id;
  float damage = enemy.damage;
  AbilitySystem* sys = combat;

  float dx = enemy.aimX - enemy.x;
  float dy = enemy.aimY - enemy.y;
  float d = std::sqrt(dx * dx + dy * dy);
  float dirX = d > 0.001f ? dx / d : 1.0f;
  float dirY = d > 0.001f ? dy / d : 0.0f;
  bool enraged = enemy.health < enemy.maxHealth * 0.35f;

  if (enemy.bossMove == 0) {
    // Slam
    const float radius = 190.0f;
    combat->fxBurst(enemy.aimX, enemy.aimY, radius, VfxTheme::Shadow);
    combat->fxRing(enemy.aimX, enemy.aimY, radius + 40.0f, VfxTheme::Blood);
    combat->playersInCircle(enemy.aimX, enemy.aimY, radius,
                            [&](Player& victim) {
                              sys->damagePlayer(victim, damage, selfId,
                                                DamageFlag::Melee);
                              sys->applyToPlayer(victim, EffectType::Vulnerable,
                                                 1, selfId);
                            });
  } else if (enemy.bossMove == 1) {
    // Nova: one projectile per genus colour, twice as many when enraged.
    int count = enraged ? 20 : 12;
    float offset = sys->randomRange(0.0f, 2.0f * PI);
    static const VfxTheme THEMES[4] = {VfxTheme::Poison, VfxTheme::Psychic,
                                       VfxTheme::Ice, VfxTheme::Fire};
    for (int i = 0; i < count; i++) {
      float angle = offset + (2.0f * PI * i) / count;
      AbilitySystem::Projectile shot;
      shot.fromEnemy = true;
      shot.ownerId = selfId;
      shot.x = enemy.x + std::cos(angle) * 50.0f;
      shot.y = enemy.y + std::sin(angle) * 50.0f;
      shot.vx = std::cos(angle) * 300.0f;
      shot.vy = std::sin(angle) * 300.0f;
      shot.radius = 13.0f;
      shot.lifeMs = 2600.0f;
      shot.theme = THEMES[i % 4];
      int flavour = i % 4;
      shot.onHitPlayer = [sys, damage, selfId, flavour](
                             AbilitySystem::Projectile&, Player& victim) {
        if (sys->damagePlayer(victim, damage * 0.45f, selfId) <= 0.0f) return;
        static const EffectType AFFLICTIONS[4] = {
            EffectType::Wound, EffectType::Weakened, EffectType::Slow,
            EffectType::Vulnerable};
        sys->applyToPlayer(victim, AFFLICTIONS[flavour], 1, selfId);
      };
      combat->spawnProjectile(shot);
    }
    combat->fxRing(enemy.x, enemy.y, 140.0f, VfxTheme::Shadow);
  } else if (enemy.bossMove == 2) {
    // Sweep
    const float range = 330.0f;
    combat->fxCone(enemy.x, enemy.y, dirX, dirY, range, 100.0f,
                   VfxTheme::Blood);
    combat->playersInCone(enemy.x, enemy.y, dirX, dirY, range, 50.0f,
                          [&](Player& victim) {
                            sys->damagePlayer(victim, damage * 0.8f, selfId,
                                              DamageFlag::Melee);
                            sys->applyToPlayer(victim, EffectType::Slow, 2,
                                               selfId);
                          });
  } else {
    // Summon a brood of swarmers from a random genus.
    std::uniform_int_distribution<int> genusPick(1, Arena::GENUS_COUNT);
    Genus genus = static_cast<Genus>(genusPick(rng));
    int count = enraged ? 7 : 5;
    combat->fxRing(enemy.x, enemy.y, 240.0f, VfxTheme::Shadow);
    for (int i = 0; i < count; i++) {
      float angle = (2.0f * PI * i) / count;
      float sx = enemy.x + std::cos(angle) * 170.0f;
      float sy = enemy.y + std::sin(angle) * 170.0f;
      combat->clampToWorld(sx, sy);
      EnemyKind kind = Arena::enemyKindFor(genus, EnemyTier::Swarmer);
      spawn(kind, sx, sy);
      GameEventPacket puff;
      puff.kind = GameEventKind::SpawnPuff;
      puff.x = sx;
      puff.y = sy;
      puff.param2 = static_cast<uint8_t>(themeFor(genus));
      combat->emit(puff);
    }
  }
}

void EnemySystem::damageEnemy(uint32_t enemyId, float damage,
                              uint32_t attackerId) {
  auto it = enemies.find(enemyId);
  if (it == enemies.end()) return;
  Enemy& enemy = it->second;
  if (enemy.state == EnemyState::Dead) return;

  enemy.health -= damage;
  if (attackerId != 0) {
    enemy.lastHitBy = attackerId;
    enemy.passive = false;
    enemy.campGuard = false;  // provoked: leave the camp and give chase
  }
  if (enemy.health <= 0.0f) kill(enemy, attackerId);
}

void EnemySystem::kill(Enemy& enemy, uint32_t killerId) {
  enemy.health = 0.0f;
  enemy.state = EnemyState::Dead;
  enemy.vx = 0.0f;
  enemy.vy = 0.0f;
  enemy.deadMs = 0.0f;
  enemy.deathTime = accumulatedTime;
  if (killerId == 0) killerId = enemy.lastHitBy;
  pendingDeaths.push_back({enemy.id, killerId});
}

void EnemySystem::pushEnemy(Enemy& enemy, float dirX, float dirY,
                            float distance, float durationMs) {
  if (enemy.state == EnemyState::Dead) return;
  const Arena::EnemyKindDef& def = Arena::enemyDef(enemy.type);
  // Big things move less; the boss barely budges.
  if (def.tier == EnemyTier::Brute) distance *= 0.45f;
  if (def.tier == EnemyTier::Boss) distance *= 0.08f;
  if (durationMs < 16.0f) durationMs = 16.0f;
  float speed = distance / (durationMs / 1000.0f);
  enemy.knockVx = dirX * speed;
  enemy.knockVy = dirY * speed;
  enemy.knockMs = durationMs;
}

void EnemySystem::disableSpawnsInRadius(float x, float y, float radius) {
  disabledZones.push_back({x, y, radius});
  // Clear out any camp guards still idling inside the zone.
  for (auto& [id, enemy] : enemies) {
    if (enemy.state == EnemyState::Dead) continue;
    if (!enemy.campGuard) continue;
    if (distance(enemy.x, enemy.y, x, y) <= radius) {
      kill(enemy, 0);
    }
  }
}

float EnemySystem::distance(float x1, float y1, float x2, float y2) const {
  float dx = x2 - x1;
  float dy = y2 - y1;
  return std::sqrt(dx * dx + dy * dy);
}
