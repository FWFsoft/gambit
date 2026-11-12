#include "Infusions.h"

#include <algorithm>
#include <cmath>

#include "EffectManager.h"
#include "EnemySystem.h"

using Arena::Genus;
using Arena::InfusionTree;

// Player::infusionTimer / infusionCounter are indexed by tree:
//   [0] Offense   timer: internal cooldown for on-hit area effects
//   [1] Defense   timer: Mind Barrier "time since hit" / Slow Burn pool
//   [2] Mobility  timer: trail drop timer / Burning Rush time left
//                 counter: Speed Siphon enemy count / Burning Rush kills
//   [3] Genealogy timer: Cataclysm internal cooldown

namespace {

constexpr int OFFENSE = static_cast<int>(InfusionTree::Offense);
constexpr int DEFENSE = static_cast<int>(InfusionTree::Defense);
constexpr int MOBILITY = static_cast<int>(InfusionTree::Mobility);
constexpr int GENEALOGY = static_cast<int>(InfusionTree::Genealogy);

constexpr float FORTRESS_DURATION_MS = 4500.0f;
constexpr float FORTRESS_DAMAGE_TAKEN = 0.3f;
constexpr int BURNING_RUSH_KILLS = 12;

void spawnMoltenPatch(AbilitySystem& sys, uint32_t ownerId, float x, float y,
                      float radius, float lifeMs, float damagePerTick) {
  AbilitySystem::Zone patch;
  patch.ownerId = ownerId;
  patch.x = x;
  patch.y = y;
  patch.radius = radius;
  patch.lifeMs = lifeMs;
  patch.tickMs = 500.0f;
  patch.theme = VfxTheme::Fire;
  patch.variant = 1;
  AbilitySystem* s = &sys;
  patch.onEnemyTick = [s, ownerId, damagePerTick](AbilitySystem::Zone&,
                                                  Enemy& enemy) {
    s->damageEnemy(ownerId, enemy, damagePerTick,
                   DamageFlag::Dot | DamageFlag::Proc);
  };
  sys.spawnZone(patch);
}

// Slow -> Snare -> Stun escalation shared by Icy Strikes and Arctic Aura.
void frostEscalate(AbilitySystem& sys, Player& player, Enemy& enemy,
                   int level) {
  const ActiveEffects& fx = sys.effects().getEnemyEffects(enemy.id);
  float scale = 1.0f + 0.25f * (level - 1);
  if (fx.hasEffect(EffectType::Snared)) {
    sys.applyToEnemy(enemy, EffectType::Stunned, 1, player.id, 800.0f * scale);
    sys.effects().removeEffect(enemy.id, true, EffectType::Snared);
  } else if (fx.hasEffect(EffectType::Slow)) {
    sys.applyToEnemy(enemy, EffectType::Snared, 1, player.id, 1200.0f * scale);
  } else {
    sys.applyToEnemy(enemy, EffectType::Slow, 1, player.id);
  }
}

}  // namespace

namespace Infusions {

bool slotInfusion(Player& player, Genus genus, int slot) {
  int g = static_cast<int>(genus);
  if (g < 1 || g > Arena::GENUS_COUNT) return false;
  if (player.infusionInventory[g - 1] == 0) return false;

  if (slot == 0) {
    // The first infusion must go in the core, and the core is permanent.
    if (player.coreGenus != Genus::None) return false;
    player.coreGenus = genus;
    player.infusionInventory[g - 1]--;
    return true;
  }

  if (player.coreGenus == Genus::None) return false;
  int tree = slot - 1;
  if (tree < 0 || tree >= Arena::TREE_COUNT) return false;

  if (player.treeGenus[tree] == genus) {
    if (player.treeLevel[tree] >= Arena::MAX_TREE_LEVEL) return false;
    player.treeLevel[tree]++;
  } else {
    // A different genus overwrites whatever was slotted in this tree.
    player.treeGenus[tree] = genus;
    player.treeLevel[tree] = 1;
    player.infusionTimer[tree] = 0.0f;
    player.infusionCounter[tree] = 0;
  }
  player.infusionInventory[g - 1]--;
  return true;
}

bool canRecastCore(const Player& player) {
  // Diamond Fortress can be shattered early.
  return player.coreGenus == Genus::Frigidariant && player.coreTimerMs > 0.0f;
}

void castCore(AbilityContext& ctx) {
  AbilitySystem& sys = ctx.sys;
  Player& self = ctx.self;
  uint32_t selfId = self.id;
  AbilitySystem* s = &sys;

  switch (self.coreGenus) {
    case Genus::Venoxarid: {
      // Toxic Miasma: a lingering cloud centred on the player.
      int venom = self.infusionLevel(Genus::Venoxarid, InfusionTree::Genealogy);
      AbilitySystem::Zone cloud;
      cloud.ownerId = selfId;
      cloud.x = self.x;
      cloud.y = self.y;
      cloud.radius = 150.0f + 20.0f * venom;
      cloud.lifeMs = 6000.0f;
      cloud.tickMs = 500.0f;
      cloud.theme = VfxTheme::Poison;
      cloud.variant = 0;
      cloud.onEnemyTick = [s, selfId](AbilitySystem::Zone&, Enemy& enemy) {
        s->applyToEnemy(enemy, EffectType::Poison, 1, selfId);
        s->damageEnemy(selfId, enemy, 3.0f, DamageFlag::Core);
      };
      sys.spawnZone(cloud);
      sys.fxRing(self.x, self.y, cloud.radius, VfxTheme::Poison);
      break;
    }
    case Genus::Cerebrian: {
      // Mind Flay: psychic wave that turns enemies on each other.
      const float radius = 270.0f;
      sys.fxRing(self.x, self.y, radius, VfxTheme::Psychic);
      sys.fxBurst(self.x, self.y, radius * 0.6f, VfxTheme::Psychic);
      sys.enemiesInCircle(self.x, self.y, radius, [&](Enemy& enemy) {
        sys.applyToEnemy(enemy, EffectType::Confused, 1, selfId, 3500.0f);
        sys.damageEnemy(selfId, enemy, 22.0f, DamageFlag::Core);
      });
      break;
    }
    case Genus::Frigidariant: {
      if (self.coreTimerMs > 0.0f) {
        // Shatter the armor.
        self.coreTimerMs = 0.0f;
        const float radius = 190.0f;
        sys.fxBurst(self.x, self.y, radius, VfxTheme::Ice);
        sys.fxRing(self.x, self.y, radius + 30.0f, VfxTheme::Ice);
        sys.enemiesInCircle(self.x, self.y, radius, [&](Enemy& enemy) {
          sys.applyToEnemy(enemy, EffectType::Slow, 2, selfId);
          sys.damageEnemy(selfId, enemy, 75.0f,
                          DamageFlag::Core | DamageFlag::Explosion);
        });
      } else {
        self.coreTimerMs = FORTRESS_DURATION_MS;
        GameEventPacket bubble;
        bubble.kind = GameEventKind::ShieldBubble;
        bubble.sourceId = selfId;
        bubble.x = self.x;
        bubble.y = self.y;
        bubble.value2 = FORTRESS_DURATION_MS;
        bubble.param2 = static_cast<uint8_t>(VfxTheme::Ice);
        sys.emit(bubble);
        sys.applyToPlayer(self, EffectType::Unbounded, 1, selfId,
                          FORTRESS_DURATION_MS);
      }
      break;
    }
    case Genus::Magnarok: {
      // Cauterize: detonate every over-time effect nearby.
      const float reach = 420.0f;
      sys.fxRing(self.x, self.y, reach, VfxTheme::Fire);
      struct Blast {
        float x, y, radius, damage;
      };
      std::vector<Blast> blasts;
      sys.enemiesInCircle(self.x, self.y, reach, [&](Enemy& enemy) {
        ActiveEffects& fx = sys.effects().mutableEffects(enemy.id, true);
        // Bleeding and venom both go up.
        int stacks = 0;
        float remainingMs = 0.0f;
        for (EffectType dot : {EffectType::Wound, EffectType::Poison}) {
          if (const EffectInstance* e = fx.findEffect(dot)) {
            stacks += e->stacks;
            remainingMs = std::max(remainingMs, e->remainingDuration);
          }
        }
        if (stacks == 0) return;
        // More stacks and more time left make a bigger blast.
        float remaining = std::max(1.0f, remainingMs / 1000.0f);
        float damage = stacks * 5.0f * std::min(remaining, 6.0f) * 0.6f;
        blasts.push_back({enemy.x, enemy.y,
                          55.0f + std::min(stacks, 30) * 9.0f,
                          std::max(12.0f, damage)});
        fx.removeEffect(EffectType::Wound);
        fx.removeEffect(EffectType::Poison);
      });
      // The player's own over-time effects go up too.
      ActiveEffects& mine = sys.effects().mutableEffects(selfId, false);
      int ownStacks = mine.getStacks(EffectType::Wound) +
                      mine.getStacks(EffectType::Mend) +
                      mine.getStacks(EffectType::Poison);
      if (ownStacks > 0) {
        mine.removeEffect(EffectType::Wound);
        mine.removeEffect(EffectType::Mend);
        mine.removeEffect(EffectType::Poison);
        blasts.push_back(
            {self.x, self.y, 100.0f + ownStacks * 10.0f, ownStacks * 16.0f});
      }
      for (const Blast& blast : blasts) {
        sys.fxBurst(blast.x, blast.y, blast.radius, VfxTheme::Fire);
        sys.enemiesInCircle(blast.x, blast.y, blast.radius, [&](Enemy& enemy) {
          sys.damageEnemy(selfId, enemy, blast.damage,
                          DamageFlag::Core | DamageFlag::Explosion);
        });
      }
      break;
    }
    default:
      break;
  }
}

void onHitEnemy(AbilitySystem& sys, Player& player, Enemy& enemy, float damage,
                uint32_t flags) {
  (void)damage;
  uint32_t playerId = player.id;
  uint32_t enemyId = enemy.id;
  float ex = enemy.x, ey = enemy.y;

  // ---- Genealogy: core ability damage carries the genus --------------------
  if (flags & DamageFlag::Core) {
    Genus genus = player.treeGenus[GENEALOGY];
    int level = player.treeLevel[GENEALOGY];
    if (level > 0) {
      switch (genus) {
        case Genus::Venoxarid:
          sys.applyToEnemy(enemy, EffectType::Poison, level, playerId);
          break;
        case Genus::Cerebrian:
          sys.applyToEnemy(enemy, EffectType::Confused, 1, playerId,
                           1200.0f + 600.0f * level);
          break;
        case Genus::Frigidariant:
          // Unbreakable triggers from hard CC (onHardCrowdControl).
          break;
        case Genus::Magnarok:
          if (player.infusionTimer[GENEALOGY] <= 0.0f) {
            player.infusionTimer[GENEALOGY] = 350.0f;
            spawnMoltenPatch(sys, playerId, ex, ey, 60.0f, 3000.0f,
                             4.0f * level);
          }
          break;
        default:
          break;
      }
    }
    return;  // core damage does not trigger Offense effects
  }

  // ---- Offense ------------------------------------------------------------
  Genus genus = player.treeGenus[OFFENSE];
  int level = player.treeLevel[OFFENSE];
  if (level <= 0) return;

  switch (genus) {
    case Genus::Venoxarid: {
      // Toxic Blade: venom splashes to nearby enemies.
      sys.applyToEnemy(enemy, EffectType::Poison, level, playerId);
      if (player.infusionTimer[OFFENSE] <= 0.0f) {
        player.infusionTimer[OFFENSE] = 200.0f;
        sys.enemiesInCircle(ex, ey, 75.0f, [&](Enemy& other) {
          if (other.id != enemyId) {
            sys.applyToEnemy(other, EffectType::Poison, 1, playerId);
          }
        });
      }
      break;
    }
    case Genus::Cerebrian: {
      // Psychic Shockwave: small burst around the target.
      if (player.infusionTimer[OFFENSE] <= 0.0f) {
        player.infusionTimer[OFFENSE] = 180.0f;
        sys.fxRing(ex, ey, 80.0f, VfxTheme::Psychic);
        sys.enemiesInCircle(ex, ey, 80.0f, [&](Enemy& other) {
          sys.damageEnemy(playerId, other, 6.0f * level, DamageFlag::Proc);
        });
      }
      break;
    }
    case Genus::Frigidariant:
      frostEscalate(sys, player, enemy, level);
      break;
    case Genus::Magnarok: {
      // Eruptive Strikes: ignite. Burn time grows with each infusion.
      sys.applyToEnemy(enemy, EffectType::Wound, 1, playerId,
                       4000.0f + 2000.0f * level);
      sys.damageEnemy(playerId, enemy, 3.0f * level, DamageFlag::Proc);
      break;
    }
    default:
      break;
  }
}

float onPlayerDamaged(AbilitySystem& sys, Player& player, float damage,
                      Enemy* source, uint32_t flags) {
  // Diamond Fortress
  if (player.coreGenus == Genus::Frigidariant && player.coreTimerMs > 0.0f) {
    damage *= FORTRESS_DAMAGE_TAKEN;
  }

  Genus genus = player.treeGenus[DEFENSE];
  int level = player.treeLevel[DEFENSE];
  if (level <= 0) return damage;
  bool melee = (flags & DamageFlag::Melee) != 0 && source != nullptr;

  switch (genus) {
    case Genus::Venoxarid: {
      // Acidic Shell
      if (melee) {
        sys.applyToEnemy(*source, EffectType::Poison, level, player.id);
      }
      if (source && sys.effects().getEnemyEffects(source->id).hasEffect(
                        EffectType::Poison)) {
        damage *= std::max(0.4f, 1.0f - 0.12f * level);
      }
      break;
    }
    case Genus::Cerebrian:
      // Mind Barrier: any hit restarts the recharge delay.
      player.infusionTimer[DEFENSE] = 0.0f;
      break;
    case Genus::Frigidariant:
      // Arctic Aura
      if (melee) frostEscalate(sys, player, *source, level);
      break;
    case Genus::Magnarok: {
      // Slow Burn: part of the hit is deferred and dealt over time.
      if (!(flags & DamageFlag::Dot)) {
        float fraction = std::min(0.7f, 0.3f + 0.1f * level);
        player.infusionTimer[DEFENSE] += damage * fraction;
        damage *= (1.0f - fraction);
      }
      break;
    }
    default:
      break;
  }
  return damage;
}

void onHardCrowdControl(AbilitySystem& sys, Player& player, Enemy& enemy) {
  (void)enemy;
  int level = player.infusionLevel(Genus::Frigidariant, InfusionTree::Genealogy);
  if (level <= 0) return;
  // Unbreakable: at most one shield refresh per half second.
  if (player.infusionTimer[GENEALOGY] > 0.0f) return;
  player.infusionTimer[GENEALOGY] = 500.0f;
  sys.grantShield(player, 22.0f * level, 4000.0f);
}

void onKill(AbilitySystem& sys, Player& player, const Enemy& enemy) {
  (void)sys;
  (void)enemy;
  int level = player.infusionLevel(Genus::Magnarok, InfusionTree::Mobility);
  if (level <= 0) return;
  player.infusionCounter[MOBILITY]++;
  if (player.infusionCounter[MOBILITY] >= BURNING_RUSH_KILLS) {
    player.infusionCounter[MOBILITY] = 0;
    player.infusionTimer[MOBILITY] = 5000.0f * (1.0f + 0.3f * (level - 1));
    sys.fxRing(player.x, player.y, 90.0f, VfxTheme::Fire);
  }
}

void onDash(AbilitySystem& sys, Player& player, float x0, float y0, float x1,
            float y1) {
  int level = player.infusionLevel(Genus::Cerebrian, InfusionTree::Mobility);
  if (level <= 0) return;
  // Phase Shift: everything along the path takes psychic damage.
  uint32_t playerId = player.id;
  sys.fxBeam(x0, y0, x1, y1, 26.0f, 260.0f, VfxTheme::Psychic);
  sys.enemiesInLine(x0, y0, x1, y1, 30.0f, [&](Enemy& enemy) {
    sys.damageEnemy(playerId, enemy, 18.0f * level, DamageFlag::Proc);
  });
}

void tick(AbilitySystem& sys, Player& player, float dtMs) {
  uint32_t playerId = player.id;
  AbilitySystem* s = &sys;

  player.infusionTimer[OFFENSE] =
      std::max(0.0f, player.infusionTimer[OFFENSE] - dtMs);
  player.infusionTimer[GENEALOGY] =
      std::max(0.0f, player.infusionTimer[GENEALOGY] - dtMs);

  // Diamond Fortress wears off on its own with a harmless crack.
  if (player.coreTimerMs > 0.0f) {
    player.coreTimerMs -= dtMs;
    if (player.coreTimerMs <= 0.0f) {
      player.coreTimerMs = 0.0f;
      sys.fxRing(player.x, player.y, 70.0f, VfxTheme::Ice);
    }
  }

  // ---- Defense ------------------------------------------------------------
  {
    Genus genus = player.treeGenus[DEFENSE];
    int level = player.treeLevel[DEFENSE];
    if (level > 0 && genus == Genus::Cerebrian) {
      // Mind Barrier: recharges after six seconds without taking a hit.
      player.infusionTimer[DEFENSE] += dtMs;
      float barrier = 28.0f * level;
      if (player.infusionTimer[DEFENSE] >= 6000.0f && player.shield < barrier) {
        sys.grantShield(player, barrier, 600000.0f);
        player.infusionTimer[DEFENSE] = 0.0f;
      }
    } else if (level > 0 && genus == Genus::Magnarok) {
      // Slow Burn: drain the deferred damage. It can hurt but never kill.
      float pool = player.infusionTimer[DEFENSE];
      if (pool > 0.0f) {
        float drain = std::min(pool, (pool * 0.5f + 4.0f) * dtMs / 1000.0f);
        player.infusionTimer[DEFENSE] -= drain;
        player.health = std::max(1.0f, player.health - drain);
      }
    }
  }

  // ---- Mobility -----------------------------------------------------------
  {
    Genus genus = player.treeGenus[MOBILITY];
    int level = player.treeLevel[MOBILITY];
    if (level <= 0) return;
    bool moving = std::abs(player.vx) + std::abs(player.vy) > 1.0f;

    switch (genus) {
      case Genus::Venoxarid: {
        // Speed Siphon: recount poisoned enemies a few times per second.
        player.infusionTimer[MOBILITY] -= dtMs;
        if (player.infusionTimer[MOBILITY] <= 0.0f) {
          player.infusionTimer[MOBILITY] = 250.0f;
          int poisoned = sys.countEnemies(
              player.x, player.y, 420.0f, [&](const Enemy& enemy) {
                return sys.effects().getEnemyEffects(enemy.id).hasEffect(
                    EffectType::Poison);
              });
          player.infusionCounter[MOBILITY] = std::min(poisoned, 8);
        }
        break;
      }
      case Genus::Frigidariant: {
        // Glacial Path: drop ice behind the player while moving.
        player.infusionTimer[MOBILITY] -= dtMs;
        if (moving && player.infusionTimer[MOBILITY] <= 0.0f) {
          player.infusionTimer[MOBILITY] = 220.0f;
          AbilitySystem::Zone ice;
          ice.ownerId = playerId;
          ice.x = player.x;
          ice.y = player.y;
          ice.radius = 42.0f;
          ice.lifeMs = 2400.0f + 500.0f * level;
          ice.tickMs = 450.0f;
          ice.theme = VfxTheme::Ice;
          ice.variant = 1;
          ice.onEnemyTick = [s, playerId, level](AbilitySystem::Zone&,
                                                 Enemy& enemy) {
            s->applyToEnemy(enemy, EffectType::Slow, level, playerId);
          };
          sys.spawnZone(ice);
        }
        break;
      }
      case Genus::Magnarok: {
        // Burning Rush: timer > 0 means the rush is active. A second timer
        // is not available, so patches drop on a distance-free cadence
        // derived from the remaining time.
        if (player.infusionTimer[MOBILITY] > 0.0f) {
          float before = player.infusionTimer[MOBILITY];
          player.infusionTimer[MOBILITY] = std::max(0.0f, before - dtMs);
          bool crossed = static_cast<int>(before / 200.0f) !=
                         static_cast<int>(player.infusionTimer[MOBILITY] / 200.0f);
          if (moving && crossed) {
            spawnMoltenPatch(sys, playerId, player.x, player.y, 44.0f, 2500.0f,
                             5.0f * level);
          }
        }
        break;
      }
      default:
        break;
    }
  }
}

float speedMultiplier(const Player& player) {
  Genus genus = player.treeGenus[MOBILITY];
  int level = player.treeLevel[MOBILITY];
  if (level <= 0) return 1.0f;
  if (genus == Genus::Venoxarid) {
    return 1.0f + 0.04f * level * player.infusionCounter[MOBILITY];
  }
  if (genus == Genus::Magnarok && player.infusionTimer[MOBILITY] > 0.0f) {
    return 1.4f;
  }
  return 1.0f;
}

bool blocksCrowdControl(const Player& player) {
  if (player.coreGenus == Genus::Frigidariant && player.coreTimerMs > 0.0f) {
    return true;  // nothing moves a diamond
  }
  return player.infusionLevel(Genus::Cerebrian, InfusionTree::Defense) > 0 &&
         player.shield > 0.0f;
}

}  // namespace Infusions
