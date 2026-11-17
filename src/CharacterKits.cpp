// Character kits. Each class implements one character from the "Character
// Concepts" design doc on top of AbilitySystem's helpers. Text for the HUD
// lives in ArenaData.cpp; numbers here are prototype tuning.
//
// Slots: 0 = LMB, 1 = RMB (hold), 2 = Q, 3 = LSHIFT.
// Per-player state lives in Player::kitTimer / kitCounter / kitTarget; each
// kit documents how it uses them.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "AbilitySystem.h"
#include "EffectManager.h"
#include "EnemySystem.h"
#include "Infusions.h"
#include "Logger.h"

namespace {

using Arena::CharacterId::Gravon;
using Arena::CharacterId::Isaac;
using Arena::CharacterId::Mina;
using Arena::CharacterId::Nolan;
using Arena::CharacterId::Nyx;
using Arena::CharacterId::Presidente;
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
//
// Gravon is the sword; the man is its host. They have separate health. While
// the host stands, Player::health is the host's and the blade's waits in
// kitTimer[1]. When the host falls the player *becomes* the blade: health
// and maxHealth are the blade's, it lies where it fell (or rides an ally's
// back) and cannot move or act until the host rises again.
//
// The host is slow, and slower with the blade out. Any attack draws it; a
// few seconds without one and he puts it back on his back. The first swing
// is a one-handed heave (off his back, or up from where he was dragging it
// on the run); after that he swings it back and forth in both hands.
//
//   kitCounter[0]  GravonMode form (mirrored in kitMode's low nibble)
//   kitCounter[1]  host: ms he has been on the move since his last attack
//   kitCounter[2]  host flags: SWING_BACK (the next swing is a backhand),
//                  LANDING (Ravager's leap and recovery), FULL_SHOWN
//   kitCounter[3]  epoch: bumped on every form change, so delayed strikes
//                  from a past life do nothing
//   kitTimer[0]    host: time until he sheathes (0 = on his back);
//                  blade: time until the host rises
//   kitTimer[1]    the blade's health while the host stands
//   kitTimer[2]    Fury of the Blade time left
//   kitTimer[3]    host: time left in a swing (slowed) or the Ravager's
//                  landing (rooted); blade: pickup delay
//   kitTarget      the ally carrying the blade
//   propX, propY   where the host fell
//   kitX           the carrier's id, for clients
// ===========================================================================
class GravonKit : public Kit {
 public:
  static constexpr float BLADE_MAX_HEALTH = 120.0f;
  static constexpr float REVIVE_MS = 15000.0f;
  static constexpr float REVIVE_PER_KILL_MS = 1000.0f;
  static constexpr float REVIVE_HEALTH_FRACTION = 0.6f;
  static constexpr float PICKUP_RADIUS = 55.0f;
  static constexpr float PICKUP_DELAY_MS = 600.0f;

  // Moving with the blade out (dragging it), on top of the slow base speed.
  static constexpr float DRAWN_SPEED_MULT = 0.85f;
  static constexpr float SHEATHE_AFTER_MS = 4000.0f;
  static constexpr int RUN_OPENER_MS = 250;  // on the move this long: heave

  // Titan's Slash: one 180-degree horizontal arc in front of him. The
  // one-handed heaves wind up longer than the two-handed swings.
  static constexpr float HEAVE_WINDUP_MS = 420.0f;
  static constexpr float HEAVE_RUN_WINDUP_MS = 380.0f;
  static constexpr float SLASH_WINDUP_MS = 300.0f;
  static constexpr float SLASH_RECOVER_MS = 260.0f;
  static constexpr float HEAVE_DAMAGE = 46.0f;
  static constexpr float SLASH_DAMAGE = 38.0f;
  static constexpr float SLASH_RANGE = 135.0f;
  static constexpr float SLASH_HALF_ANGLE = 90.0f;
  static constexpr float SWING_SPEED_MULT = 0.35f;

  // Ravager's Edge: he throws it over with his whole weight; once it is in
  // front of him it drags him a short lurch forward (LEAP over LEAP_MS)
  // and lands, and he spends the rest of LANDING_MS hauling it out of the
  // floor and back onto his shoulder, rooted.
  static constexpr float RAVAGER_REACH = 95.0f;
  static constexpr float RAVAGER_LEAP = 15.0f;
  static constexpr float RAVAGER_LEAP_MS = 300.0f;
  static constexpr float RAVAGER_LANDING_MS = 1200.0f;
  static constexpr float RAVAGER_RADIUS = 110.0f;
  static constexpr float RAVAGER_RADIUS_CHARGED = 50.0f;
  static constexpr float RAVAGER_DAMAGE = 35.0f;
  static constexpr float RAVAGER_DAMAGE_CHARGED = 115.0f;

  static constexpr float FURY_MS = 6000.0f;
  static constexpr float FURY_RUSH_REFUND_MS = 700.0f;

  static constexpr float RUSH_SLIDE = 190.0f;
  static constexpr float RUSH_SLIDE_MS = 170.0f;
  static constexpr float RUSH_BEAT_MS = 90.0f;  // the pause between the two
  static constexpr float RUSH_STAB_LUNGE = 45.0f;
  static constexpr float RUSH_STAB_MS = 70.0f;
  static constexpr float RUSH_STAB_REACH = 95.0f;
  static constexpr float RUSH_STAB_DAMAGE = 62.0f;

  // kitCounter[2] flags (host)
  static constexpr int SWING_BACK = 1 << 0;
  static constexpr int LANDING = 1 << 1;
  static constexpr int FULL_SHOWN = 1 << 2;

  static float hostMaxHealth() {
    const Arena::KitInfo* info = Arena::kitInfo(Gravon);
    assert(info && "Gravon has a kit");
    return info->maxHealth;
  }
  static float ravagerMaxChargeMs() {
    const Arena::KitInfo* info = Arena::kitInfo(Gravon);
    assert(info && "Gravon has a kit");
    return info->abilities[1].maxChargeMs;
  }
  static int form(const Player& self) { return self.kitCounter[0]; }
  static bool isBlade(const Player& self) {
    return form(self) != Arena::GravonMode::Host;
  }
  static bool isDrawn(const Player& self) {
    return !isBlade(self) && self.kitTimer[0] > 0.0f;
  }
  static bool isLanding(const Player& self) {
    return !isBlade(self) && (self.kitCounter[2] & LANDING) != 0;
  }

  bool canCast(AbilitySystem&, const Player& self, int) override {
    // A sword on the floor swings at nothing; a man pulling one out of the
    // floor cannot either.
    return !isBlade(self) && !isLanding(self);
  }

  float speedMultiplier(const Player& self) const override {
    if (isBlade(self)) return 1.0f;
    if (self.kitTimer[3] > 0.0f) return SWING_SPEED_MULT;
    return isDrawn(self) ? DRAWN_SPEED_MULT : 1.0f;
  }

  void chargeTick(AbilityContext& ctx, int slot, float) override {
    if (slot != 1) return;
    Player& self = ctx.self;
    self.kitAnchored = true;  // Ravager's Edge roots him while it builds
    draw(self);
    if (ctx.charge >= 1.0f && !(self.kitCounter[2] & FULL_SHOWN)) {
      self.kitCounter[2] |= FULL_SHOWN;
      ctx.sys.fxRing(self.x, self.y, 70.0f, VfxTheme::Blood);
    }
  }

  void cast(AbilityContext& ctx, int slot) override {
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    assert(!isBlade(self) && "canCast refuses in blade form");
    AbilitySystem* s = &sys;
    uint32_t selfId = self.id;
    int epoch = self.kitCounter[3];
    float dirX = ctx.dirX, dirY = ctx.dirY;
    bool wasDrawn = isDrawn(self);
    bool onTheRun = self.kitCounter[1] >= RUN_OPENER_MS;
    self.kitCounter[1] = 0;

    if (slot == 0) {
      // Titan's Slash. Which swing: heave it off his back, heave it up from
      // the drag, or swing it back and forth in both hands.
      float windup = SLASH_WINDUP_MS;
      float damage = SLASH_DAMAGE;
      if (!wasDrawn) {
        // Backhand off the back: it ends on his right, so a forehand next.
        self.castVariant = Arena::GravonForm::SlashHeave;
        windup = HEAVE_WINDUP_MS;
        damage = HEAVE_DAMAGE;
        self.kitCounter[2] &= ~SWING_BACK;
      } else if (onTheRun) {
        // Forehand up from behind: it ends on his left, so a backhand next.
        self.castVariant = Arena::GravonForm::SlashHeaveRun;
        windup = HEAVE_RUN_WINDUP_MS;
        damage = HEAVE_DAMAGE;
        self.kitCounter[2] |= SWING_BACK;
      } else {
        bool back = (self.kitCounter[2] & SWING_BACK) != 0;
        self.castVariant = back ? Arena::GravonForm::SlashBackhand
                                : Arena::GravonForm::SlashForehand;
        self.kitCounter[2] ^= SWING_BACK;
      }
      draw(self);
      self.actionLockMs =
          std::max(self.actionLockMs, windup + SLASH_RECOVER_MS);
      self.kitTimer[3] = windup + SLASH_RECOVER_MS;
      sys.schedule(windup, [s, selfId, epoch, dirX, dirY, damage]() {
        Player* me = strikingHost(*s, selfId, epoch);
        if (!me) return;
        s->fxSlash(me->x, me->y, dirX, dirY, SLASH_RANGE,
                   SLASH_HALF_ANGLE * 2.0f, VfxTheme::Shadow);
        float x = me->x, y = me->y;
        s->enemiesInCone(x, y, dirX, dirY, SLASH_RANGE, SLASH_HALF_ANGLE,
                         [&](Enemy& enemy) {
                           uint32_t enemyId = enemy.id;
                           s->damageEnemy(selfId, enemy, damage,
                                          DamageFlag::Basic | DamageFlag::Melee);
                           if (Enemy* still = s->findEnemy(enemyId)) {
                             s->knockbackEnemy(*still, x, y, 22.0f);
                           }
                         });
      });
    } else if (slot == 1) {
      // Ravager's Edge: everything goes into the blade. Its weight pulls
      // him over after it as it lands, and he has to heave it out again.
      float charge = ctx.charge;
      bool full = charge >= 0.98f;
      float radius = RAVAGER_RADIUS + RAVAGER_RADIUS_CHARGED * charge;
      float damage = RAVAGER_DAMAGE + RAVAGER_DAMAGE_CHARGED * charge;
      uint32_t flags = DamageFlag::Melee;
      if (full) {
        sys.applyToPlayer(self, EffectType::Sharpened, 1, selfId);
        flags |= DamageFlag::ForceCrit;
      }
      draw(self);
      self.castVariant = full ? Arena::GravonForm::RavagerFull : 0;
      self.kitCounter[2] = (self.kitCounter[2] & ~FULL_SHOWN) | LANDING;
      self.kitAnchored = true;
      self.kitTimer[3] = RAVAGER_LANDING_MS;
      self.actionLockMs = std::max(self.actionLockMs, RAVAGER_LANDING_MS);
      sys.dashPlayer(self, dirX, dirY, RAVAGER_LEAP, RAVAGER_LEAP_MS, false);
      // Where it will land: the point ahead of where the lurch ends.
      float x = self.x + dirX * (RAVAGER_LEAP + RAVAGER_REACH);
      float y = self.y + dirY * (RAVAGER_LEAP + RAVAGER_REACH);
      sys.clampToWorld(x, y);
      sys.fxTelegraphCircle(x, y, radius, RAVAGER_LEAP_MS, VfxTheme::Shadow);
      sys.schedule(RAVAGER_LEAP_MS,
                   [s, selfId, epoch, x, y, radius, damage, flags, full]() {
        if (!strikingHost(*s, selfId, epoch)) return;
        s->fxBurst(x, y, radius, full ? VfxTheme::Blood : VfxTheme::Shadow);
        s->fxRing(x, y, radius * 1.15f, VfxTheme::Shadow);
        s->enemiesInCircle(x, y, radius, [&](Enemy& enemy) {
          s->damageEnemy(selfId, enemy, damage, flags);
        });
      });
    } else if (slot == 2) {
      // Fury of the Blade
      draw(self);
      sys.applyToPlayer(self, EffectType::Empowered, 1, selfId, FURY_MS);
      sys.applyToPlayer(self, EffectType::Haste, 1, selfId, FURY_MS);
      self.kitTimer[2] = FURY_MS;
      sys.fxRing(self.x, self.y, 80.0f, VfxTheme::Shadow);
    } else if (slot == 3) {
      // Blade Rush, in two beats: the slide, a breath, then the stab.
      draw(self);
      float endX = self.x + dirX * RUSH_SLIDE;
      float endY = self.y + dirY * RUSH_SLIDE;
      sys.clampToWorld(endX, endY);
      sys.fxDashTrail(self.x, self.y, endX, endY, VfxTheme::Shadow);
      sys.dashPlayer(self, dirX, dirY, RUSH_SLIDE, RUSH_SLIDE_MS, true);
      self.actionLockMs = std::max(
          self.actionLockMs, RUSH_SLIDE_MS + RUSH_BEAT_MS + RUSH_STAB_MS + 120.0f);
      sys.schedule(RUSH_SLIDE_MS + RUSH_BEAT_MS,
                   [s, selfId, epoch, dirX, dirY]() {
                     Player* me = strikingHost(*s, selfId, epoch);
                     if (me) stab(*s, *me, dirX, dirY);
                   });
    }
  }

  // The blade is out (or stays out) for a while.
  static void draw(Player& self) {
    self.kitTimer[0] = SHEATHE_AFTER_MS;
  }

  // The second beat of Blade Rush: a short lunge and one deep thrust into
  // the first enemy on the line.
  static void stab(AbilitySystem& sys, Player& self, float dirX, float dirY) {
    draw(self);
    float reach = RUSH_STAB_LUNGE + RUSH_STAB_REACH;
    float tipX = self.x + dirX * reach;
    float tipY = self.y + dirY * reach;
    uint32_t firstId = 0;
    float firstDist = reach * 2.0f;
    sys.enemiesInLine(self.x, self.y, tipX, tipY, 30.0f, [&](Enemy& enemy) {
      float d = dist(self.x, self.y, enemy.x, enemy.y);
      if (d < firstDist) {
        firstDist = d;
        firstId = enemy.id;
      }
    });
    sys.fxBeam(self.x, self.y, tipX, tipY, 14.0f, 140.0f, VfxTheme::Blood);
    sys.dashPlayer(self, dirX, dirY, RUSH_STAB_LUNGE, RUSH_STAB_MS, true);

    // Clients play the thrust as its own clip (GravonForm::RushStab).
    GameEventPacket thrust;
    thrust.kind = GameEventKind::AbilityCast;
    thrust.sourceId = self.id;
    thrust.x = self.x;
    thrust.y = self.y;
    thrust.x2 = self.x + dirX * 200.0f;
    thrust.y2 = self.y + dirY * 200.0f;
    thrust.value = 260.0f;
    thrust.param = 3;
    thrust.extraId = Arena::GravonForm::RushStab;
    sys.emit(thrust);

    if (Enemy* first = sys.findEnemy(firstId)) {
      sys.damageEnemy(self.id, *first, RUSH_STAB_DAMAGE, DamageFlag::Melee);
    }
  }

  // The host, if he is still the same living man who started the swing.
  static Player* strikingHost(AbilitySystem& sys, uint32_t id, int epoch) {
    Player* me = sys.findPlayer(id);
    if (!me || me->isDead() || isBlade(*me) || me->kitCounter[3] != epoch) {
      return nullptr;
    }
    return me;
  }

  void onDealDamage(AbilitySystem& sys, Player& self, Enemy&, float,
                    uint32_t flags) override {
    if (flags & DamageFlag::Dot) return;
    const ActiveEffects& fx = sys.effects().getPlayerEffects(self.id);
    if (fx.hasEffect(EffectType::Empowered) || fx.hasEffect(EffectType::Haste)) {
      self.cooldownMs[3] = std::max(0.0f, self.cooldownMs[3] - FURY_RUSH_REFUND_MS);
    }
  }

  // Soulblade. The host falling exposes the blade; the blade breaking is
  // death.
  bool onLethal(AbilitySystem& sys, Player& self) override {
    if (isBlade(self)) {
      // The blade broke. He comes back (revived, or next round) as the host,
      // with the blade half mended.
      setForm(self, Arena::GravonMode::Host);
      self.maxHealth = hostMaxHealth();
      self.kitTimer[0] = 0.0f;
      self.kitTimer[1] = BLADE_MAX_HEALTH * 0.5f;
      self.kitTarget = 0;
      self.kitAnchored = false;
      return false;
    }
    float bladeHealth = self.kitTimer[1];
    assert(bladeHealth > 0.0f && "the blade outlives its host");
    setForm(self, Arena::GravonMode::Exposed);
    self.propX = self.x;
    self.propY = self.y;
    self.maxHealth = BLADE_MAX_HEALTH;
    self.health = bladeHealth;
    self.kitTimer[0] = REVIVE_MS;
    self.kitTimer[2] = 0.0f;
    self.kitTimer[3] = PICKUP_DELAY_MS;
    self.kitCounter[1] = 0;
    self.kitCounter[2] = 0;
    self.kitTarget = 0;
    self.kitAnchored = true;
    self.chargeSlot = -1;
    self.forcedMs = 0.0f;
    self.shield = 0.0f;
    self.shieldMs = 0.0f;
    sys.effects().clearEntity(self.id, false);
    sys.fxBurst(self.x, self.y, 90.0f, VfxTheme::Shadow);
    return true;
  }

  void tick(AbilitySystem& sys, Player& self, float dtMs) override {
    if (!isBlade(self)) {
      tickHost(self, dtMs);
    } else {
      tickBlade(sys, self, dtMs);
    }
    uint8_t mode = static_cast<uint8_t>(form(self));
    if (self.kitTimer[2] > 0.0f) mode |= Arena::GravonMode::FuryBit;
    if (!isBlade(self) && self.chargeSlot == 1 &&
        self.chargeMs >= ravagerMaxChargeMs()) {
      mode |= Arena::GravonMode::ChargedBit;
    }
    if (isDrawn(self)) mode |= Arena::GravonMode::DrawnBit;
    self.kitMode = mode;
  }

  void onEnemyDiedNearby(AbilitySystem&, Player& self, const Enemy&) override {
    if (!isBlade(self)) return;
    self.kitTimer[0] = std::max(0.0f, self.kitTimer[0] - REVIVE_PER_KILL_MS);
  }

  void onRoundStart(Player& self) override {
    // Scratch state was just cleared, so he is the host again. If the round
    // turned over while he was the blade, give the host his body back.
    float hostMax = hostMaxHealth();
    if (self.maxHealth != hostMax) {
      self.maxHealth = hostMax;
      if (self.isAlive()) self.health = hostMax * REVIVE_HEALTH_FRACTION;
    }
    self.kitTimer[1] = BLADE_MAX_HEALTH;
  }

 private:
  static void setForm(Player& self, uint8_t next) {
    self.kitCounter[0] = next;
    self.kitCounter[3]++;
    self.kitMode = static_cast<uint8_t>((self.kitMode & 0xF0) | next);
  }

  static void tickHost(Player& self, float dtMs) {
    self.kitTimer[2] = std::max(0.0f, self.kitTimer[2] - dtMs);
    self.kitTimer[3] = std::max(0.0f, self.kitTimer[3] - dtMs);
    if (self.kitTimer[3] <= 0.0f) self.kitCounter[2] &= ~LANDING;
    if (self.chargeSlot == 1) {
      draw(self);  // the blade stays out while it builds
    } else {
      self.kitCounter[2] &= ~FULL_SHOWN;
      if (self.kitTimer[0] > 0.0f && self.kitTimer[3] <= 0.0f) {
        self.kitTimer[0] = std::max(0.0f, self.kitTimer[0] - dtMs);
        if (self.kitTimer[0] <= 0.0f) self.kitCounter[2] &= ~SWING_BACK;
      }
    }
    // Rooted while Ravager's Edge builds and while he pulls it back out.
    self.kitAnchored = self.chargeSlot == 1 || isLanding(self);
    // How long he has been on the move (a heave comes out of a run).
    bool moving = self.vx * self.vx + self.vy * self.vy > 20.0f * 20.0f;
    self.kitCounter[1] =
        moving ? std::min(self.kitCounter[1] + static_cast<int>(dtMs), 60000)
               : 0;
    self.kitX = 0.0f;
    // The meter is the blade's own health: what is left if the host falls.
    self.resource = static_cast<uint8_t>(
        std::clamp(255.0f * self.kitTimer[1] / BLADE_MAX_HEALTH, 0.0f, 255.0f));
  }

  void tickBlade(AbilitySystem& sys, Player& self, float dtMs) {
    self.kitAnchored = true;
    self.kitTimer[0] -= dtMs;
    if (self.kitTimer[0] <= 0.0f) {
      rise(sys, self);
      return;
    }
    self.resource = static_cast<uint8_t>(std::clamp(
        255.0f * (1.0f - self.kitTimer[0] / REVIVE_MS), 0.0f, 255.0f));

    if (form(self) == Arena::GravonMode::Carried) {
      Player* carrier = sys.findPlayer(self.kitTarget);
      if (!carrier || carrier->isDead() || carrier->characterId == 0) {
        // The carrier fell: the blade is on the floor again, where they were.
        setForm(self, Arena::GravonMode::Exposed);
        self.kitTarget = 0;
        self.kitTimer[3] = PICKUP_DELAY_MS;
        sys.fxBurst(self.x, self.y, 50.0f, VfxTheme::Shadow);
      } else {
        self.x = carrier->x;
        self.y = carrier->y;
        self.vx = self.vy = 0.0f;
        // On an ally's back nothing can reach it.
        self.intangibleMs = std::max(self.intangibleMs, 100.0f);
        self.hiddenMs = std::max(self.hiddenMs, 100.0f);
        self.kitX = static_cast<float>(carrier->id);
      }
      return;
    }

    // Exposed: the first ally to reach the blade takes it up.
    self.kitX = 0.0f;
    self.vx = self.vy = 0.0f;
    self.kitTimer[3] = std::max(0.0f, self.kitTimer[3] - dtMs);
    if (self.kitTimer[3] > 0.0f) return;
    Player* nearest = nullptr;
    float nearestDist = PICKUP_RADIUS;
    for (auto& [id, other] : sys.allPlayers()) {
      if (id == self.id || other.isDead() || other.characterId == 0) continue;
      if (other.characterId == Gravon && isBlade(other)) continue;
      float d = dist(self.x, self.y, other.x, other.y);
      if (d <= nearestDist) {
        nearestDist = d;
        nearest = &other;
      }
    }
    if (nearest) {
      setForm(self, Arena::GravonMode::Carried);
      self.kitTarget = nearest->id;
      self.kitX = static_cast<float>(nearest->id);
      sys.fxRing(self.x, self.y, 50.0f, VfxTheme::Shadow);
    }
  }

  // The delay is over: the host stands up where the blade is.
  void rise(AbilitySystem& sys, Player& self) {
    float bladeHealth = self.health;
    setForm(self, Arena::GravonMode::Host);
    self.kitTimer[0] = 0.0f;
    self.kitTimer[1] = bladeHealth;
    self.kitTimer[3] = 0.0f;
    self.kitTarget = 0;
    self.kitX = 0.0f;
    self.kitAnchored = false;
    self.maxHealth = hostMaxHealth();
    self.health = self.maxHealth * REVIVE_HEALTH_FRACTION;
    self.intangibleMs = std::max(self.intangibleMs, 800.0f);
    // One tick of forced motion: clients must not predict over the jump.
    self.forcedVx = self.forcedVy = 0.0f;
    self.forcedMs = std::max(self.forcedMs, 50.0f);
    sys.effects().clearEntity(self.id, false);
    sys.fxRing(self.x, self.y, 120.0f, VfxTheme::Shadow);
    sys.fxBurst(self.x, self.y, 70.0f, VfxTheme::Blood);
  }
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
// Nolan, the Prodigy (Striker)
// Wade's hype foil: every move is a kick. He throws foes into the air and
// stacks Expose on them, and his Q and Shift only work on an airborne foe:
// sweep it up, teleport to where it lands, kick it back down.
//
// Passive (No guys, I don't need a passive either): knock-ups and
// knockbacks last longer for every stack of Expose on the target. Stacks
// are read before the hit, since the hit's damage consumes one.
// Kicks land on frame 2 of the forge's 24 fps clips.
// ===========================================================================
class NolanKit : public Kit {
 public:
  static constexpr float KICK_MS = 2000.0f / 24.0f;
  static constexpr float EXPOSE_SCALE = 0.35f;  // +35% per Expose stack

  // LMB: Lower Limb Devastation. A floor sweep, then a hop back.
  static constexpr float SWEEP_RANGE = 120.0f;
  static constexpr float SWEEP_HALF_ANGLE = 80.0f;
  static constexpr float SWEEP_DAMAGE = 16.0f;
  static constexpr float SWEEP_AIR_MS = 650.0f;
  static constexpr float HOP_AT_MS = 6000.0f / 24.0f;  // frame 6
  static constexpr float HOP_DISTANCE = 80.0f;
  static constexpr float HOP_MS = 120.0f;

  // RMB: Spinning Lotus Crane Tortoise Sky Splitter. Knockback all around.
  static constexpr float LOTUS_RADIUS = 150.0f;
  static constexpr float LOTUS_PUSH_MIN = 90.0f;
  static constexpr float LOTUS_PUSH_MAX = 260.0f;
  static constexpr float LOTUS_PUSH_MS = 170.0f;

  // Q: Sun Slaying Black-out Kick of the Demon God.
  static constexpr float BLACKOUT_REACH = 230.0f;
  static constexpr float BLACKOUT_DAMAGE = 48.0f;
  static constexpr float BLACKOUT_STUN_RADIUS = 140.0f;
  static constexpr float BLACKOUT_STUN_MS = 1400.0f;
  static constexpr float RESONANCE_MS = 8000.0f;

  // LSHIFT: Anime Shit. Teleport to where an airborne foe will land.
  static constexpr float BLINK_REACH = 520.0f;

  static float exposeScale(AbilitySystem& sys, const Enemy& enemy) {
    int stacks =
        sys.effects().getEnemyEffects(enemy.id).getStacks(EffectType::Expose);
    return 1.0f + EXPOSE_SCALE * static_cast<float>(stacks);
  }

  // The airborne foe nearest the cursor within reach of Nolan.
  static Enemy* airborneTarget(AbilitySystem& sys, const Player& self,
                               float reach) {
    Enemy* best = nullptr;
    float bestD = 1e9f;
    sys.enemiesInCircle(self.x, self.y, reach, [&](Enemy& enemy) {
      if (!enemy.airborne()) return;
      float d = dist(enemy.x, enemy.y, self.aimX, self.aimY);
      if (d < bestD) {
        bestD = d;
        best = &enemy;
      }
    });
    return best;
  }

  // Where an airborne foe comes down: the end of any knockback it is in.
  static void landingPoint(AbilitySystem& sys, const Enemy& enemy, float& x,
                           float& y) {
    float t = std::max(0.0f, enemy.knockMs) / 1000.0f;
    x = enemy.x + enemy.knockVx * t;
    y = enemy.y + enemy.knockVy * t;
    sys.clampToWorld(x, y);
  }

  bool canCast(AbilitySystem& sys, const Player& self, int slot) override {
    if (slot != 2 && slot != 3) return true;
    float reach = slot == 2 ? BLACKOUT_REACH : BLINK_REACH;
    if (airborneTarget(sys, self, reach)) return true;
    // Nothing in the air: show his reach so the player learns it.
    sys.fxRing(self.x, self.y, reach, VfxTheme::Neutral);
    return false;
  }

  void cast(AbilityContext& ctx, int slot) override {
    switch (slot) {
      case 0:
        sweep(ctx);
        break;
      case 1:
        lotus(ctx);
        break;
      case 2:
        blackout(ctx);
        break;
      case 3:
        blink(ctx);
        break;
      default:
        assert(false && "Nolan has four kit slots");
    }
  }

 private:
  static void sweep(AbilityContext& ctx) {
    AbilitySystem* sys = &ctx.sys;
    const uint32_t id = ctx.self.id;
    const float x = ctx.self.x, y = ctx.self.y;
    const float dx = ctx.dirX, dy = ctx.dirY;
    sys->schedule(KICK_MS, [sys, id, x, y, dx, dy]() {
      Player* self = sys->findPlayer(id);
      if (!self || !self->isAlive()) return;
      sys->fxSlash(x, y, dx, dy, SWEEP_RANGE, SWEEP_HALF_ANGLE * 2.0f,
                   VfxTheme::Wind);
      sys->enemiesInCone(x, y, dx, dy, SWEEP_RANGE, SWEEP_HALF_ANGLE,
                         [&](Enemy& enemy) {
                           uint32_t foeId = enemy.id;
                           float scale = exposeScale(*sys, enemy);
                           sys->damageEnemy(id, enemy, SWEEP_DAMAGE,
                                            DamageFlag::Melee | DamageFlag::Basic);
                           Enemy* foe = sys->findEnemy(foeId);
                           if (!foe || foe->state == EnemyState::Dead) return;
                           sys->knockUpEnemy(*foe, SWEEP_AIR_MS * scale);
                           sys->applyToEnemy(*foe, EffectType::Expose, 1, id);
                         });
    });
    // The hop back, as the clip lands it.
    sys->schedule(HOP_AT_MS, [sys, id, dx, dy]() {
      Player* self = sys->findPlayer(id);
      if (!self || !self->isAlive()) return;
      sys->dashPlayer(*self, -dx, -dy, HOP_DISTANCE, HOP_MS, false);
    });
  }

  static void lotus(AbilityContext& ctx) {
    AbilitySystem* sys = &ctx.sys;
    const uint32_t id = ctx.self.id;
    const float x = ctx.self.x, y = ctx.self.y;
    const float push =
        LOTUS_PUSH_MIN + (LOTUS_PUSH_MAX - LOTUS_PUSH_MIN) * ctx.charge;
    sys->schedule(KICK_MS, [sys, id, x, y, push]() {
      Player* self = sys->findPlayer(id);
      if (!self || !self->isAlive()) return;
      sys->fxRing(x, y, LOTUS_RADIUS, VfxTheme::Wind);
      sys->enemiesInCircle(x, y, LOTUS_RADIUS, [&](Enemy& enemy) {
        float scale = exposeScale(*sys, enemy);
        float dx = enemy.x - x, dy = enemy.y - y;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d < 0.01f) {
          dx = 1.0f;
          dy = 0.0f;
          d = 1.0f;
        }
        sys->enemySystem().pushEnemy(enemy, dx / d, dy / d, push * scale,
                                     LOTUS_PUSH_MS * scale);
        sys->applyToEnemy(enemy, EffectType::Expose, 1, id);
      });
    });
  }

  static void blackout(AbilityContext& ctx) {
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    Enemy* target = airborneTarget(sys, self, BLACKOUT_REACH);
    assert(target && "canCast checked for an airborne target");
    // Step in beside it for the kick.
    float dx = target->x - self.x, dy = target->y - self.y;
    float d = std::sqrt(dx * dx + dy * dy);
    if (d > 70.0f) {
      sys.blinkPlayer(self, target->x - dx / d * 55.0f,
                      target->y - dy / d * 55.0f);
    }
    AbilitySystem* s = &sys;
    const uint32_t id = self.id, foeId = target->id;
    sys.schedule(KICK_MS, [s, id, foeId]() {
      Player* nolan = s->findPlayer(id);
      Enemy* foe = s->findEnemy(foeId);
      if (!nolan || !nolan->isAlive() || !foe) return;
      foe->airMs = 0.0f;  // driven into the ground
      foe->knockMs = 0.0f;
      const float x = foe->x, y = foe->y;
      s->fxBurst(x, y, BLACKOUT_STUN_RADIUS, VfxTheme::Wind);
      s->damageEnemy(id, *foe, BLACKOUT_DAMAGE, DamageFlag::Melee);
      s->enemiesInCircle(x, y, BLACKOUT_STUN_RADIUS, [&](Enemy& enemy) {
        s->applyToEnemy(enemy, EffectType::Stunned, 1, id, BLACKOUT_STUN_MS);
      });
      // When I win, we win: Wade gets Resonance for a long Roll.
      for (auto& [otherId, ally] : s->allPlayers()) {
        if (otherId == id || !ally.isAlive()) continue;
        if (ally.characterId != Arena::CharacterId::Wade) continue;
        int before = s->effects().getPlayerEffects(otherId).getStacks(
            EffectType::Resonance);
        s->applyToPlayer(ally, EffectType::Resonance, 1, id, RESONANCE_MS);
        int after = s->effects().getPlayerEffects(otherId).getStacks(
            EffectType::Resonance);
        if (after != before) {
          s->fxEffectVerb(EffectVerb::Apply, EffectType::Resonance, otherId,
                          false, ally.x, ally.y, static_cast<float>(before),
                          static_cast<float>(after), id, nolan->x, nolan->y);
        }
      }
    });
  }

  static void blink(AbilityContext& ctx) {
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    Enemy* target = airborneTarget(sys, self, BLINK_REACH);
    assert(target && "canCast checked for an airborne target");
    float lx, ly;
    landingPoint(sys, *target, lx, ly);
    // Arrive at the landing point, just short of the foe's body.
    float dx = lx - self.x, dy = ly - self.y;
    float d = std::sqrt(dx * dx + dy * dy);
    float gap = Arena::Tuning::PLAYER_RADIUS + target->radius;
    if (d > gap) {
      lx -= dx / d * gap;
      ly -= dy / d * gap;
    }
    sys.fxDashTrail(self.x, self.y, lx, ly, VfxTheme::Wind);
    sys.blinkPlayer(self, lx, ly);
    sys.applyToEnemy(*target, EffectType::Expose, 1, self.id);
  }
};

// ===========================================================================
// Wade, the Old Head (Striker)
//   kitCounter[0]  punches thrown in the current combination (0-2); the
//                  third grants Haste
//   kitCounter[1]  alternates the uppercut hand
//   kitCounter[2]  which combination he is throwing, plus 1 (0 = none yet)
//   kitTimer[0]    Slip window: struck while it runs, he counters
// Every cast sets Player::castVariant (Arena::WadeForm) so clients play the
// punch the server threw.
// ===========================================================================
class WadeKit : public Kit {
public:
  // Three-punch boxing combinations. Hands alternate, or the lead hand
  // doubles up (1-1-2, body hook into head hook); the rear hand never
  // throws twice in a row. The third punch is the one that grants Haste.
  static constexpr int COMBO_LENGTH = 3;
  static constexpr int COMBO_COUNT = 10;
  static constexpr uint8_t COMBOS[COMBO_COUNT][COMBO_LENGTH] = {
      {Arena::WadeForm::Jab, Arena::WadeForm::Cross,
       Arena::WadeForm::LeadHook}, // 1-2-3
      {Arena::WadeForm::Jab, Arena::WadeForm::Jab,
       Arena::WadeForm::Cross}, // 1-1-2
      {Arena::WadeForm::Jab, Arena::WadeForm::Cross,
       Arena::WadeForm::HookBody}, // 1-2-3b
      {Arena::WadeForm::Jab, Arena::WadeForm::CrossBody,
       Arena::WadeForm::LeadHook}, // 1-2b-3
      {Arena::WadeForm::Jab, Arena::WadeForm::LeadHook,
       Arena::WadeForm::Cross}, // 1-3-2
      {Arena::WadeForm::Jab, Arena::WadeForm::RearHook,
       Arena::WadeForm::LeadHook}, // 1-4-3
      {Arena::WadeForm::Cross, Arena::WadeForm::LeadHook,
       Arena::WadeForm::Cross}, // 2-3-2
      {Arena::WadeForm::Cross, Arena::WadeForm::LeadHook,
       Arena::WadeForm::RearHook}, // 2-3-4
      {Arena::WadeForm::LeadHook, Arena::WadeForm::Cross,
       Arena::WadeForm::LeadHook}, // 3-2-3
      {Arena::WadeForm::HookBody, Arena::WadeForm::LeadHook,
       Arena::WadeForm::Cross}, // 3b-3-2
  };

  static constexpr float SLIP_MS = 500.0f;
  // The haymaker lands this long after the slipped hit: long enough to
  // see him load it, short enough that it is still the answer to that hit.
  // Matches the impact frame of the forge's "haymaker" clip.
  static constexpr float HAYMAKER_WINDUP_MS = 210.0f;
  static constexpr float HAYMAKER_RANGE = 270.0f;
  static constexpr float ROLL_DISTANCE = 190.0f;
  static constexpr float ROLL_MS = 200.0f;
  static constexpr float RESONANCE_ROLL_DISTANCE = 430.0f;
  static constexpr float RESONANCE_ROLL_MS = 300.0f;

  void cast(AbilityContext &ctx, int slot) override {
    AbilitySystem &sys = ctx.sys;
    Player &self = ctx.self;
    uint32_t selfId = self.id;

    if (slot == 0) {
      // Simple Punches: the next punch of a combination picked at random.
      int step = self.kitCounter[0];
      assert(step >= 0 && step < COMBO_LENGTH);
      if (step == 0) {
        // Never the same combination twice in a row.
        int last = self.kitCounter[2] - 1; // -1 before his first punch
        int pick = static_cast<int>(sys.random01() * COMBO_COUNT);
        pick = std::max(0, std::min(COMBO_COUNT - 1, pick));
        if (pick == last)
          pick = (pick + 1) % COMBO_COUNT;
        self.kitCounter[2] = pick + 1;
      }
      int combo = self.kitCounter[2] - 1;
      assert(combo >= 0 && combo < COMBO_COUNT);
      uint8_t punch = COMBOS[combo][step];
      self.castVariant = punch;

      // Every punch hits the same; only the picture differs. Straights
      // draw a tight thrust, hooks a wide arc from that hand's side.
      const float range = 88.0f;
      bool hook = punch == Arena::WadeForm::LeadHook ||
                  punch == Arena::WadeForm::RearHook ||
                  punch == Arena::WadeForm::HookBody;
      float side = 0.0f;
      if (hook)
        side = punch == Arena::WadeForm::RearHook ? 0.35f : -0.35f;
      float dx = ctx.dirX - ctx.dirY * side;
      float dy = ctx.dirY + ctx.dirX * side;
      float d = std::sqrt(dx * dx + dy * dy);
      sys.fxSlash(self.x, self.y, dx / d, dy / d, range, hook ? 110.0f : 50.0f,
                  VfxTheme::Steel);
      sys.enemiesInCone(
          self.x, self.y, ctx.dirX, ctx.dirY, range, 45.0f, [&](Enemy &enemy) {
            sys.damageEnemy(selfId, enemy, 15.0f,
                            DamageFlag::Basic | DamageFlag::Melee);
          });
      if (++self.kitCounter[0] >= COMBO_LENGTH) {
        self.kitCounter[0] = 0;
        sys.applyToPlayer(self, EffectType::Haste, 1, selfId);
        self.cooldownMs[3] = 0.0f;
      }
    } else if (slot == 1) {
      // Uppercuts (channel): 5, 6, 5, 6... The lead hand opens.
      const float range = 92.0f;
      bool rear = self.kitCounter[1] != 0;
      self.kitCounter[1] ^= 1;
      self.castVariant =
          rear ? Arena::WadeForm::RearUppercut : Arena::WadeForm::LeadUppercut;
      // Nudge the swing to the side of the hand that threw it.
      float side = rear ? 0.25f : -0.25f;
      float dx = ctx.dirX - ctx.dirY * side;
      float dy = ctx.dirY + ctx.dirX * side;
      float d = std::sqrt(dx * dx + dy * dy);
      sys.fxSlash(self.x, self.y, dx / d, dy / d, range, 60.0f,
                  VfxTheme::Steel);
      sys.enemiesInCone(
          self.x, self.y, ctx.dirX, ctx.dirY, range, 42.0f, [&](Enemy &enemy) {
            sys.damageEnemy(selfId, enemy, 13.0f, DamageFlag::Melee);
          });
    } else if (slot == 2) {
      // Slip: off the line with the rear hand chambered.
      self.castVariant = Arena::WadeForm::SlipChamber;
      self.kitTimer[0] = SLIP_MS;
      self.intangibleMs = std::max(self.intangibleMs, SLIP_MS);
      sys.fxRing(self.x, self.y, 46.0f, VfxTheme::Steel);
    } else if (slot == 3) {
      // Roll
      sys.effects().removeEffect(selfId, false, EffectType::Slow);
      sys.effects().removeEffect(selfId, false, EffectType::Snared);
      sys.effects().removeEffect(selfId, false, EffectType::Stunned);
      float distance = ROLL_DISTANCE;
      float durationMs = ROLL_MS;
      if (sys.effects().getPlayerEffects(selfId).hasEffect(
              EffectType::Resonance)) {
        // Discharge Resonance into the roll.
        sys.effects().removeEffect(selfId, false, EffectType::Resonance);
        distance = RESONANCE_ROLL_DISTANCE;
        durationMs = RESONANCE_ROLL_MS;
        self.castVariant = Arena::WadeForm::ResonanceRoll;
        sys.fxBurst(self.x, self.y, 70.0f, VfxTheme::Steel);
      }
      sys.fxDashTrail(self.x, self.y, self.x + ctx.dirX * distance,
                      self.y + ctx.dirY * distance, VfxTheme::Steel);
      sys.dashPlayer(self, ctx.dirX, ctx.dirY, distance, durationMs, true);
    }
  }

  // Uppercuts: a new hold always opens with the lead hand.
  void channelEnd(AbilitySystem &, Player &self, int slot) override {
    if (slot == 1)
      self.kitCounter[1] = 0;
  }

  // "Plants his feet": no walking while the uppercuts are going.
  float speedMultiplier(const Player &self) const override {
    return self.chargeSlot == 1 ? 0.0f : 1.0f;
  }

  void tick(AbilitySystem &, Player &self, float dtMs) override {
    self.kitTimer[0] = std::max(0.0f, self.kitTimer[0] - dtMs);
    self.resource = static_cast<uint8_t>(self.kitCounter[0] * 85);
  }

  // Slip: struck during the window, he slips it and answers with a haymaker
  // toward whoever threw it. The punch takes a beat to land; he stays
  // planted and untouchable until it does.
  float onTakeDamage(AbilitySystem &sys, Player &self, float damage,
                     uint32_t sourceEnemyId) override {
    if (self.kitTimer[0] <= 0.0f)
      return damage;
    self.kitTimer[0] = 0.0f;
    self.cooldownMs[2] = 0.0f;

    float dirX = 1.0f, dirY = 0.0f;
    if (Enemy *attacker = sys.findEnemy(sourceEnemyId)) {
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

    self.intangibleMs = std::max(self.intangibleMs, HAYMAKER_WINDUP_MS + 60.0f);
    self.chargeSlot = -1;
    self.forcedVx = self.forcedVy = 0.0f;
    self.forcedGhost = false;
    self.forcedMs = HAYMAKER_WINDUP_MS;

    // Tell clients: the slip landed, here comes the haymaker, that way.
    sys.fxRing(self.x, self.y, 30.0f, VfxTheme::Steel);
    GameEventPacket cast;
    cast.kind = GameEventKind::AbilityCast;
    cast.sourceId = self.id;
    cast.x = self.x;
    cast.y = self.y;
    cast.x2 = self.x + dirX * 100.0f;
    cast.y2 = self.y + dirY * 100.0f;
    cast.value = HAYMAKER_WINDUP_MS + 200.0f;
    cast.param = 2;
    cast.extraId = Arena::WadeForm::Haymaker;
    sys.emit(cast);

    uint32_t selfId = self.id;
    AbilitySystem *s = &sys;
    s->schedule(HAYMAKER_WINDUP_MS, [s, selfId, dirX, dirY]() {
      Player *wade = s->findPlayer(selfId);
      if (!wade || !wade->isAlive())
        return;
      float x = wade->x, y = wade->y;
      s->fxCone(x, y, dirX, dirY, HAYMAKER_RANGE, 60.0f, VfxTheme::Steel);
      s->fxBeam(x, y, x + dirX * HAYMAKER_RANGE, y + dirY * HAYMAKER_RANGE,
                26.0f, 220.0f, VfxTheme::Steel);
      s->enemiesInCone(
          x, y, dirX, dirY, HAYMAKER_RANGE, 30.0f, [&](Enemy &enemy) {
            s->knockbackEnemy(enemy, x, y, 120.0f);
            s->damageEnemy(selfId, enemy, 70.0f, DamageFlag::Melee);
          });
    });
    return 0.0f;
  }
};

// ===========================================================================
// El Presidente (Playmaker)
//   kitTarget     the foe he is holding (0 = hands free)
//   kitTimer[0]   how much longer he can hold them
//   kitTimer[1]   Abrazo de Oso cooldown (separate from Grande Cajones)
// While holding a foe he has Resonance and every move uses its alternate
// form. The held foe is Grappled: carried overhead, helpless.
// ===========================================================================
class PresidenteKit : public Kit {
 public:
  static constexpr float HOLD_MS = 3800.0f;
  static constexpr float LEAP_MS = 600.0f;   // matches the forge clips
  static constexpr float BOMBA_MS = 620.0f;
  static constexpr float SITOUT_MS = 260.0f;  // seated after the powerbomb

  static Enemy* held(AbilitySystem& sys, Player& self) {
    if (self.kitTarget == 0) return nullptr;
    Enemy* e = sys.findEnemy(self.kitTarget);
    if (!e || e->state == EnemyState::Dead) return nullptr;
    // Freed some other way (he went down, the grapple wore off).
    if (!sys.effects().getEnemyEffects(e->id).hasEffect(EffectType::Grappled)) {
      return nullptr;
    }
    return e;
  }

  static bool grab(AbilitySystem& sys, Player& self, Enemy& enemy) {
    sys.applyToEnemy(enemy, EffectType::Grappled, 1, self.id, HOLD_MS + 500.0f);
    if (!sys.effects().getEnemyEffects(enemy.id).hasEffect(
            EffectType::Grappled)) {
      return false;  // too big to lift (the boss)
    }
    self.kitTarget = enemy.id;
    self.kitTimer[0] = HOLD_MS;
    enemy.knockMs = 0.0f;
    sys.applyToPlayer(self, EffectType::Resonance, 1, self.id, HOLD_MS + 500.0f);
    return true;
  }

  static void release(AbilitySystem& sys, Player& self) {
    if (Enemy* e = sys.findEnemy(self.kitTarget)) {
      sys.effects().removeEffect(e->id, true, EffectType::Grappled);
      e->heldLow = false;
    }
    sys.effects().removeEffect(self.id, false, EffectType::Resonance);
    self.kitTarget = 0;
    self.kitTimer[0] = 0.0f;
  }

  // Leap toward (x, y); `land` runs where he comes down.
  static void leap(AbilitySystem& sys, Player& self, float x, float y,
                   float durationMs, std::function<void(Player&)> land) {
    float dx = x - self.x, dy = y - self.y;
    float d = std::sqrt(dx * dx + dy * dy);
    if (d > 1.0f) {
      sys.dashPlayer(self, dx / d, dy / d, d, durationMs, true);
    } else {
      self.forcedVx = self.forcedVy = 0.0f;
      self.forcedMs = durationMs;
    }
    uint32_t id = self.id;
    AbilitySystem* s = &sys;
    s->schedule(durationMs, [s, id, land]() {
      if (Player* p = s->findPlayer(id)) {
        if (p->isAlive()) land(*p);
      }
    });
  }

  static void slam(AbilitySystem& sys, Player& self, float radius,
                   float damage, float stunMs, VfxTheme theme) {
    uint32_t id = self.id;
    uint32_t heldId = self.kitTarget;
    float cx = self.x, cy = self.y;
    sys.fxBurst(cx, cy, radius, theme);
    sys.fxRing(cx, cy, radius + 24.0f, theme);
    sys.enemiesInCircle(cx, cy, radius, [&](Enemy& enemy) {
      if (enemy.id == heldId) return;
      sys.damageEnemy(id, enemy, damage, DamageFlag::Explosion);
      if (Enemy* still = sys.findEnemy(enemy.id)) {
        sys.applyToEnemy(*still, EffectType::Stunned, 1, id, stunMs);
        sys.knockbackEnemy(*still, cx, cy, 40.0f);
      }
    });
  }

  void cast(AbilityContext& ctx, int slot) override {
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    uint32_t selfId = self.id;
    Enemy* foe = held(sys, self);

    if (slot == 0) {
      if (foe) {
        // Látigo: whirl, scattering everyone, then hurl the foe.
        const float radius = 120.0f;
        uint32_t foeId = foe->id;
        sys.fxRing(self.x, self.y, radius, VfxTheme::Holy);
        sys.fxSlash(self.x, self.y, ctx.dirX, ctx.dirY, radius, 340.0f,
                    VfxTheme::Holy);
        sys.enemiesInCircle(self.x, self.y, radius, [&](Enemy& enemy) {
          if (enemy.id == foeId) return;
          sys.knockbackEnemy(enemy, self.x, self.y, 120.0f);
          sys.damageEnemy(selfId, enemy, 18.0f);
        });
        release(sys, self);
        if (Enemy* thrown = sys.findEnemy(foeId)) {
          thrown->x = self.x + ctx.dirX * 30.0f;
          thrown->y = self.y + ctx.dirY * 30.0f;
          sys.enemySystem().pushEnemy(*thrown, ctx.dirX, ctx.dirY, 300.0f,
                                      280.0f);
          sys.fxDashTrail(thrown->x, thrown->y, thrown->x + ctx.dirX * 300.0f,
                          thrown->y + ctx.dirY * 300.0f, VfxTheme::Holy);
        }
        AbilitySystem* s = &sys;
        sys.schedule(280.0f, [s, selfId, foeId]() {
          Enemy* landed = s->findEnemy(foeId);
          if (!landed || landed->state == EnemyState::Dead) return;
          float lx = landed->x, ly = landed->y;
          s->fxBurst(lx, ly, 80.0f, VfxTheme::Holy);
          s->enemiesInCircle(lx, ly, 80.0f, [&](Enemy& other) {
            float dmg = other.id == foeId ? 35.0f : 28.0f;
            s->damageEnemy(selfId, other, dmg, DamageFlag::Explosion);
            if (Enemy* still = s->findEnemy(other.id)) {
              s->applyToEnemy(*still, EffectType::Stunned, 1, selfId, 900.0f);
            }
          });
        });
        return;
      }
      // Lucha Libre: grab the closest foe in front.
      Enemy* best = nullptr;
      float bestDist = 1e9f;
      sys.enemiesInCone(self.x, self.y, ctx.dirX, ctx.dirY, 100.0f, 55.0f,
                        [&](Enemy& enemy) {
                          float d = dist(self.x, self.y, enemy.x, enemy.y);
                          if (d < bestDist) {
                            bestDist = d;
                            best = &enemy;
                          }
                        });
      sys.fxSlash(self.x, self.y, ctx.dirX, ctx.dirY, 100.0f, 90.0f,
                  VfxTheme::Holy);
      if (best) {
        uint32_t id = best->id;
        sys.damageEnemy(selfId, *best, 10.0f, DamageFlag::Basic |
                                                  DamageFlag::Melee);
        if (Enemy* still = sys.findEnemy(id)) grab(sys, self, *still);
      }
    } else if (slot == 1) {
      if (foe) {
        // Bomba: a leaping sit-out powerbomb. He comes down seated with
        // the foe driven into the ground just in front of him.
        float range = 170.0f + 250.0f * ctx.charge;
        float x = self.x + ctx.dirX * range, y = self.y + ctx.dirY * range;
        sys.clampToWorld(x, y);
        float radius = 90.0f + 70.0f * ctx.charge;
        float damage = 40.0f + 40.0f * ctx.charge;
        float fx = ctx.dirX, fy = ctx.dirY;
        sys.fxTelegraphCircle(x, y, radius, BOMBA_MS, VfxTheme::Holy);
        self.kitTimer[0] = std::max(self.kitTimer[0], BOMBA_MS + 200.0f);
        leap(sys, self, x, y, BOMBA_MS,
             [&sys, radius, damage, fx, fy](Player& p) {
          AbilitySystem& s2 = sys;
          slam(s2, p, radius, damage, 1000.0f, VfxTheme::Holy);
          // Sit out for a beat before getting up.
          p.forcedVx = p.forcedVy = 0.0f;
          p.forcedMs = SITOUT_MS;
          if (Enemy* victim = held(s2, p)) {
            uint32_t vid = victim->id;
            victim->x = p.x + fx * 30.0f;
            victim->y = p.y + fy * 30.0f;
            release(s2, p);
            if (Enemy* v = s2.findEnemy(vid)) {
              s2.damageEnemy(p.id, *v, damage * 1.5f, DamageFlag::Explosion);
              if (Enemy* still = s2.findEnemy(vid)) {
                s2.applyToEnemy(*still, EffectType::Stunned, 1, p.id, 1500.0f);
              }
            }
          }
        });
        return;
      }
      // Pesado: shoulder charge. First foe in the way is scooped up.
      float distance = 160.0f + 260.0f * ctx.charge;
      float endX = self.x + ctx.dirX * distance;
      float endY = self.y + ctx.dirY * distance;
      sys.clampToWorld(endX, endY);
      sys.fxDashTrail(self.x, self.y, endX, endY, VfxTheme::Blood);
      Enemy* first = nullptr;
      float firstDist = 1e9f;
      sys.enemiesInLine(self.x, self.y, endX, endY, 40.0f, [&](Enemy& enemy) {
        float d = dist(self.x, self.y, enemy.x, enemy.y);
        if (d < firstDist) {
          firstDist = d;
          first = &enemy;
        }
      });
      uint32_t firstId = first ? first->id : 0;
      float sx = self.x, sy = self.y;
      sys.enemiesInLine(self.x, self.y, endX, endY, 40.0f, [&](Enemy& enemy) {
        if (enemy.id == firstId) return;
        // Knock everyone else aside.
        float side = ((enemy.x - sx) * -ctx.dirY + (enemy.y - sy) * ctx.dirX) >= 0
                         ? 1.0f : -1.0f;
        sys.enemySystem().pushEnemy(enemy, -ctx.dirY * side, ctx.dirX * side,
                                    90.0f, 200.0f);
        sys.damageEnemy(selfId, enemy, 20.0f, DamageFlag::Melee);
        if (Enemy* still = sys.findEnemy(enemy.id)) {
          sys.applyToEnemy(*still, EffectType::Stunned, 1, selfId, 500.0f);
        }
      });
      if (first) {
        sys.damageEnemy(selfId, *first, 15.0f, DamageFlag::Melee);
        if (Enemy* still = sys.findEnemy(firstId)) grab(sys, self, *still);
      }
      sys.dashPlayer(self, ctx.dirX, ctx.dirY, distance, 330.0f, true);
    } else if (slot == 2) {
      if (foe) {
        // Abrazo de Oso: three crushing squeezes, then drop them stunned.
        if (self.kitTimer[1] > 0.0f) return;
        self.kitTimer[1] = 7000.0f;
        self.cooldownMs[2] = std::max(0.0f, self.cooldownMs[2] - 9000.0f);
        self.kitTimer[0] = std::max(self.kitTimer[0], 1200.0f);
        foe->heldLow = true;  // brought down to chest height for the hug
        AbilitySystem* s = &sys;
        for (int i = 0; i < 3; i++) {
          sys.schedule(180.0f + i * 230.0f, [s, selfId, i]() {
            Player* p = s->findPlayer(selfId);
            if (!p) return;
            Enemy* victim = held(*s, *p);
            if (!victim) return;
            uint32_t vid = victim->id;
            s->fxBurst(victim->x, victim->y, 34.0f, VfxTheme::Blood);
            s->damageEnemy(selfId, *victim, 24.0f, DamageFlag::Melee);
            if (i == 2) {
              if (Enemy* still = s->findEnemy(vid)) {
                release(*s, *p);
                s->applyToEnemy(*still, EffectType::Stunned, 1, selfId, 2000.0f);
              }
            }
          });
        }
        return;
      }
      // Grande Cajones: a flex that strips their nerve.
      const float range = 230.0f;
      sys.fxCone(self.x, self.y, ctx.dirX, ctx.dirY, range, 120.0f,
                 VfxTheme::Holy);
      sys.enemiesInCone(self.x, self.y, ctx.dirX, ctx.dirY, range, 60.0f,
                        [&](Enemy& enemy) {
                          sys.effects().mutableEffects(enemy.id, true)
                              .removeAllBuffs();
                          sys.applyToEnemy(enemy, EffectType::Vulnerable, 2,
                                           selfId);
                          sys.applyToEnemy(enemy, EffectType::Weakened, 1,
                                           selfId);
                        });
      sys.applyToPlayer(self, EffectType::Fortified, 2, selfId, 4000.0f);
    } else if (slot == 3) {
      // El Splash de Rana / Grande Rana Bomba: leap to the cursor.
      float x, y;
      aimPoint(ctx, 460.0f, x, y);
      bool carrying = foe != nullptr;
      float radius = carrying ? 150.0f : 110.0f;
      sys.fxTelegraphCircle(x, y, radius, LEAP_MS, VfxTheme::Holy);
      if (carrying) self.kitTimer[0] = std::max(self.kitTimer[0], LEAP_MS + 200.0f);
      leap(sys, self, x, y, LEAP_MS, [&sys, carrying, radius](Player& p) {
        slam(sys, p, radius, carrying ? 45.0f : 30.0f, carrying ? 900.0f : 600.0f,
             carrying ? VfxTheme::Fire : VfxTheme::Holy);
        if (!carrying) return;
        if (Enemy* victim = held(sys, p)) {
          uint32_t vid = victim->id;
          release(sys, p);
          if (Enemy* v = sys.findEnemy(vid)) {
            sys.damageEnemy(p.id, *v, 80.0f, DamageFlag::Explosion);
            if (Enemy* still = sys.findEnemy(vid)) {
              sys.applyToEnemy(*still, EffectType::Stunned, 1, p.id, 1500.0f);
            }
          }
        }
      });
    }
  }

  bool canRecast(const Player& self, int slot) const override {
    // Abrazo de Oso has its own cooldown, separate from Grande Cajones.
    return slot == 2 && self.kitTarget != 0 && self.kitTimer[1] <= 0.0f;
  }

  void tick(AbilitySystem& sys, Player& self, float dtMs) override {
    self.kitTimer[1] = std::max(0.0f, self.kitTimer[1] - dtMs);
    Enemy* foe = held(sys, self);
    if (!foe) {
      if (self.kitTarget != 0) release(sys, self);
      self.resource = 0;
      return;
    }
    self.kitTimer[0] -= dtMs;
    if (self.kitTimer[0] <= 0.0f) {
      // They wriggle free.
      release(sys, self);
      self.resource = 0;
      return;
    }
    self.resource = 255;
    // Carry the foe overhead, a little in front, facing the cursor.
    float dx = self.aimX - self.x, dy = self.aimY - self.y;
    float d = std::sqrt(dx * dx + dy * dy);
    float fx = d > 1.0f ? dx / d : 0.0f;
    float fy = d > 1.0f ? dy / d : 1.0f;
    float reach = foe->heldLow ? 26.0f : 14.0f;
    foe->x = self.x + fx * reach;
    foe->y = self.y + fy * reach + 2.0f;
    foe->vx = foe->vy = 0.0f;
    foe->knockMs = 0.0f;
  }

  float speedMultiplier(const Player& self) const override {
    return self.kitTarget != 0 ? 0.85f : 1.0f;
  }

  void onRoundStart(Player& self) override {
    self.kitTarget = 0;
    self.kitTimer[0] = self.kitTimer[1] = 0.0f;
  }
};

// ===========================================================================
// Nyx, the Hive Mother (Playmaker)
// A woman made of nanites who carries an old CRT on her shoulder. She sends
// her swarm out to ride a foe (Silenced: +1 Dulled, +1 Weakened a second) or
// an ally (Focused: +1 Sharpened a second), and can become a swarm herself:
// she sets the CRT down, rides a target as a cloud, then streams back to the
// CRT and lifts it onto her shoulder again.
//
// Nyx needs more state than the kit scratch slots hold, so it lives here,
// keyed by player id. What clients draw is mirrored into Player::kitX/kitY
// (the swarm), propX/propY (the CRT) and kitMode (Arena::NyxMode).
//
// Not built yet: the Miles clauses of Plague Bringer and Nanite Cloud (Miles
// has no kit, so nobody can possess a target).
// ===========================================================================
class NyxKit : public Kit {
 public:
  // The swarm (LMB, passive)
  static constexpr float SWARM_REACH = 560.0f;   // how far LMB sends it
  static constexpr float SWARM_SPEED = 900.0f;   // px/s
  static constexpr float SWARM_RADIUS = 60.0f;   // enemies inside gain Slow
  static constexpr float SWARM_PICK = 70.0f;     // cursor to body, to attach
  static constexpr float SWARM_LEASH = 760.0f;   // farther from Nyx: it returns
  static constexpr float PULSE_MS = 1000.0f;     // stacks once a second
  static constexpr float HOLD_MS = 1400.0f;      // Silenced/Focused while held
  static constexpr float HOME_DIST = 24.0f;      // closer counts as with Nyx
  // Plague Bringer (RMB)
  static constexpr float PLAGUE_RADIUS_MIN = 150.0f;
  static constexpr float PLAGUE_RADIUS_MAX = 260.0f;
  static constexpr int CONTAMINATE_COUNT = 3;
  // Become the Swarm (Shift)
  static constexpr float FORM_REACH = 460.0f;
  static constexpr float DROP_MS = 280.0f;       // setting the CRT down
  static constexpr float CLOUD_SPEED = 1500.0f;  // px/s as a cloud
  static constexpr float REFORM_MS = 450.0f;     // lifting the CRT back up
  static constexpr float SHARE = 0.5f;           // of damage to her ride

  struct State {
    // The swarm
    uint8_t swarm = Arena::NyxMode::SwarmHome;
    float sx = 0.0f, sy = 0.0f;        // where it is
    float destX = 0.0f, destY = 0.0f;  // where it is flying (a point)
    uint32_t swarmTarget = 0;          // who it flies to or rides
    bool swarmOnEnemy = false;
    float swarmPulse = 0.0f;
    std::vector<uint32_t> inside;  // enemies inside it now (Slow on entry)
    // Nyx herself
    uint8_t self = Arena::NyxMode::Body;
    uint32_t rideTarget = 0;
    bool rideOnEnemy = false;
    float ridePulse = 0.0f;
    float timerMs = 0.0f;  // Dropping / Reforming
    float crtX = 0.0f, crtY = 0.0f;
  };

  void cast(AbilityContext& ctx, int slot) override {
    switch (slot) {
      case 0:
        sendSwarm(ctx);
        break;
      case 1:
        plagueBringer(ctx);
        break;
      case 2:
        naniteCloud(ctx);
        break;
      case 3:
        becomeSwarm(ctx);
        break;
      default:
        assert(false && "Nyx has four kit slots");
    }
  }

  bool canCast(AbilitySystem& sys, const Player& self, int slot) override {
    const State* s = find(self.id);
    uint8_t form = s ? s->self : Arena::NyxMode::Body;
    // Mid-transformation she is neither a body nor a swarm: no casting.
    if (form == Arena::NyxMode::Dropping || form == Arena::NyxMode::Homing ||
        form == Arena::NyxMode::Reforming) {
      return false;
    }
    if (slot == 2) {
      if (s && (riding(*s) || swarmAttached(*s))) return true;
      sys.fxRing(self.x, self.y, 60.0f, VfxTheme::Neutral);
      return false;
    }
    if (slot == 3) {
      if (form != Arena::NyxMode::Body) return true;  // detach
      bool enemy = false;
      if (pickTarget(sys, self, self.aimX, self.aimY, FORM_REACH, enemy)) {
        return true;
      }
      sys.fxRing(self.x, self.y, FORM_REACH, VfxTheme::Neutral);
      return false;
    }
    return true;
  }

  bool canRecast(const Player& self, int slot) const override {
    if (slot != 3) return false;
    const State* s = find(self.id);
    return s && (s->self == Arena::NyxMode::Flying || riding(*s));
  }

  void tick(AbilitySystem& sys, Player& self, float dtMs) override {
    State& s = state(self);
    tickSelf(sys, self, s, dtMs);
    tickSwarm(sys, self, s, dtMs);
    mirror(self, s);
  }

  // Riding a target, she cannot be hit herself; she takes her share of what
  // hits the target instead (onCreatureDamaged).
  float onTakeDamage(AbilitySystem&, Player& self, float damage,
                     uint32_t) override {
    const State* s = find(self.id);
    if (s && riding(*s) && !takingShare) return 0.0f;
    return damage;
  }

  float onCreatureDamaged(AbilitySystem& sys, Player& self, bool isEnemy,
                          uint32_t targetId, float damage) override {
    const State* s = find(self.id);
    if (!s || !riding(*s) || s->rideTarget != targetId ||
        s->rideOnEnemy != isEnemy) {
      return damage;
    }
    float share = damage * SHARE;
    takingShare = true;
    sys.damagePlayer(self, share, 0, DamageFlag::Proc);
    takingShare = false;
    // An ally is spared her share; a foe still takes all of it.
    return isEnemy ? damage : damage - share;
  }

  // She falls beside her CRT, whatever she was riding.
  bool onLethal(AbilitySystem&, Player& self) override {
    State& s = state(self);
    if (s.self != Arena::NyxMode::Body) {
      self.x = s.crtX;
      self.y = s.crtY;
    }
    s.self = Arena::NyxMode::Body;
    s.rideTarget = 0;
    s.swarm = Arena::NyxMode::SwarmHome;
    s.swarmTarget = 0;
    self.kitAnchored = false;
    mirror(self, s);
    return false;
  }

  // All abilities radiate from Nyx and from her swarm, core ones included:
  // an area core goes off again where the swarm is.
  void onCoreCast(AbilityContext& ctx) override {
    Player& self = ctx.self;
    const State* s = find(self.id);
    if (!s || !swarmAway(*s, self)) return;
    // Diamond Fortress is armour on Nyx, not an area: nothing to echo.
    if (self.coreGenus == Arena::Genus::Frigidariant) return;
    float x = self.x, y = self.y;
    self.x = s->sx;
    self.y = s->sy;
    Infusions::castCore(ctx);
    self.x = x;
    self.y = y;
  }

  void onRoundStart(Player& self) override { states.erase(self.id); }

 private:
  std::unordered_map<uint32_t, State> states;
  bool takingShare = false;

  const State* find(uint32_t id) const {
    auto it = states.find(id);
    return it == states.end() ? nullptr : &it->second;
  }

  State& state(Player& self) {
    auto it = states.find(self.id);
    if (it == states.end()) {
      State fresh;
      fresh.sx = self.x;
      fresh.sy = self.y;
      it = states.emplace(self.id, fresh).first;
    }
    return it->second;
  }

  static bool riding(const State& s) {
    return s.self == Arena::NyxMode::OnEnemy || s.self == Arena::NyxMode::OnAlly;
  }
  static bool swarmAttached(const State& s) {
    return s.swarm == Arena::NyxMode::SwarmOnEnemy ||
           s.swarm == Arena::NyxMode::SwarmOnAlly;
  }
  static bool swarmAway(const State& s, const Player& self) {
    return s.swarm != Arena::NyxMode::SwarmHome &&
           dist(s.sx, s.sy, self.x, self.y) > HOME_DIST;
  }

  // The enemy or ally (not Nyx) whose body is nearest (x, y), within
  // SWARM_PICK of it and `reach` of Nyx. Returns its id; 0 for none.
  static uint32_t pickTarget(AbilitySystem& sys, const Player& self, float x,
                             float y, float reach, bool& isEnemy) {
    uint32_t best = 0;
    float bestD = 1e9f;
    sys.enemiesInCircle(x, y, SWARM_PICK + 80.0f, [&](Enemy& enemy) {
      float d = dist(enemy.x, enemy.y, x, y) - enemy.radius;
      if (d > SWARM_PICK || d >= bestD) return;
      if (dist(enemy.x, enemy.y, self.x, self.y) > reach + enemy.radius) return;
      best = enemy.id;
      bestD = d;
      isEnemy = true;
    });
    for (auto& [id, ally] : sys.allPlayers()) {
      if (id == self.id || !ally.isAlive() || ally.characterId == 0) continue;
      float d = dist(ally.x, ally.y, x, y) - Arena::Tuning::PLAYER_RADIUS;
      if (d > SWARM_PICK || d >= bestD) continue;
      if (dist(ally.x, ally.y, self.x, self.y) > reach) continue;
      best = id;
      bestD = d;
      isEnemy = false;
    }
    return best;
  }

  // Where a creature stands; false if it is gone or dead.
  static bool bodyAt(AbilitySystem& sys, uint32_t id, bool isEnemy, float& x,
                     float& y) {
    if (isEnemy) {
      Enemy* enemy = sys.findEnemy(id);
      if (!enemy || enemy->state == EnemyState::Dead) return false;
      x = enemy->x;
      y = enemy->y;
      return true;
    }
    Player* ally = sys.findPlayer(id);
    if (!ally || !ally->isAlive()) return false;
    x = ally->x;
    y = ally->y;
    return true;
  }

  // Step (x, y) toward (tx, ty); true on arrival.
  static bool moveToward(float& x, float& y, float tx, float ty, float speed,
                         float dtMs) {
    float d = dist(x, y, tx, ty);
    float step = speed * dtMs / 1000.0f;
    if (d <= step) {
      x = tx;
      y = ty;
      return true;
    }
    x += (tx - x) / d * step;
    y += (ty - y) / d * step;
    return false;
  }

  // Hive Mother on one host. Once a second: Silenced (enemy) or Focused
  // (ally), whose own secondaries add the Dulled and Weakened or the
  // Sharpened. False once the host has been cleansed or purged of it,
  // which sends the swarm home.
  static bool holdHost(AbilitySystem& sys, uint32_t nyxId, uint32_t hostId,
                       bool isEnemy, float& pulse, float dtMs) {
    EffectType hold = isEnemy ? EffectType::Silenced : EffectType::Focused;
    pulse += dtMs;
    if (pulse >= PULSE_MS) {
      pulse -= PULSE_MS;
      ActiveEffects& fx = sys.effects().mutableEffects(hostId, isEnemy);
      const EffectInstance* had = fx.findEffect(hold);
      float before = had ? had->remainingDuration : 0.0f;
      if (isEnemy) {
        if (Enemy* enemy = sys.findEnemy(hostId)) {
          sys.applyToEnemy(*enemy, hold, 1, nyxId, HOLD_MS);
        }
      } else if (Player* ally = sys.findPlayer(hostId)) {
        sys.applyToPlayer(*ally, hold, 1, nyxId, HOLD_MS);
      }
      // Silenced and Focused extend on reapply; the swarm only holds them,
      // it does not bank time.
      ActiveEffects& after = sys.effects().mutableEffects(hostId, isEnemy);
      if (EffectInstance* held = after.findEffect(hold)) {
        held->remainingDuration = std::max(before, HOLD_MS);
      }
    }
    return sys.effects().mutableEffects(hostId, isEnemy).hasEffect(hold);
  }

  // ---- Nyx herself ---------------------------------------------------------

  void tickSelf(AbilitySystem& sys, Player& self, State& s, float dtMs) {
    using namespace Arena::NyxMode;
    switch (s.self) {
      case Body:
        break;
      case Dropping:
        s.timerMs -= dtMs;
        if (s.timerMs <= 0.0f) {
          s.self = Flying;
          sys.fxBurst(self.x, self.y, 70.0f, VfxTheme::Nanite);
        }
        break;
      case Flying: {
        float tx, ty;
        if (!bodyAt(sys, s.rideTarget, s.rideOnEnemy, tx, ty)) {
          startHoming(s);
          break;
        }
        if (moveToward(self.x, self.y, tx, ty, CLOUD_SPEED, dtMs)) {
          s.self = s.rideOnEnemy ? OnEnemy : OnAlly;
          s.ridePulse = PULSE_MS;  // takes hold at once
        }
        break;
      }
      case OnEnemy:
      case OnAlly: {
        float tx, ty;
        if (!bodyAt(sys, s.rideTarget, s.rideOnEnemy, tx, ty) ||
            !holdHost(sys, self.id, s.rideTarget, s.rideOnEnemy, s.ridePulse,
                      dtMs)) {
          startHoming(s);
          break;
        }
        self.x = tx;
        self.y = ty;
        break;
      }
      case Homing:
        if (moveToward(self.x, self.y, s.crtX, s.crtY, CLOUD_SPEED, dtMs)) {
          s.self = Reforming;
          s.timerMs = REFORM_MS;
          sys.fxBurst(self.x, self.y, 60.0f, VfxTheme::Nanite);
        }
        break;
      case Reforming:
        s.timerMs -= dtMs;
        if (s.timerMs <= 0.0f) s.self = Body;
        break;
      default:
        assert(false && "unknown Nyx form");
    }

    self.kitAnchored = s.self != Body;
    if (s.self != Body) self.vx = self.vy = 0.0f;
    if (s.self == Dropping || s.self == Homing || s.self == Reforming) {
      self.chargeSlot = -1;  // a held Plague Bringer is lost
    }
    // A cloud is not a target. In transit she cannot be touched at all;
    // riding, she takes her share of the host's damage.
    bool cloud = s.self == Flying || s.self == Homing || riding(s);
    if (cloud) self.hiddenMs = std::max(self.hiddenMs, 100.0f);
    if (s.self == Flying || s.self == Homing) {
      self.intangibleMs = std::max(self.intangibleMs, 100.0f);
    }
  }

  static void startHoming(State& s) {
    s.self = Arena::NyxMode::Homing;
    s.rideTarget = 0;
  }

  void becomeSwarm(AbilityContext& ctx) {
    using namespace Arena::NyxMode;
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    State& s = state(self);
    if (s.self == Flying || riding(s)) {
      startHoming(s);
      sys.fxBurst(self.x, self.y, 50.0f, VfxTheme::Nanite);
      return;
    }
    assert(s.self == Body && "canCast refuses mid-transformation");
    bool enemy = false;
    uint32_t target = pickTarget(sys, self, ctx.aimX, ctx.aimY, FORM_REACH, enemy);
    assert(target != 0 && "canCast checked for a target");
    s.self = Dropping;
    s.timerMs = DROP_MS;
    s.rideTarget = target;
    s.rideOnEnemy = enemy;
    s.crtX = self.x;
    s.crtY = self.y;
    self.kitAnchored = true;
    float tx = 0.0f, ty = 0.0f;
    if (bodyAt(sys, target, enemy, tx, ty)) {
      sys.fxTether(self.x, self.y, tx, ty, DROP_MS + 200.0f, VfxTheme::Nanite);
    }
  }

  // ---- the swarm -----------------------------------------------------------

  void tickSwarm(AbilitySystem& sys, Player& self, State& s, float dtMs) {
    using namespace Arena::NyxMode;
    // Too far from Nyx (even as a cloud, she is its anchor): it comes home.
    if (s.swarm != SwarmHome && s.swarm != SwarmReturning &&
        dist(s.sx, s.sy, self.x, self.y) > SWARM_LEASH) {
      s.swarm = SwarmReturning;
      s.swarmTarget = 0;
    }
    switch (s.swarm) {
      case SwarmHome:
        s.sx = self.x;
        s.sy = self.y;
        break;
      case SwarmFlying: {
        float tx = s.destX, ty = s.destY;
        if (s.swarmTarget != 0 &&
            !bodyAt(sys, s.swarmTarget, s.swarmOnEnemy, tx, ty)) {
          // The host is gone: hang where the swarm got to.
          s.swarmTarget = 0;
          s.swarm = SwarmParked;
          break;
        }
        if (moveToward(s.sx, s.sy, tx, ty, SWARM_SPEED, dtMs)) {
          if (s.swarmTarget != 0) {
            s.swarm = s.swarmOnEnemy ? SwarmOnEnemy : SwarmOnAlly;
            s.swarmPulse = PULSE_MS;  // takes hold at once
          } else {
            s.swarm = SwarmParked;
          }
        }
        break;
      }
      case SwarmParked:
        break;
      case SwarmOnEnemy:
      case SwarmOnAlly: {
        float tx, ty;
        if (!bodyAt(sys, s.swarmTarget, s.swarmOnEnemy, tx, ty) ||
            !holdHost(sys, self.id, s.swarmTarget, s.swarmOnEnemy,
                      s.swarmPulse, dtMs)) {
          s.swarm = SwarmReturning;
          s.swarmTarget = 0;
          break;
        }
        s.sx = tx;
        s.sy = ty;
        break;
      }
      case SwarmReturning:
        if (moveToward(s.sx, s.sy, self.x, self.y, SWARM_SPEED * 1.3f, dtMs)) {
          s.swarm = SwarmHome;
        }
        break;
      default:
        assert(false && "unknown swarm mode");
    }

    // Enemies passing through the swarm gain +1 Slow, once per pass.
    std::vector<uint32_t> nowInside;
    if (s.swarm != SwarmHome) {
      sys.enemiesInCircle(s.sx, s.sy, SWARM_RADIUS, [&](Enemy& enemy) {
        nowInside.push_back(enemy.id);
        if (std::find(s.inside.begin(), s.inside.end(), enemy.id) ==
            s.inside.end()) {
          sys.applyToEnemy(enemy, EffectType::Slow, 1, self.id);
        }
      });
    }
    s.inside = std::move(nowInside);
  }

  void sendSwarm(AbilityContext& ctx) {
    using namespace Arena::NyxMode;
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    State& s = state(self);
    bool enemy = false;
    uint32_t target =
        pickTarget(sys, self, ctx.aimX, ctx.aimY, SWARM_REACH, enemy);
    if (s.swarm == SwarmHome) {
      s.sx = self.x;
      s.sy = self.y;
    }
    s.swarm = SwarmFlying;
    s.swarmTarget = target;
    s.swarmOnEnemy = enemy;
    aimPoint(ctx, SWARM_REACH, s.destX, s.destY);
  }

  // ---- RMB and Q -----------------------------------------------------------

  // Every enemy and ally within `radius` of Nyx or of her swarm, once each.
  void inWave(AbilitySystem& sys, const Player& self, const State& s,
              float radius, std::vector<uint32_t>& foes,
              std::vector<uint32_t>& friends) {
    auto add = [](std::vector<uint32_t>& v, uint32_t id) {
      if (std::find(v.begin(), v.end(), id) == v.end()) v.push_back(id);
    };
    std::vector<std::pair<float, float>> origins = {{self.x, self.y}};
    if (swarmAway(s, self)) origins.push_back({s.sx, s.sy});
    for (const auto& [x, y] : origins) {
      sys.fxRing(x, y, radius, VfxTheme::Nanite);
      sys.fxBurst(x, y, radius * 0.45f, VfxTheme::Nanite);
      sys.enemiesInCircle(x, y, radius, [&](Enemy& e) { add(foes, e.id); });
      sys.playersInCircle(x, y, radius, [&](Player& p) { add(friends, p.id); });
    }
  }

  void plagueBringer(AbilityContext& ctx) {
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    State& s = state(self);
    float radius = PLAGUE_RADIUS_MIN +
                   (PLAGUE_RADIUS_MAX - PLAGUE_RADIUS_MIN) * ctx.charge;
    std::vector<uint32_t> foes, friends;
    inWave(sys, self, s, radius, foes, friends);

    for (uint32_t id : foes) {
      if (Enemy* enemy = sys.findEnemy(id)) {
        sys.applyToEnemy(*enemy, EffectType::Weakened, 2, self.id);
      }
    }
    for (uint32_t id : friends) {
      if (Player* ally = sys.findPlayer(id)) {
        sys.applyToPlayer(*ally, EffectType::Empowered, 1, self.id);
      }
    }

    // Contaminate: the swarms' hosts pass their worst debuffs to every
    // enemy the wave hit. A foe keeps its own; an ally is rid of them.
    if (swarmAttached(s)) {
      contaminate(sys, self, s.swarmTarget, s.swarmOnEnemy, foes);
    }
    if (riding(s) && !(swarmAttached(s) && s.swarmTarget == s.rideTarget &&
                       s.swarmOnEnemy == s.rideOnEnemy)) {
      contaminate(sys, self, s.rideTarget, s.rideOnEnemy, foes);
    }
  }

  static void contaminate(AbilitySystem& sys, const Player& self,
                          uint32_t hostId, bool hostIsEnemy,
                          const std::vector<uint32_t>& foes) {
    float hx, hy;
    if (!bodyAt(sys, hostId, hostIsEnemy, hx, hy)) return;
    std::vector<EffectInstance> debuffs;
    for (const EffectInstance& e :
         sys.effects().mutableEffects(hostId, hostIsEnemy).effects) {
      if (EffectRegistry::getCategory(e.type) == EffectCategory::Debuff) {
        debuffs.push_back(e);
      }
    }
    std::stable_sort(debuffs.begin(), debuffs.end(),
                     [](const EffectInstance& a, const EffectInstance& b) {
                       return a.stacks > b.stacks;
                     });
    if (debuffs.size() > CONTAMINATE_COUNT) debuffs.resize(CONTAMINATE_COUNT);

    for (const EffectInstance& debuff : debuffs) {
      for (uint32_t id : foes) {
        if (hostIsEnemy && id == hostId) continue;
        Enemy* enemy = sys.findEnemy(id);
        if (!enemy) continue;
        int before = sys.effects().getEnemyEffects(id).getStacks(debuff.type);
        sys.applyToEnemy(*enemy, debuff.type, debuff.stacks, self.id);
        int after = sys.effects().getEnemyEffects(id).getStacks(debuff.type);
        if (after != before) {
          sys.fxEffectVerb(EffectVerb::Spread, debuff.type, id, true, enemy->x,
                           enemy->y, static_cast<float>(before),
                           static_cast<float>(after), hostId, hx, hy);
        }
      }
      if (!hostIsEnemy) sys.effects().removeEffect(hostId, false, debuff.type);
    }
  }

  void naniteCloud(AbilityContext& ctx) {
    AbilitySystem& sys = ctx.sys;
    Player& self = ctx.self;
    State& s = state(self);
    auto cloud = [&](uint32_t id, bool isEnemy) {
      if (isEnemy) {
        if (Enemy* enemy = sys.findEnemy(id)) {
          sys.applyToEnemy(*enemy, EffectType::Marked, 1, self.id);
          sys.fxBurst(enemy->x, enemy->y, 55.0f, VfxTheme::Nanite);
        }
      } else if (Player* ally = sys.findPlayer(id)) {
        sys.applyToPlayer(*ally, EffectType::Stealth, 1, self.id);
        sys.fxBurst(ally->x, ally->y, 55.0f, VfxTheme::Nanite);
      }
    };
    if (swarmAttached(s)) cloud(s.swarmTarget, s.swarmOnEnemy);
    if (riding(s) && (s.rideTarget != s.swarmTarget || !swarmAttached(s))) {
      cloud(s.rideTarget, s.rideOnEnemy);
    }
  }

  static void mirror(Player& self, const State& s) {
    self.kitX = s.sx;
    self.kitY = s.sy;
    bool crtDown = s.self != Arena::NyxMode::Body;
    self.propX = crtDown ? s.crtX : 0.0f;
    self.propY = crtDown ? s.crtY : 0.0f;
    self.kitMode = Arena::NyxMode::pack(s.swarm, s.self);
    // The HUD meter: full while a swarm (or Nyx) rides someone.
    self.resource = swarmAttached(s) || riding(s) ? 255 : 0;
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
    case Nolan:
      return std::make_unique<NolanKit>();
    case Nyx:
      return std::make_unique<NyxKit>();
    case Wade:
      return std::make_unique<WadeKit>();
    case Presidente:
      return std::make_unique<PresidenteKit>();
    default:
      return nullptr;
  }
}
