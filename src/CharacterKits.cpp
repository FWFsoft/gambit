// Character kits. Each class implements one character from the "Character
// Concepts" design doc on top of AbilitySystem's helpers. Text for the HUD
// lives in ArenaData.cpp; numbers here are prototype tuning.
//
// Slots: 0 = LMB, 1 = RMB (hold), 2 = Q, 3 = LSHIFT.
// Per-player state lives in Player::kitTimer / kitCounter / kitTarget; each
// kit documents how it uses them.

#include <algorithm>
#include <cmath>
#include <memory>

#include "AbilitySystem.h"
#include "EffectManager.h"
#include "EnemySystem.h"

namespace {

using Arena::CharacterId::Gravon;
using Arena::CharacterId::Isaac;
using Arena::CharacterId::Mina;
using Arena::CharacterId::Suds;
using Arena::CharacterId::Volgore;
using Arena::CharacterId::Wade;
using Arena::CharacterId::Zephyr;

float dist(float x0, float y0, float x1, float y1) {
  float dx = x1 - x0;
  float dy = y1 - y0;
  return std::sqrt(dx * dx + dy * dy);
}

// Point `range` away from the player toward the cursor, or the cursor itself
// if it is closer than that.
void aimPoint(const AbilityContext& ctx, float range, float& outX,
              float& outY) {
  float d = dist(ctx.self.x, ctx.self.y, ctx.aimX, ctx.aimY);
  float reach = std::min(d, range);
  outX = ctx.self.x + ctx.dirX * reach;
  outY = ctx.self.y + ctx.dirY * reach;
  ctx.sys.clampToWorld(outX, outY);
}

void doubleStacks(AbilitySystem& sys, uint32_t entityId, bool isEnemy,
                  EffectType type) {
  ActiveEffects& fx = sys.effects().mutableEffects(entityId, isEnemy);
  if (EffectInstance* effect = fx.findEffect(type)) {
    int maxStacks = EffectRegistry::get(type).maxStacks;
    effect->stacks =
        static_cast<uint8_t>(std::min(effect->stacks * 2, maxStacks));
    effect->remainingDuration =
        std::max(effect->remainingDuration, EffectRegistry::get(type).baseDuration);
  }
}

// ===========================================================================
// Suds Sullivan, the Brew Brawler (Defender)
// ===========================================================================
class SudsKit : public Kit {
 public:
  static void brewSplash(AbilitySystem& sys, uint32_t sudsId, float x, float y,
                         uint32_t flags) {
    const float radius = 70.0f;
    sys.fxBurst(x, y, radius, VfxTheme::Brew);
    sys.enemiesInCircle(x, y, radius, [&](Enemy& enemy) {
      uint32_t enemyId = enemy.id;
      sys.damageEnemy(sudsId, enemy, 10.0f, flags);
      // Damage first, then the suds: the Guard is for the *next* hit.
      if (Enemy* still = sys.findEnemy(enemyId)) {
        sys.applyToEnemy(*still, EffectType::Guard, 3, sudsId);
      }
    });
    sys.playersInCircle(x, y, radius, [&](Player& ally) {
      sys.applyToPlayer(ally, EffectType::Guard, 3, sudsId);
    });
  }

  static void tossBrew(AbilitySystem& sys, Player& self, float x, float y,
                       uint32_t flags) {
    const float flightMs = 300.0f;
    sys.fxLob(self.x, self.y, x, y, 70.0f, flightMs, VfxTheme::Brew, 2);
    AbilitySystem* s = &sys;
    uint32_t sudsId = self.id;
    sys.schedule(flightMs,
                 [s, sudsId, x, y, flags]() { brewSplash(*s, sudsId, x, y, flags); });
  }

  void cast(AbilityContext& ctx, int slot) override {
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    uint32_t selfId = self.id;

    if (slot == 0) {
      // Brew Toss
      float x, y;
      aimPoint(ctx, 420.0f, x, y);
      tossBrew(sys, self, x, y, DamageFlag::Basic);
    } else if (slot == 1) {
      // Frothy Cleanse
      float range = 170.0f + 130.0f * ctx.charge;
      float half = 35.0f + 10.0f * ctx.charge;
      float stunMs = 1200.0f + 800.0f * ctx.charge;
      sys.fxCone(self.x, self.y, ctx.dirX, ctx.dirY, range, half * 2.0f,
                 VfxTheme::Brew);
      sys.enemiesInCone(self.x, self.y, ctx.dirX, ctx.dirY, range, half,
                        [&](Enemy& enemy) {
                          uint32_t enemyId = enemy.id;
                          sys.effects().removeEffect(enemyId, true,
                                                     EffectType::Guard);
                          sys.effects().removeEffect(enemyId, true,
                                                     EffectType::Wound);
                          sys.applyToEnemy(enemy, EffectType::Stunned, 1,
                                           selfId, stunMs);
                          sys.damageEnemy(selfId, enemy, 8.0f);
                        });
      auto refresh = [&](Player& ally) {
        sys.effects().removeEffect(ally.id, false, EffectType::Wound);
        sys.effects().removeEffect(ally.id, false, EffectType::Guard);
        sys.applyToPlayer(ally, EffectType::Berserk, 1, selfId);
      };
      sys.playersInCone(self.x, self.y, ctx.dirX, ctx.dirY, range, half,
                        refresh);
    } else if (slot == 2) {
      // Round's On Me!
      float x, y;
      aimPoint(ctx, 420.0f, x, y);
      AbilitySystem* s = &sys;
      for (int i = 0; i < 6; i++) {
        float ox = sys.randomRange(-85.0f, 85.0f);
        float oy = sys.randomRange(-85.0f, 85.0f);
        if (i == 0) ox = oy = 0.0f;
        sys.schedule(i * 130.0f, [s, selfId, x, y, ox, oy]() {
          Player* suds = s->findPlayer(selfId);
          if (!suds || suds->isDead()) return;
          tossBrew(*s, *suds, x + ox, y + oy, DamageFlag::None);
        });
      }
    } else if (slot == 3) {
      // What's In This Stuff?
      const float radius = 260.0f;
      sys.fxRing(self.x, self.y, radius, VfxTheme::Brew);
      sys.fxBurst(self.x, self.y, radius * 0.5f, VfxTheme::Brew);
      sys.enemiesInCircle(self.x, self.y, radius, [&](Enemy& enemy) {
        doubleStacks(sys, enemy.id, true, EffectType::Guard);
        doubleStacks(sys, enemy.id, true, EffectType::Wound);
        sys.applyToEnemy(enemy, EffectType::Weakened, 2, selfId);
      });
      sys.playersInCircle(self.x, self.y, radius, [&](Player& ally) {
        doubleStacks(sys, ally.id, false, EffectType::Guard);
        doubleStacks(sys, ally.id, false, EffectType::Wound);
        sys.applyToPlayer(ally, EffectType::Fortified, 2, selfId);
      });
    }
  }

  // Frothy: lost Guard comes back as Wound.
  void onGuardLostNearby(AbilitySystem& sys, Player& self, bool targetIsEnemy,
                         uint32_t targetId, float prevented) override {
    int stacks = std::max(1, std::min(4, static_cast<int>(std::ceil(prevented / 6.0f))));
    if (targetIsEnemy) {
      if (Enemy* enemy = sys.findEnemy(targetId)) {
        sys.applyToEnemy(*enemy, EffectType::Wound, stacks, self.id);
      }
    } else if (Player* ally = sys.findPlayer(targetId)) {
      sys.applyToPlayer(*ally, EffectType::Wound, 1, self.id);
    }
  }
};

// ===========================================================================
// Gravon, the Undying Blade (Striker)
//   kitCounter[0]  1 once Soulblade has been spent this round
//   kitTimer[0]    time left in blade form
// ===========================================================================
class GravonKit : public Kit {
 public:
  void cast(AbilityContext& ctx, int slot) override {
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    uint32_t selfId = self.id;

    if (slot == 0) {
      // Titan's Slash
      const float range = 125.0f;
      sys.fxSlash(self.x, self.y, ctx.dirX, ctx.dirY, range, 140.0f,
                  VfxTheme::Shadow);
      sys.enemiesInCone(self.x, self.y, ctx.dirX, ctx.dirY, range, 70.0f,
                        [&](Enemy& enemy) {
                          sys.damageEnemy(selfId, enemy, 26.0f,
                                          DamageFlag::Basic | DamageFlag::Melee);
                        });
    } else if (slot == 1) {
      // Ravager's Edge
      const float range = 165.0f;
      bool full = ctx.charge >= 0.98f;
      float damage = 30.0f + 85.0f * ctx.charge;
      uint32_t flags = DamageFlag::Melee;
      if (full) {
        sys.applyToPlayer(self, EffectType::Sharpened, 1, selfId);
        flags |= DamageFlag::ForceCrit;
      }
      sys.fxSlash(self.x, self.y, ctx.dirX, ctx.dirY, range, 50.0f,
                  full ? VfxTheme::Blood : VfxTheme::Shadow);
      sys.fxBeam(self.x, self.y, self.x + ctx.dirX * range,
                 self.y + ctx.dirY * range, full ? 22.0f : 12.0f, 180.0f,
                 VfxTheme::Shadow);
      sys.enemiesInCone(self.x, self.y, ctx.dirX, ctx.dirY, range, 24.0f,
                        [&](Enemy& enemy) {
                          sys.damageEnemy(selfId, enemy, damage, flags);
                        });
    } else if (slot == 2) {
      // Fury of the Blade
      sys.applyToPlayer(self, EffectType::Empowered, 1, selfId, 6000.0f);
      sys.applyToPlayer(self, EffectType::Haste, 1, selfId, 6000.0f);
      sys.fxRing(self.x, self.y, 80.0f, VfxTheme::Blood);
    } else if (slot == 3) {
      // Blade Rush
      const float distance = 270.0f;
      float endX = self.x + ctx.dirX * distance;
      float endY = self.y + ctx.dirY * distance;
      sys.clampToWorld(endX, endY);
      sys.fxDashTrail(self.x, self.y, endX, endY, VfxTheme::Shadow);
      sys.enemiesInLine(self.x, self.y, endX, endY, 36.0f, [&](Enemy& enemy) {
        sys.damageEnemy(selfId, enemy, 42.0f, DamageFlag::Melee);
      });
      sys.dashPlayer(self, ctx.dirX, ctx.dirY, distance, 170.0f, true);
    }
  }

  void onDealDamage(AbilitySystem& sys, Player& self, Enemy&, float,
                    uint32_t) override {
    const ActiveEffects& fx = sys.effects().getPlayerEffects(self.id);
    if (fx.hasEffect(EffectType::Empowered) || fx.hasEffect(EffectType::Haste)) {
      self.cooldownMs[3] = std::max(0.0f, self.cooldownMs[3] - 700.0f);
    }
  }

  // Soulblade: the host falls, the blade waits.
  bool onLethal(AbilitySystem& sys, Player& self) override {
    if (self.kitCounter[0] != 0) return false;
    self.kitCounter[0] = 1;
    self.kitTimer[0] = 5000.0f;
    self.health = 1.0f;
    self.chargeSlot = -1;
    self.intangibleMs = std::max(self.intangibleMs, 200.0f);
    self.hiddenMs = std::max(self.hiddenMs, 200.0f);
    sys.effects().clearEntity(self.id, false);
    sys.fxBurst(self.x, self.y, 90.0f, VfxTheme::Shadow);
    return true;
  }

  void tick(AbilitySystem& sys, Player& self, float dtMs) override {
    self.resource = self.kitCounter[0] == 0 ? 255 : 0;
    if (self.kitTimer[0] <= 0.0f) return;
    self.kitTimer[0] -= dtMs;
    if (self.kitTimer[0] > 0.0f) {
      // Blade form: cannot move, act or be hit.
      self.forcedVx = self.forcedVy = 0.0f;
      self.forcedMs = std::max(self.forcedMs, 100.0f);
      self.intangibleMs = std::max(self.intangibleMs, 100.0f);
      self.hiddenMs = std::max(self.hiddenMs, 100.0f);
      self.resource = static_cast<uint8_t>(
          std::min(255.0f, 255.0f * (1.0f - self.kitTimer[0] / 5000.0f)));
    } else {
      self.kitTimer[0] = 0.0f;
      self.health = self.maxHealth * 0.5f;
      sys.fxRing(self.x, self.y, 120.0f, VfxTheme::Blood);
      sys.fxBurst(self.x, self.y, 70.0f, VfxTheme::Shadow);
    }
  }

  void onEnemyDiedNearby(AbilitySystem&, Player& self, const Enemy&) override {
    if (self.kitTimer[0] > 1000.0f) self.kitTimer[0] -= 1000.0f;
  }

  void onRoundStart(Player& self) override { self.kitCounter[0] = 0; }
};

// ===========================================================================
// Isaac, the Beacon (Winger)
//   kitCounter[0]  player id carrying the Blessing (0 = nobody)
//   kitTarget      Lifeline ally id (0 = anchored to terrain)
//   kitTimer[1,2]  Lifeline anchor point
//   kitTimer[3]    Lifeline heal/tether pulse accumulator
// ===========================================================================
class IsaacKit : public Kit {
 public:
  static constexpr float LIFELINE_RANGE = 520.0f;

  void cast(AbilityContext& ctx, int slot) override {
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    uint32_t selfId = self.id;

    if (slot == 0) {
      // Ray of Benevolence
      const float range = 235.0f;
      sys.fxCone(self.x, self.y, ctx.dirX, ctx.dirY, range, 70.0f,
                 VfxTheme::Holy);
      int hits = 0;
      sys.enemiesInCone(self.x, self.y, ctx.dirX, ctx.dirY, range, 35.0f,
                        [&](Enemy& enemy) {
                          hits++;
                          sys.applyToEnemy(enemy, EffectType::Slow, 1, selfId);
                          sys.damageEnemy(selfId, enemy, 9.0f, DamageFlag::Basic);
                        });
      if (hits > 0) {
        float heal = 4.0f * std::min(hits, 5);
        sys.playersInCircle(self.x, self.y, 300.0f, [&](Player& ally) {
          sys.healPlayer(selfId, ally, heal);
        });
      }
    } else if (slot == 1) {
      // Lifeline release: get pulled to the anchor.
      float tx = self.kitTimer[1];
      float ty = self.kitTimer[2];
      if (Player* ally = sys.findPlayer(self.kitTarget)) {
        if (ally->isAlive()) {
          tx = ally->x;
          ty = ally->y;
        }
      }
      float d = dist(self.x, self.y, tx, ty);
      float travel = std::min(LIFELINE_RANGE, std::max(0.0f, d - 34.0f));
      if (travel > 10.0f) {
        float dx = (tx - self.x) / d;
        float dy = (ty - self.y) / d;
        sys.fxDashTrail(self.x, self.y, self.x + dx * travel,
                        self.y + dy * travel, VfxTheme::Holy);
        sys.dashPlayer(self, dx, dy, travel, 200.0f, true);
      }
      self.kitTarget = 0;
    } else if (slot == 2) {
      // The Blessing of Isaac
      Player* target = sys.nearestAlly(self, ctx.aimX, ctx.aimY, 130.0f, true);
      if (!target) target = &self;
      self.kitCounter[0] = static_cast<int>(target->id);
      sys.fxRing(target->x, target->y, 70.0f, VfxTheme::Holy);
      sys.fxTether(self.x, self.y, target->x, target->y, 350.0f,
                   VfxTheme::Holy);
      sys.healPlayer(selfId, *target, 12.0f, false);
    } else if (slot == 3) {
      // Flash of Sanctity
      float x, y;
      aimPoint(ctx, 330.0f, x, y);
      float fromX = self.x, fromY = self.y;
      sys.blinkPlayer(self, x, y);
      sys.fxDashTrail(fromX, fromY, self.x, self.y, VfxTheme::Holy);
      sys.fxBurst(self.x, self.y, 150.0f, VfxTheme::Holy);
      sys.playersInCircle(self.x, self.y, 150.0f, [&](Player& ally) {
        sys.healPlayer(selfId, ally, 30.0f);
      });
    }
  }

  void chargeTick(AbilityContext& ctx, int slot, float dtMs) override {
    if (slot != 1) return;
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;

    if (self.chargeMs <= dtMs + 0.01f) {
      // First tick: pick the anchor.
      float x, y;
      aimPoint(ctx, LIFELINE_RANGE, x, y);
      Player* ally = sys.nearestAlly(self, ctx.aimX, ctx.aimY, 120.0f, false);
      if (ally && dist(self.x, self.y, ally->x, ally->y) <= LIFELINE_RANGE) {
        self.kitTarget = ally->id;
      } else {
        self.kitTarget = 0;
      }
      self.kitTimer[1] = x;
      self.kitTimer[2] = y;
      self.kitTimer[3] = 400.0f;  // pulse immediately
    }

    self.kitTimer[3] += dtMs;
    if (self.kitTimer[3] < 400.0f) return;
    self.kitTimer[3] -= 400.0f;

    Player* ally = sys.findPlayer(self.kitTarget);
    if (ally && ally->isDead()) ally = nullptr;
    float tx = ally ? ally->x : self.kitTimer[1];
    float ty = ally ? ally->y : self.kitTimer[2];
    sys.fxTether(self.x, self.y, tx, ty, 420.0f, VfxTheme::Holy);
    if (ally) {
      sys.healPlayer(self.id, self, 4.0f);
      sys.healPlayer(self.id, *ally, 5.0f);
    } else {
      // Terrain gives Isaac himself more.
      sys.healPlayer(self.id, self, 7.0f);
    }
  }

  void onHealDone(AbilitySystem& sys, Player& self, Player& target,
                  float amount) override {
    uint32_t blessed = static_cast<uint32_t>(self.kitCounter[0]);
    if (blessed == 0 || blessed == target.id) return;
    if (Player* ally = sys.findPlayer(blessed)) {
      sys.healPlayer(self.id, *ally, amount * 0.5f, false);
    }
  }

  float speedMultiplier(const Player& self) const override {
    return static_cast<uint32_t>(self.kitCounter[0]) == self.id ? 1.2f : 1.0f;
  }

  void tick(AbilitySystem&, Player& self, float) override {
    self.resource = self.kitCounter[0] != 0 ? 255 : 0;
  }
};

// ===========================================================================
// Zephyr, the Galewalker (Playmaker)
//   kitTarget     player wrapped in Gale's Embrace
//   kitTimer[0]   time left on Gale's Embrace
// ===========================================================================
class ZephyrKit : public Kit {
 public:
  // Wind's Favor: +1 Haste the first time an ability connects.
  static void windsFavor(AbilitySystem& sys, uint32_t zephyrId,
                         const std::shared_ptr<bool>& granted) {
    if (*granted) return;
    *granted = true;
    if (Player* zephyr = sys.findPlayer(zephyrId)) {
      sys.applyToPlayer(*zephyr, EffectType::Haste, 1, zephyrId);
    }
  }

  static void galeBurst(AbilitySystem& sys, Player& self) {
    Player* target = sys.findPlayer(self.kitTarget);
    self.kitTimer[0] = 0.0f;
    self.kitTarget = 0;
    if (!target || target->isDead()) return;
    const float radius = 160.0f;
    uint32_t selfId = self.id;
    float cx = target->x, cy = target->y;
    auto granted = std::make_shared<bool>(false);
    sys.fxRing(cx, cy, radius, VfxTheme::Wind);
    sys.fxBurst(cx, cy, radius * 0.6f, VfxTheme::Wind);
    sys.enemiesInCircle(cx, cy, radius, [&](Enemy& enemy) {
      sys.knockbackEnemy(enemy, cx, cy, 210.0f);
      sys.damageEnemy(selfId, enemy, 15.0f);
      windsFavor(sys, selfId, granted);
    });
  }

  void cast(AbilityContext& ctx, int slot) override {
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    uint32_t selfId = self.id;
    AbilitySystem* s = &sys;
    auto granted = std::make_shared<bool>(false);

    if (slot == 0) {
      // Wind Slash
      AbilitySystem::Projectile gust;
      gust.ownerId = selfId;
      gust.x = self.x + ctx.dirX * 18.0f;
      gust.y = self.y + ctx.dirY * 18.0f;
      gust.vx = ctx.dirX * 640.0f;
      gust.vy = ctx.dirY * 640.0f;
      gust.radius = 24.0f;
      gust.lifeMs = 580.0f;
      gust.pierce = true;
      gust.theme = VfxTheme::Wind;
      gust.sprite = 1;
      gust.onHitEnemy = [s, selfId, granted](AbilitySystem::Projectile& p,
                                             Enemy& enemy) {
        s->knockbackEnemy(enemy, p.x - p.vx * 0.1f, p.y - p.vy * 0.1f, 55.0f);
        s->damageEnemy(selfId, enemy, 14.0f, DamageFlag::Basic);
        windsFavor(*s, selfId, granted);
      };
      sys.spawnProjectile(gust);
    } else if (slot == 1) {
      // Vortex
      float x, y;
      aimPoint(ctx, 460.0f, x, y);
      float radius = 140.0f + 100.0f * ctx.charge;
      float stunMs = 800.0f + 800.0f * ctx.charge;
      AbilitySystem::Zone swirl;
      swirl.ownerId = selfId;
      swirl.x = x;
      swirl.y = y;
      swirl.radius = radius;
      swirl.lifeMs = 700.0f;
      swirl.tickMs = 10000.0f;  // visual only
      swirl.theme = VfxTheme::Wind;
      swirl.variant = 2;
      sys.spawnZone(swirl);
      sys.enemiesInCircle(x, y, radius, [&](Enemy& enemy) {
        sys.pullEnemy(enemy, x, y, radius, 14.0f);
        sys.damageEnemy(selfId, enemy, 12.0f);
        windsFavor(sys, selfId, granted);
      });
      sys.schedule(300.0f, [s, selfId, x, y, radius, stunMs]() {
        s->fxBurst(x, y, radius * 0.55f, VfxTheme::Wind);
        s->enemiesInCircle(x, y, radius * 0.6f, [&](Enemy& enemy) {
          s->applyToEnemy(enemy, EffectType::Stunned, 1, selfId, stunMs);
        });
      });
    } else if (slot == 2) {
      // Gale's Embrace (press again to release the gust early)
      if (self.kitTimer[0] > 0.0f) {
        if (Player* target = sys.findPlayer(self.kitTarget)) {
          target->shield = 0.0f;
          target->shieldMs = 0.0f;
        }
        galeBurst(sys, self);
        return;
      }
      Player* target = sys.nearestAlly(self, ctx.aimX, ctx.aimY, 130.0f, true);
      if (!target) target = &self;
      self.kitTarget = target->id;
      self.kitTimer[0] = 4000.0f;
      sys.grantShield(*target, 70.0f, 4000.0f);
      sys.fxRing(target->x, target->y, 60.0f, VfxTheme::Wind);
    } else if (slot == 3) {
      // Tempest Dash
      const float distance = 300.0f;
      float endX = self.x + ctx.dirX * distance;
      float endY = self.y + ctx.dirY * distance;
      sys.clampToWorld(endX, endY);
      sys.fxDashTrail(self.x, self.y, endX, endY, VfxTheme::Wind);
      sys.enemiesInLine(self.x, self.y, endX, endY, 30.0f, [&](Enemy& enemy) {
        sys.damageEnemy(selfId, enemy, 10.0f);
        windsFavor(sys, selfId, granted);
      });
      sys.dashPlayer(self, ctx.dirX, ctx.dirY, distance, 180.0f, true);
    }
  }

  bool canRecast(const Player& self, int slot) const override {
    return slot == 2 && self.kitTimer[0] > 0.0f;
  }

  void tick(AbilitySystem& sys, Player& self, float dtMs) override {
    if (self.kitTimer[0] <= 0.0f) return;
    self.kitTimer[0] -= dtMs;
    if (self.kitTimer[0] <= 0.0f) galeBurst(sys, self);
  }
};

// ===========================================================================
// Mina, the Professional (Striker, ranged)
// ===========================================================================
class MinaKit : public Kit {
 public:
  static void explode(AbilitySystem& sys, uint32_t minaId, float x, float y,
                      float radius, float damage, uint32_t flags) {
    sys.fxBurst(x, y, radius, VfxTheme::Fire);
    sys.fxRing(x, y, radius + 14.0f, VfxTheme::Fire);
    sys.enemiesInCircle(x, y, radius, [&](Enemy& enemy) {
      sys.damageEnemy(minaId, enemy, damage, flags | DamageFlag::Explosion);
    });
    // Not the Goalie: explosions near Mina toughen her up.
    if (Player* mina = sys.findPlayer(minaId)) {
      if (mina->isAlive() && dist(mina->x, mina->y, x, y) <= radius + 110.0f) {
        sys.applyToPlayer(*mina, EffectType::Fortified, 1, minaId);
      }
    }
  }

  void cast(AbilityContext& ctx, int slot) override {
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    uint32_t selfId = self.id;
    AbilitySystem* s = &sys;

    if (slot == 0) {
      // Pass the Ball
      AbilitySystem::Projectile ball;
      ball.ownerId = selfId;
      ball.x = self.x + ctx.dirX * 16.0f;
      ball.y = self.y + ctx.dirY * 16.0f;
      ball.vx = ctx.dirX * 560.0f;
      ball.vy = ctx.dirY * 560.0f;
      ball.radius = 13.0f;
      ball.lifeMs = 800.0f;
      ball.theme = VfxTheme::Fire;
      ball.sprite = 3;
      ball.onEnd = [s, selfId](AbilitySystem::Projectile& p) {
        explode(*s, selfId, p.x, p.y, 70.0f, 22.0f, DamageFlag::Basic);
      };
      sys.spawnProjectile(ball);
    } else if (slot == 1) {
      // The Long Shot
      float x, y;
      aimPoint(ctx, 220.0f + 360.0f * ctx.charge, x, y);
      float radius = 80.0f + 60.0f * ctx.charge;
      float damage = 30.0f + 55.0f * ctx.charge;
      const float flightMs = 520.0f;
      sys.fxLob(self.x, self.y, x, y, radius, flightMs, VfxTheme::Fire, 3);
      sys.fxTelegraphCircle(x, y, radius, flightMs, VfxTheme::Fire);
      sys.schedule(flightMs, [s, selfId, x, y, radius, damage]() {
        explode(*s, selfId, x, y, radius, damage, DamageFlag::None);
      });
    } else if (slot == 2) {
      // GOAL!!!!
      float x, y;
      aimPoint(ctx, 500.0f, x, y);
      sys.fxTelegraphCircle(x, y, 170.0f, 1500.0f, VfxTheme::Fire);
      for (int wave = 0; wave < 4; wave++) {
        sys.schedule(250.0f + wave * 350.0f, [s, selfId, x, y]() {
          for (int i = 0; i < 3; i++) {
            float ox = s->randomRange(-105.0f, 105.0f);
            float oy = s->randomRange(-105.0f, 105.0f);
            explode(*s, selfId, x + ox, y + oy, 65.0f, 16.0f, DamageFlag::None);
          }
          s->enemiesInCircle(x, y, 170.0f, [&](Enemy& enemy) {
            s->applyToEnemy(enemy, EffectType::Vulnerable, 1, selfId);
          });
        });
      }
    } else if (slot == 3) {
      // Handball!
      sys.applyToPlayer(self, EffectType::Haste, 2, selfId, 4000.0f);
      sys.fxRing(self.x, self.y, 60.0f, VfxTheme::Fire);
    }
  }
};

// ===========================================================================
// Volgore, the Berserker (Defender)
//   kitTimer[0]   Rage Unending window (can be ended early while > 0)
//   kitTimer[1]   Fortified refresh cadence
// ===========================================================================
class VolgoreKit : public Kit {
 public:
  void cast(AbilityContext& ctx, int slot) override {
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    uint32_t selfId = self.id;

    if (slot == 0) {
      // Hamstring
      const float range = 108.0f;
      sys.fxSlash(self.x, self.y, ctx.dirX, ctx.dirY, range, 100.0f,
                  VfxTheme::Blood);
      sys.enemiesInCone(self.x, self.y, ctx.dirX, ctx.dirY, range, 50.0f,
                        [&](Enemy& enemy) {
                          sys.applyToEnemy(enemy, EffectType::Weakened, 1,
                                           selfId);
                          sys.damageEnemy(selfId, enemy, 16.0f,
                                          DamageFlag::Basic | DamageFlag::Melee);
                        });
    } else if (slot == 1) {
      // Hooked
      float range = 220.0f + 160.0f * ctx.charge;
      float half = 28.0f + 17.0f * ctx.charge;
      sys.fxCone(self.x, self.y, ctx.dirX, ctx.dirY, range, half * 2.0f,
                 VfxTheme::Blood);
      float sx = self.x, sy = self.y;
      sys.enemiesInCone(self.x, self.y, ctx.dirX, ctx.dirY, range, half,
                        [&](Enemy& enemy) {
                          sys.pullEnemy(enemy, sx, sy, range, 46.0f);
                          sys.damageEnemy(selfId, enemy, 10.0f);
                        });
    } else if (slot == 2) {
      // Ferocious Bellow
      const float radius = 300.0f;
      sys.fxRing(self.x, self.y, radius, VfxTheme::Blood);
      sys.fxBurst(self.x, self.y, radius * 0.4f, VfxTheme::Blood);
      sys.enemiesInCircle(self.x, self.y, radius, [&](Enemy& enemy) {
        if (sys.effects().getEnemyEffects(enemy.id).hasEffect(
                EffectType::Snared)) {
          sys.applyToEnemy(enemy, EffectType::Doomed, 1, selfId);
        }
        sys.applyToEnemy(enemy, EffectType::Snared, 1, selfId, 2500.0f);
      });
    } else if (slot == 3) {
      // Rage Unending
      if (self.kitTimer[0] > 0.0f) {
        // End it early: Wounds become Mend.
        ActiveEffects& fx = sys.effects().mutableEffects(selfId, false);
        int wounds = fx.getStacks(EffectType::Wound);
        fx.removeEffect(EffectType::Berserk);
        fx.removeEffect(EffectType::Wound);
        fx.removeEffect(EffectType::Fortified);
        if (wounds > 0) {
          sys.applyToPlayer(self, EffectType::Mend, wounds, selfId);
        }
        self.kitTimer[0] = 0.0f;
        sys.fxRing(self.x, self.y, 80.0f, VfxTheme::Heal);
        return;
      }
      sys.applyToPlayer(self, EffectType::Berserk, 1, selfId);
      self.kitTimer[0] = EffectRegistry::get(EffectType::Berserk).baseDuration;
      sys.fxRing(self.x, self.y, 90.0f, VfxTheme::Blood);
    }
  }

  bool canRecast(const Player& self, int slot) const override {
    return slot == 3 && self.kitTimer[0] > 0.0f;
  }

  void tick(AbilitySystem& sys, Player& self, float dtMs) override {
    bool berserk =
        sys.effects().getPlayerEffects(self.id).hasEffect(EffectType::Berserk);
    self.resource = berserk ? 255 : 0;
    if (!berserk) {
      self.kitTimer[0] = 0.0f;
      return;
    }
    // Berserk can also come from Suds; either way it makes Volgore a wall.
    self.kitTimer[0] = std::max(self.kitTimer[0], 100.0f);
    self.kitTimer[1] -= dtMs;
    if (self.kitTimer[1] <= 0.0f) {
      self.kitTimer[1] = 250.0f;
      sys.applyToPlayer(self, EffectType::Fortified, 5, self.id, 450.0f);
    }
  }

  float onTakeDamage(AbilitySystem& sys, Player& self, float damage,
                     uint32_t) override {
    if (sys.effects().getPlayerEffects(self.id).hasEffect(EffectType::Berserk)) {
      sys.applyToPlayer(self, EffectType::Wound, 1, self.id);
    }
    return damage;
  }
};

// ===========================================================================
// Wade, the Old Head (Striker)
//   kitCounter[0]  punches thrown toward the next Haste
//   kitCounter[1]  alternates the uppercut hand
//   kitTimer[0]    Slip window
// ===========================================================================
class WadeKit : public Kit {
 public:
  void cast(AbilityContext& ctx, int slot) override {
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    uint32_t selfId = self.id;

    if (slot == 0) {
      // Simple Punches
      const float range = 88.0f;
      sys.fxSlash(self.x, self.y, ctx.dirX, ctx.dirY, range, 70.0f,
                  VfxTheme::Steel);
      sys.enemiesInCone(self.x, self.y, ctx.dirX, ctx.dirY, range, 45.0f,
                        [&](Enemy& enemy) {
                          sys.damageEnemy(selfId, enemy, 15.0f,
                                          DamageFlag::Basic | DamageFlag::Melee);
                        });
      if (++self.kitCounter[0] >= 3) {
        self.kitCounter[0] = 0;
        sys.applyToPlayer(self, EffectType::Haste, 1, selfId);
        self.cooldownMs[3] = 0.0f;
      }
    } else if (slot == 1) {
      // Uppercuts (channel)
      const float range = 92.0f;
      self.kitCounter[1] ^= 1;
      // Alternate hands: nudge the swing left / right of the aim line.
      float side = self.kitCounter[1] ? 0.25f : -0.25f;
      float dx = ctx.dirX - ctx.dirY * side;
      float dy = ctx.dirY + ctx.dirX * side;
      float d = std::sqrt(dx * dx + dy * dy);
      sys.fxSlash(self.x, self.y, dx / d, dy / d, range, 60.0f,
                  VfxTheme::Steel);
      sys.enemiesInCone(self.x, self.y, ctx.dirX, ctx.dirY, range, 42.0f,
                        [&](Enemy& enemy) {
                          sys.damageEnemy(selfId, enemy, 13.0f,
                                          DamageFlag::Melee);
                        });
    } else if (slot == 2) {
      // Slip
      self.kitTimer[0] = 500.0f;
      self.intangibleMs = std::max(self.intangibleMs, 500.0f);
      sys.fxRing(self.x, self.y, 46.0f, VfxTheme::Steel);
    } else if (slot == 3) {
      // Roll
      sys.effects().removeEffect(selfId, false, EffectType::Slow);
      sys.effects().removeEffect(selfId, false, EffectType::Snared);
      sys.effects().removeEffect(selfId, false, EffectType::Stunned);
      const float distance = 190.0f;
      sys.fxDashTrail(self.x, self.y, self.x + ctx.dirX * distance,
                      self.y + ctx.dirY * distance, VfxTheme::Steel);
      sys.dashPlayer(self, ctx.dirX, ctx.dirY, distance, 200.0f, true);
    }
  }

  void tick(AbilitySystem&, Player& self, float dtMs) override {
    self.kitTimer[0] = std::max(0.0f, self.kitTimer[0] - dtMs);
    self.resource = static_cast<uint8_t>(self.kitCounter[0] * 85);
  }

  // Slip: get hit during the window and answer with a haymaker.
  float onTakeDamage(AbilitySystem& sys, Player& self, float damage,
                     uint32_t sourceEnemyId) override {
    if (self.kitTimer[0] <= 0.0f) return damage;
    self.kitTimer[0] = 0.0f;
    self.cooldownMs[2] = 0.0f;

    float dirX = 1.0f, dirY = 0.0f;
    if (Enemy* attacker = sys.findEnemy(sourceEnemyId)) {
      float d = dist(self.x, self.y, attacker->x, attacker->y);
      if (d > 0.001f) {
        dirX = (attacker->x - self.x) / d;
        dirY = (attacker->y - self.y) / d;
      }
    } else {
      float d = dist(self.x, self.y, self.aimX, self.aimY);
      if (d > 0.001f) {
        dirX = (self.aimX - self.x) / d;
        dirY = (self.aimY - self.y) / d;
      }
    }
    const float range = 270.0f;
    uint32_t selfId = self.id;
    sys.fxCone(self.x, self.y, dirX, dirY, range, 60.0f, VfxTheme::Steel);
    sys.fxBeam(self.x, self.y, self.x + dirX * range, self.y + dirY * range,
               26.0f, 220.0f, VfxTheme::Steel);
    sys.enemiesInCone(self.x, self.y, dirX, dirY, range, 30.0f,
                      [&](Enemy& enemy) {
                        sys.knockbackEnemy(enemy, self.x, self.y, 120.0f);
                        sys.damageEnemy(selfId, enemy, 70.0f, DamageFlag::Melee);
                      });
    return 0.0f;
  }
};

}  // namespace

std::unique_ptr<Kit> makeKit(uint32_t characterId) {
  switch (characterId) {
    case Suds:
      return std::make_unique<SudsKit>();
    case Gravon:
      return std::make_unique<GravonKit>();
    case Isaac:
      return std::make_unique<IsaacKit>();
    case Zephyr:
      return std::make_unique<ZephyrKit>();
    case Mina:
      return std::make_unique<MinaKit>();
    case Volgore:
      return std::make_unique<VolgoreKit>();
    case Wade:
      return std::make_unique<WadeKit>();
    default:
      return nullptr;
  }
}
