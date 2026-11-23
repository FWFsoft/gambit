#include "Infusions.h"

#include <algorithm>
#include <cmath>

#include "EffectManager.h"
#include "EnemySystem.h"

using Arena::Genus;
using Arena::InfusionPart;
using Arena::InfusionTree;
using Arena::Talent;

// Player::infusionTimer / infusionCounter are indexed [tree][genus], so two
// genera sharing a tree never share state. What each one holds:
//   Offense   Venoxarid/Cerebrian timer: internal cooldown of the splash
//   Defense   Cerebrian timer: Mind Barrier time since hit
//             Magnarok timer: Slow Burn damage still to drain
//   Mobility  Venoxarid timer/counter: Speed Siphon recount / enemy count
//             Frigidariant timer: Glacial Path drop timer
//             Magnarok timer/counter: Burning Rush time left / kills
//   Genealogy Magnarok timer: Cataclysm internal cooldown
//   Genealogy Frigidariant timer: Unbreakable internal cooldown
// Player::talentTimer / talentCounter are indexed by Talent; see each talent.
//
// Refined/Exalted effects (specs/infusion-grades.md) keep their state in
// Player::gradeTimer / gradeCounter [tree][genus]:
//   Offense   Venoxarid timer: Rupture internal cooldown
//             Cerebrian counter: Dominate hits on Player::dominateTarget
//             Frigidariant timer: Glacier, time standing still
//             Magnarok timer: Inferno Heart patch cooldown
//   Defense   Venoxarid timer: Corrosion pulse
//             Cerebrian timer: Backlash cooldown
//             Frigidariant timer: Cryo Sanctuary cooldown
//             Magnarok timer: Heat Sink heat (not a countdown)
//   Mobility  Venoxarid timer: Toxic Wake pulse
//             Cerebrian timer: Phantom contact pulse
//             Frigidariant timer/counter: Avalanche momentum / flags
//               (MOMENTUM_READY, ON_ICE)
//             Magnarok timer: Firewalker recount
//   Genealogy Venoxarid timer: Miasma Heart pulse
//             Magnarok counter: core casts, for Supernova

namespace {

constexpr int OFFENSE = static_cast<int>(InfusionTree::Offense);
constexpr int DEFENSE = static_cast<int>(InfusionTree::Defense);
constexpr int MOBILITY = static_cast<int>(InfusionTree::Mobility);
constexpr int GENEALOGY = static_cast<int>(InfusionTree::Genealogy);

constexpr int VENOXARID = static_cast<int>(Genus::Venoxarid);
constexpr int CEREBRIAN = static_cast<int>(Genus::Cerebrian);
constexpr int FRIGIDARIANT = static_cast<int>(Genus::Frigidariant);
constexpr int MAGNAROK = static_cast<int>(Genus::Magnarok);

constexpr int SHIFT_SLOT = 3;
constexpr int CORE_SLOT = static_cast<int>(Arena::AbilitySlot::Core);

constexpr float FORTRESS_DURATION_MS = 4500.0f;
constexpr float FORTRESS_DAMAGE_TAKEN = 0.3f;
constexpr int BURNING_RUSH_KILLS = 12;
// Refined/Exalted tuning (specs/infusion-grades.md; first guesses).
constexpr int RUPTURE_STACKS = 10;
constexpr int DOMINATE_HITS = 6;
constexpr float GLACIER_STILL_MS = 500.0f;
constexpr int FLASHPOINT_STACKS = 5;
constexpr float AVALANCHE_MOVE_MS = 2000.0f;
constexpr float CRYO_COOLDOWN_MS = 25000.0f;
constexpr float HEAT_SINK_HEAT = 100.0f;
constexpr int SUPERNOVA_EVERY = 3;
constexpr uint8_t INVENTORY_MAX = 99;

int t(Talent talent) { return static_cast<int>(talent); }

float& talentTimer(Player& player, Talent talent) {
  return player.talentTimer[t(talent)];
}
int& talentCounter(Player& player, Talent talent) {
  return player.talentCounter[t(talent)];
}

int level(const Player& player, Genus genus, InfusionTree tree) {
  return player.infusionLevel(genus, tree);
}

// Whether the player has `genus`'s effect of `grade` in `tree`.
bool has(const Player& player, Genus genus, InfusionTree tree, int grade) {
  return player.infusionGrade(genus, tree) >= grade;
}
constexpr int REFINED = Arena::GRADE_REFINED;
constexpr int EXALTED = Arena::GRADE_EXALTED;

// Avalanche flags in gradeCounter[MOBILITY][FRIGIDARIANT].
constexpr int MOMENTUM_READY = 1;
constexpr int ON_ICE = 2;

int poisonStacks(AbilitySystem& sys, const Enemy& enemy) {
  return sys.effects().getEnemyEffects(enemy.id).getStacks(EffectType::Poison);
}

// True if `player` stands in one of their own zones of this theme/variant.
bool inOwnZone(AbilitySystem& sys, const Player& player, VfxTheme theme,
               uint8_t variant) {
  for (const AbilitySystem::Zone& zone : sys.allZones()) {
    if (zone.ownerId != player.id || zone.theme != theme ||
        zone.variant != variant || zone.fromEnemy) {
      continue;
    }
    if (std::hypot(zone.x - player.x, zone.y - player.y) <= zone.radius) {
      return true;
    }
  }
  return false;
}

// The Confused effect on `enemy`, if `playerId` applied it.
bool confusedBy(AbilitySystem& sys, const Enemy& enemy, uint32_t playerId) {
  const EffectInstance* confused =
      sys.effects().getEnemyEffects(enemy.id).findEffect(EffectType::Confused);
  return confused != nullptr && confused->sourceId == playerId;
}

// Recompute the derived counts after the suit changes.
void recount(Player& player) {
  for (auto& row : player.genusInTree) {
    for (uint8_t& count : row) count = 0;
  }
  for (auto& row : player.genusGradeInTree) {
    for (uint8_t& grade : row) grade = 0;
  }
  for (uint8_t& count : player.partsSlotted) count = 0;
  if (player.coreGenus != Genus::None) {
    player.partsSlotted[static_cast<int>(player.corePart)] +=
        Arena::gradeRawCount(player.coreGrade);
  }
  for (int i = 0; i < Arena::suitNodeCount(); i++) {
    Genus genus = player.nodeGenus[i];
    if (genus == Genus::None) continue;
    int tree = static_cast<int>(Arena::suitNode(i).tree);
    int g = static_cast<int>(genus);
    player.genusInTree[tree][g]++;
    player.genusGradeInTree[tree][g] =
        std::max(player.genusGradeInTree[tree][g], player.nodeGrade[i]);
    player.partsSlotted[static_cast<int>(player.nodePart[i])] +=
        Arena::gradeRawCount(player.nodeGrade[i]);
  }
  Arena::SuitSlots suit = player.suitSlots();
  for (int t = 0; t < Arena::TREE_COUNT; t++) {
    player.treeAttunement[t] =
        Arena::treeAttunement(suit, static_cast<InfusionTree>(t));
  }
  player.ascendedGenus = Arena::ascendedGenus(suit);
}

// The pack count for one item, or nullptr if the item is out of range.
uint8_t* packCount(Player& player, Genus genus, InfusionPart part, int grade) {
  int g = static_cast<int>(genus);
  int p = static_cast<int>(part);
  if (g < 1 || g > Arena::GENUS_COUNT || p < 0 || p >= Arena::PART_COUNT ||
      !Arena::validGrade(grade)) {
    return nullptr;
  }
  return &player.infusionInventory[g - 1][p][grade - 1];
}

// Sum of one part stat over every slotted item.
float partTotal(const Player& player, float Arena::InfusionPartInfo::*stat) {
  float total = 0.0f;
  for (int p = 0; p < Arena::PART_COUNT; p++) {
    total += player.partsSlotted[p] *
             (Arena::partInfo(static_cast<InfusionPart>(p)).*stat);
  }
  return total;
}

int slottedOfGenus(const Player& player, Genus genus) {
  int count = player.coreGenus == genus ? 1 : 0;
  for (int i = 0; i < Arena::suitNodeCount(); i++) {
    if (player.nodeGenus[i] == genus) count++;
  }
  return count;
}

int distinctGenera(const Player& player) {
  bool seen[Arena::GENUS_COUNT + 1] = {};
  seen[static_cast<int>(player.coreGenus)] = true;
  for (int i = 0; i < Arena::suitNodeCount(); i++) {
    seen[static_cast<int>(player.nodeGenus[i])] = true;
  }
  int count = 0;
  for (int g = 1; g <= Arena::GENUS_COUNT; g++) count += seen[g] ? 1 : 0;
  return count;
}

bool hasAffliction(const ActiveEffects& fx) {
  for (EffectType type :
       {EffectType::Poison, EffectType::Wound, EffectType::Slow,
        EffectType::Snared, EffectType::Stunned, EffectType::Confused}) {
    if (fx.hasEffect(type)) return true;
  }
  return false;
}

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
void frostEscalate(AbilitySystem& sys, Player& player, Enemy& enemy, int lvl) {
  const ActiveEffects& fx = sys.effects().getEnemyEffects(enemy.id);
  float scale = 1.0f + 0.25f * (lvl - 1);
  if (fx.hasEffect(EffectType::Snared)) {
    sys.applyToEnemy(enemy, EffectType::Stunned, 1, player.id, 800.0f * scale);
    sys.effects().removeEffect(enemy.id, true, EffectType::Snared);
  } else if (fx.hasEffect(EffectType::Slow)) {
    sys.applyToEnemy(enemy, EffectType::Snared, 1, player.id, 1200.0f * scale);
  } else {
    sys.applyToEnemy(enemy, EffectType::Slow, 1, player.id);
  }
}

// Living allies (not `self`) within `radius` of `self`.
template <typename Fn>
void forAllies(AbilitySystem& sys, const Player& self, float radius, Fn fn) {
  uint32_t selfId = self.id;
  sys.playersInCircle(self.x, self.y, radius, [&](Player& other) {
    if (other.id == selfId || other.isDead() || other.trainingDummy) return;
    fn(other);
  });
}

// Genealogy: core ability damage carries each genus slotted there.
void genealogyOnCoreHit(AbilitySystem& sys, Player& player, Enemy& enemy,
                        float damage) {
  uint32_t playerId = player.id;
  uint32_t enemyId = enemy.id;
  if (has(player, Genus::Cerebrian, InfusionTree::Genealogy, REFINED) &&
      damage > 0.0f) {
    // Mind Echo: the hit repeats as a psychic echo.
    AbilitySystem* s = &sys;
    float echo = damage * 0.4f;
    sys.schedule(1500.0f, [s, playerId, enemyId, echo]() {
      Enemy* still = s->findEnemy(enemyId);
      if (!still) return;
      s->fxRing(still->x, still->y, 40.0f, VfxTheme::Psychic);
      s->damageEnemy(playerId, *still, echo, DamageFlag::Proc);
    });
  }
  if (has(player, Genus::Magnarok, InfusionTree::Genealogy, REFINED)) {
    // Fuel: the core ability ignites.
    sys.applyToEnemy(enemy, EffectType::Wound, 1, playerId, 4000.0f);
  }
  if (has(player, Genus::Frigidariant, InfusionTree::Genealogy, EXALTED)) {
    // Diamond Heart: the core ability freezes.
    sys.applyToEnemy(enemy, EffectType::Stunned, 1, playerId, 1200.0f);
  }
  int venom = level(player, Genus::Venoxarid, InfusionTree::Genealogy);
  if (venom > 0) {
    // Venom Infusion
    sys.applyToEnemy(enemy, EffectType::Poison, venom, playerId);
  }
  int psychic = level(player, Genus::Cerebrian, InfusionTree::Genealogy);
  if (psychic > 0) {
    // Mass Confusion
    sys.applyToEnemy(enemy, EffectType::Confused, 1, playerId,
                     1200.0f + 600.0f * psychic);
  }
  // Frigidariant (Unbreakable) triggers from hard CC: onHardCrowdControl.
  int molten = level(player, Genus::Magnarok, InfusionTree::Genealogy);
  float& cataclysm = player.infusionTimer[GENEALOGY][MAGNAROK];
  if (molten > 0 && cataclysm <= 0.0f) {
    cataclysm = 350.0f;
    spawnMoltenPatch(sys, playerId, enemy.x, enemy.y, 60.0f, 3000.0f,
                     4.0f * molten);
  }
}

void offenseOnHit(AbilitySystem& sys, Player& player, Enemy& enemy) {
  uint32_t playerId = player.id;
  uint32_t enemyId = enemy.id;
  float ex = enemy.x, ey = enemy.y;

  // Rupture: a heavily poisoned target pops before more venom goes on.
  float& rupture = player.gradeTimer[OFFENSE][VENOXARID];
  if (has(player, Genus::Venoxarid, InfusionTree::Offense, REFINED) &&
      rupture <= 0.0f) {
    int stacks = poisonStacks(sys, enemy);
    if (stacks >= RUPTURE_STACKS) {
      rupture = 300.0f;
      sys.effects().removeEffect(enemyId, true, EffectType::Poison);
      sys.fxBurst(ex, ey, 90.0f, VfxTheme::Poison);
      sys.enemiesInCircle(ex, ey, 90.0f, [&](Enemy& other) {
        if (other.id != enemyId) {
          sys.applyToEnemy(other, EffectType::Poison, stacks / 2, playerId);
        }
      });
      if (Enemy* still = sys.findEnemy(enemyId)) {
        sys.damageEnemy(playerId, *still, 6.0f * stacks,
                        DamageFlag::Proc | DamageFlag::Explosion);
      }
      if (!sys.findEnemy(enemyId)) return;
    }
  }

  // Dominate: every 6th hit on one enemy turns it.
  if (has(player, Genus::Cerebrian, InfusionTree::Offense, REFINED)) {
    int& hits = player.gradeCounter[OFFENSE][CEREBRIAN];
    if (player.dominateTarget != enemyId) {
      player.dominateTarget = enemyId;
      hits = 0;
    }
    if (++hits >= DOMINATE_HITS) {
      hits = 0;
      if (Enemy* still = sys.findEnemy(enemyId)) {
        sys.fxRing(still->x, still->y, 45.0f, VfxTheme::Psychic);
        sys.applyToEnemy(*still, EffectType::Confused, 1, playerId, 2000.0f);
      }
    }
  }

  int venom = level(player, Genus::Venoxarid, InfusionTree::Offense);
  if (venom > 0) {
    // Toxic Blade: venom splashes to nearby enemies.
    sys.applyToEnemy(enemy, EffectType::Poison, venom, playerId);
    float& icd = player.infusionTimer[OFFENSE][VENOXARID];
    if (icd <= 0.0f) {
      icd = 200.0f;
      sys.enemiesInCircle(ex, ey, 75.0f, [&](Enemy& other) {
        if (other.id != enemyId) {
          sys.applyToEnemy(other, EffectType::Poison, 1, playerId);
        }
      });
    }
  }
  int psychic = level(player, Genus::Cerebrian, InfusionTree::Offense);
  float& shockwave = player.infusionTimer[OFFENSE][CEREBRIAN];
  if (psychic > 0 && shockwave <= 0.0f) {
    // Psychic Shockwave: small burst around the target.
    shockwave = 180.0f;
    sys.fxRing(ex, ey, 80.0f, VfxTheme::Psychic);
    sys.enemiesInCircle(ex, ey, 80.0f, [&](Enemy& other) {
      sys.damageEnemy(playerId, other, 6.0f * psychic, DamageFlag::Proc);
    });
  }
  int frost = level(player, Genus::Frigidariant, InfusionTree::Offense);
  if (frost > 0) {
    Enemy* still = sys.findEnemy(enemyId);
    bool frozen = still && sys.effects().getEnemyEffects(enemyId).hasEffect(
                               EffectType::Stunned);
    if (still && frozen &&
        has(player, Genus::Frigidariant, InfusionTree::Offense, REFINED)) {
      // Splinter: the freeze shatters outward.
      float burst = std::min(60.0f, still->maxHealth * 0.12f);
      float sx = still->x, sy = still->y;
      sys.effects().removeEffect(enemyId, true, EffectType::Stunned);
      sys.fxBurst(sx, sy, 80.0f, VfxTheme::Ice);
      sys.enemiesInCircle(sx, sy, 80.0f, [&](Enemy& other) {
        sys.damageEnemy(playerId, other, burst,
                        DamageFlag::Proc | DamageFlag::Explosion);
      });
    } else if (still &&
               has(player, Genus::Frigidariant, InfusionTree::Offense,
                   EXALTED) &&
               player.gradeTimer[OFFENSE][FRIGIDARIANT] >= GLACIER_STILL_MS) {
      // Glacier: planted, every hit freezes outright.
      sys.applyToEnemy(*still, EffectType::Stunned, 1, playerId,
                       800.0f * (1.0f + 0.25f * (frost - 1)));
    } else if (still) {
      // Icy Strikes
      frostEscalate(sys, player, *still, frost);
    }
  }
  int molten = level(player, Genus::Magnarok, InfusionTree::Offense);
  if (molten > 0) {
    // Eruptive Strikes: ignite. Burn time grows with each infusion.
    if (Enemy* still = sys.findEnemy(enemyId)) {
      sys.applyToEnemy(*still, EffectType::Wound, 1, playerId,
                       4000.0f + 2000.0f * molten);
      sys.damageEnemy(playerId, *still, 3.0f * molten, DamageFlag::Proc);
    }
    Enemy* still = sys.findEnemy(enemyId);
    const EffectInstance* burn =
        still ? sys.effects().getEnemyEffects(enemyId).findEffect(
                    EffectType::Wound)
              : nullptr;
    if (burn && burn->stacks >= FLASHPOINT_STACKS &&
        has(player, Genus::Magnarok, InfusionTree::Offense, REFINED)) {
      // Flashpoint: the burn goes up all at once.
      float blast = std::min(
          150.0f, burn->remainingDuration / 1000.0f * burn->stacks * 2.0f);
      float bx = still->x, by = still->y;
      sys.effects().removeEffect(enemyId, true, EffectType::Wound);
      sys.fxBurst(bx, by, 70.0f, VfxTheme::Fire);
      sys.enemiesInCircle(bx, by, 70.0f, [&](Enemy& other) {
        sys.damageEnemy(playerId, other, blast,
                        DamageFlag::Proc | DamageFlag::Explosion);
      });
    }
    float& inferno = player.gradeTimer[OFFENSE][MAGNAROK];
    if (has(player, Genus::Magnarok, InfusionTree::Offense, EXALTED) &&
        inferno <= 0.0f) {
      // Inferno Heart: hits leave molten ground.
      if (Enemy* target = sys.findEnemy(enemyId)) {
        inferno = 500.0f;
        spawnMoltenPatch(sys, playerId, target->x, target->y, 55.0f, 3000.0f,
                         4.0f);
      }
    }
  }
}

// Swarm Rider: enough poisoned enemies around (counted by Speed Siphon).
bool swarmRiding(const Player& player) {
  return has(player, Genus::Venoxarid, InfusionTree::Mobility, EXALTED) &&
         player.infusionCounter[MOBILITY][VENOXARID] >= 5;
}

}  // namespace

namespace Infusions {

// ---------------------------------------------------------------------------
// Loadout
// ---------------------------------------------------------------------------

bool addToInventory(Player& player, Genus genus, InfusionPart part, int grade) {
  uint8_t* count = packCount(player, genus, part, grade);
  if (!count || *count >= INVENTORY_MAX) return false;
  (*count)++;
  return true;
}

bool combineInfusions(Player& player, Genus genus, int grade,
                      InfusionPart targetPart, InfusionPart otherPart) {
  if (!Arena::canCombine(player.ascendedGenus, genus, grade)) return false;
  uint8_t* target = packCount(player, genus, targetPart, grade);
  uint8_t* other = packCount(player, genus, otherPart, grade);
  if (!target || !other) return false;
  // Two items: both counts are the same slot when the parts match.
  int needed = target == other ? 2 : 1;
  if (*target < needed || *other < 1) return false;
  uint8_t* result = packCount(player, genus, targetPart, grade + 1);
  assert(result != nullptr);
  if (*result >= INVENTORY_MAX) return false;
  (*target)--;
  (*other)--;
  (*result)++;
  return true;
}

bool slotInfusion(Player& player, Genus genus, InfusionPart part, int grade,
                  int node, int maxTier) {
  uint8_t* count = packCount(player, genus, part, grade);
  if (!count || *count == 0) return false;
  if (node != Arena::CORE_NODE &&
      (node < 0 || node >= Arena::suitNodeCount())) {
    return false;
  }
  if (Arena::slotBlock(player.suitSlots(), node, genus, grade, maxTier) !=
      Arena::SlotBlock::None) {
    return false;
  }

  if (node == Arena::CORE_NODE) {
    // The first infusion must go in the core, and the core is permanent.
    player.coreGenus = genus;
    player.corePart = part;
    player.coreGrade = static_cast<uint8_t>(grade);
  } else {
    player.nodeGenus[node] = genus;
    player.nodePart[node] = part;
    player.nodeGrade[node] = static_cast<uint8_t>(grade);
  }
  (*count)--;
  recount(player);

  // A Hide raises max health at once, and fills the new health in.
  float hide = Arena::partInfo(part).maxHealthBonus *
               static_cast<float>(Arena::gradeRawCount(grade));
  if (hide > 0.0f) {
    player.maxHealth += hide;
    if (!player.isDead()) player.health += hide;
  }
  return true;
}

void resetLoadout(Player& player) {
  for (auto& genusRow : player.infusionInventory) {
    for (auto& partRow : genusRow) {
      for (uint8_t& count : partRow) count = 0;
    }
  }
  player.coreGenus = Genus::None;
  player.corePart = InfusionPart::Talon;
  player.coreGrade = Arena::GRADE_RAW;
  for (int i = 0; i < Arena::MAX_SUIT_NODES; i++) {
    player.nodeGenus[i] = Genus::None;
    player.nodePart[i] = InfusionPart::Talon;
    player.nodeGrade[i] = 0;
  }
  for (auto& row : player.infusionCounter) {
    for (int& counter : row) counter = 0;
  }
  for (auto& row : player.gradeCounter) {
    for (int& counter : row) counter = 0;
  }
  player.dominateTarget = 0;
  recount(player);
  resetForRound(player);
}

void resetForRound(Player& player) {
  for (auto& row : player.infusionTimer) {
    for (float& timer : row) timer = 0.0f;
  }
  for (auto& row : player.gradeTimer) {
    for (float& timer : row) timer = 0.0f;
  }
  for (float& timer : player.talentTimer) timer = 0.0f;
  for (int& counter : player.talentCounter) counter = 0;
}

float maxHealthBonus(const Player& player) {
  return partTotal(player, &Arena::InfusionPartInfo::maxHealthBonus);
}

// ---------------------------------------------------------------------------
// Core ability
// ---------------------------------------------------------------------------

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

  if (has(self, Genus::Magnarok, InfusionTree::Genealogy, EXALTED)) {
    // Supernova: every third cast erupts around you.
    int& casts = self.gradeCounter[GENEALOGY][MAGNAROK];
    if (++casts >= SUPERNOVA_EVERY) {
      casts = 0;
      const float radius = 300.0f;
      sys.fxBurst(self.x, self.y, radius, VfxTheme::Fire);
      sys.fxRing(self.x, self.y, radius + 30.0f, VfxTheme::Fire);
      sys.enemiesInCircle(self.x, self.y, radius, [&](Enemy& enemy) {
        sys.damageEnemy(selfId, enemy, 40.0f,
                        DamageFlag::Core | DamageFlag::Explosion);
      });
      spawnMoltenPatch(sys, selfId, self.x, self.y, 200.0f, 6000.0f, 6.0f);
    }
  }

  // Shared Gift: the core ability heals the team around you.
  if (self.hasTalent(Talent::SharedGift)) {
    forAllies(sys, self, 300.0f, [&](Player& ally) {
      sys.healPlayer(selfId, ally, ally.maxHealth * 0.08f);
    });
  }
}

// ---------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------

float damageMultiplier(AbilitySystem& sys, const Player& player,
                       const Enemy& enemy, uint32_t flags) {
  float mult = 1.0f + partTotal(player, &Arena::InfusionPartInfo::damageBonus);
  bool direct = !(flags & (DamageFlag::Dot | DamageFlag::Proc));

  if (player.hasTalent(Talent::Purebred) && player.coreGenus != Genus::None) {
    mult += 0.03f * slottedOfGenus(player, player.coreGenus);
  }
  if (player.hasTalent(Talent::HybridVigor)) {
    mult += 0.04f * distinctGenera(player);
  }
  if (player.hasTalent(Talent::Relentless)) {
    mult += 0.03f * player.talentCounter[t(Talent::Relentless)];
  }
  if (player.hasTalent(Talent::Overcharge) && (flags & DamageFlag::Core)) {
    mult += 0.30f;
  }
  if ((flags & DamageFlag::Core) &&
      has(player, Genus::Venoxarid, InfusionTree::Genealogy, REFINED) &&
      sys.effects().getEnemyEffects(enemy.id).getStacks(EffectType::Poison) >=
          3) {
    mult += 1.0f;  // Toxic Catalyst
  }
  if (player.hasTalent(Talent::OpeningStrike) && direct &&
      enemy.health >= enemy.maxHealth) {
    mult += 0.50f;
  }
  if (player.hasTalent(Talent::Executioner) &&
      enemy.health < enemy.maxHealth * 0.30f) {
    mult += 0.35f;
  }
  if (player.hasTalent(Talent::Affliction) &&
      hasAffliction(sys.effects().getEnemyEffects(enemy.id))) {
    mult += 0.20f;
  }
  if (player.hasTalent(Talent::Blitz) && direct &&
      player.talentTimer[t(Talent::Blitz)] > 0.0f) {
    mult += 0.60f;
  }
  return mult;
}

float critChanceBonus(const Player& player) {
  return player.hasTalent(Talent::KillerInstinct) ? 0.05f : 0.0f;
}

float rechargeMultiplier(const Player& player, int slot) {
  float mult =
      1.0f + partTotal(player, &Arena::InfusionPartInfo::rechargeBonus);
  if (slot == SHIFT_SLOT && player.hasTalent(Talent::QuickRecovery)) {
    mult += 0.25f;
  }
  if (slot == CORE_SLOT && player.hasTalent(Talent::Bloodline)) mult += 0.15f;
  if (slot == SHIFT_SLOT && swarmRiding(player)) mult += 0.5f;
  return mult;
}

void onHitEnemy(AbilitySystem& sys, Player& player, Enemy& enemy, float damage,
                uint32_t flags) {
  (void)damage;
  uint32_t enemyId = enemy.id;

  // ---- talents --------------------------------------------------------------
  if (player.hasTalent(Talent::FocusFire) &&
      !sys.effects().getEnemyEffects(enemyId).hasEffect(
          EffectType::Vulnerable)) {
    sys.applyToEnemy(enemy, EffectType::Vulnerable, 1, player.id, 2000.0f);
  }
  if (player.hasTalent(Talent::Blitz) &&
      player.talentTimer[t(Talent::Blitz)] > 0.0f) {
    talentTimer(player, Talent::Blitz) = 0.0f;
    if (Enemy* still = sys.findEnemy(enemyId)) {
      sys.applyToEnemy(*still, EffectType::Slow, 1, player.id);
    }
  }
  if (player.hasTalent(Talent::Relentless) && (flags & DamageFlag::Basic)) {
    int& stacks = talentCounter(player, Talent::Relentless);
    stacks = std::min(5, stacks + 1);
    talentTimer(player, Talent::Relentless) = 2000.0f;
  }

  Enemy* still = sys.findEnemy(enemyId);
  if (!still) return;
  if (damage > 0.0f && confusedBy(sys, *still, player.id) &&
      has(player, Genus::Cerebrian, InfusionTree::Genealogy, EXALTED)) {
    // Overmind: every enemy you confused shares the hit.
    float share = damage * 0.3f;
    float sx = still->x, sy = still->y;
    uint32_t playerId = player.id;
    int linked = 0;
    sys.enemiesInCircle(sx, sy, 400.0f, [&](Enemy& other) {
      if (other.id == enemyId || linked >= 8 ||
          !confusedBy(sys, other, playerId)) {
        return;
      }
      linked++;
      sys.fxBeam(sx, sy, other.x, other.y, 6.0f, 160.0f, VfxTheme::Psychic);
      sys.damageEnemy(playerId, other, share, DamageFlag::Proc);
    });
    still = sys.findEnemy(enemyId);
    if (!still) return;
  }
  // Core ability damage feeds Genealogy and never triggers Offense.
  if (flags & DamageFlag::Core) {
    genealogyOnCoreHit(sys, player, *still, damage);
    return;
  }
  int& momentum = player.gradeCounter[MOBILITY][FRIGIDARIANT];
  if ((momentum & MOMENTUM_READY) &&
      has(player, Genus::Frigidariant, InfusionTree::Mobility, EXALTED)) {
    // Avalanche: the built-up momentum breaks over the target and a cone.
    momentum &= ~MOMENTUM_READY;
    player.gradeTimer[MOBILITY][FRIGIDARIANT] = 0.0f;
    uint32_t playerId = player.id;
    float dx = still->x - player.x, dy = still->y - player.y;
    float d = std::max(1.0f, std::hypot(dx, dy));
    sys.applyToEnemy(*still, EffectType::Stunned, 1, playerId, 1200.0f);
    sys.fxCone(player.x, player.y, dx / d, dy / d, 170.0f, 90.0f,
               VfxTheme::Ice);
    sys.enemiesInCone(
        player.x, player.y, dx / d, dy / d, 170.0f, 45.0f, [&](Enemy& other) {
          sys.applyToEnemy(other, EffectType::Stunned, 1, playerId, 1000.0f);
          sys.damageEnemy(playerId, other, 20.0f, DamageFlag::Proc);
        });
    still = sys.findEnemy(enemyId);
    if (!still) return;
  }
  offenseOnHit(sys, player, *still);
}

float onPlayerDamaged(AbilitySystem& sys, Player& player, float damage,
                      Enemy* source, uint32_t flags) {
  // Diamond Fortress
  if (player.coreGenus == Genus::Frigidariant && player.coreTimerMs > 0.0f) {
    damage *= FORTRESS_DAMAGE_TAKEN;
  }
  bool melee = (flags & DamageFlag::Melee) != 0 && source != nullptr;
  bool shared = (flags & DamageFlag::Shared) != 0;

  // ---- talents: less damage -------------------------------------------------
  float reduction = 0.0f;
  if (player.hasTalent(Talent::Plating)) reduction += 0.08f;
  if (player.hasTalent(Talent::HybridVigor)) {
    reduction += 0.04f * distinctGenera(player);
  }
  bool guarded = false;
  forAllies(sys, player, 250.0f, [&](Player& ally) {
    if (ally.hasTalent(Talent::Guardian)) guarded = true;
  });
  if (guarded) reduction += 0.10f;
  damage *= std::max(0.3f, 1.0f - reduction);

  // Share the Load: the nearest ally who took it on takes a quarter.
  if (!shared && damage > 0.0f) {
    Player* carrier = nullptr;
    float best = 1e9f;
    forAllies(sys, player, 300.0f, [&](Player& ally) {
      if (!ally.hasTalent(Talent::ShareTheLoad)) return;
      float d = std::hypot(ally.x - player.x, ally.y - player.y);
      if (d < best) {
        best = d;
        carrier = &ally;
      }
    });
    if (carrier) {
      float share = damage * 0.25f;
      damage -= share;
      sys.fxBeam(player.x, player.y, carrier->x, carrier->y, 10.0f, 200.0f,
                 VfxTheme::Ice);
      sys.damagePlayer(*carrier, share, source ? source->id : 0,
                       (flags & DamageFlag::Dot) | DamageFlag::Shared);
    }
  }

  // ---- genus effects in the Defense tree ------------------------------------
  int venom = level(player, Genus::Venoxarid, InfusionTree::Defense);
  if (venom > 0) {
    // Acidic Shell
    if (melee) sys.applyToEnemy(*source, EffectType::Poison, venom, player.id);
    if (source && sys.effects()
                      .getEnemyEffects(source->id)
                      .hasEffect(EffectType::Poison)) {
      damage *= std::max(0.4f, 1.0f - 0.12f * venom);
    }
  }
  if (level(player, Genus::Cerebrian, InfusionTree::Defense) > 0) {
    // Mind Barrier: any hit restarts the recharge delay.
    player.infusionTimer[DEFENSE][CEREBRIAN] = 0.0f;
  }
  int frost = level(player, Genus::Frigidariant, InfusionTree::Defense);
  if (frost > 0 && melee)
    frostEscalate(sys, player, *source, frost);  // Arctic Aura
  int molten = level(player, Genus::Magnarok, InfusionTree::Defense);
  if (molten > 0 && !(flags & DamageFlag::Dot)) {
    // Slow Burn: part of the hit is deferred and dealt over time.
    float fraction = std::min(0.7f, 0.3f + 0.1f * molten);
    player.infusionTimer[DEFENSE][MAGNAROK] += damage * fraction;
    if (has(player, Genus::Magnarok, InfusionTree::Defense, EXALTED)) {
      // Heat Sink: held-back damage builds heat that vents as a nova.
      float& heat = player.gradeTimer[DEFENSE][MAGNAROK];
      heat += damage * fraction;
      if (heat >= HEAT_SINK_HEAT) {
        heat = 0.0f;
        uint32_t playerId = player.id;
        sys.fxBurst(player.x, player.y, 200.0f, VfxTheme::Fire);
        sys.enemiesInCircle(player.x, player.y, 200.0f, [&](Enemy& enemy) {
          sys.damageEnemy(playerId, enemy, 40.0f,
                          DamageFlag::Proc | DamageFlag::Explosion);
        });
        sys.applyToPlayer(player, EffectType::Fortified, 3, playerId, 3000.0f);
      }
    }
    damage *= (1.0f - fraction);
  }

  if (melee && player.hasTalent(Talent::Thorns)) {
    sys.damageEnemy(player.id, *source, 8.0f, DamageFlag::Proc);
  }
  talentTimer(player, Talent::Regrowth) = 0.0f;

  // ---- talents: thresholds --------------------------------------------------
  float healthAfter = player.health - std::max(0.0f, damage - player.shield);
  bool lastStood = false;
  if (player.hasTalent(Talent::LastStand) && healthAfter < 1.0f &&
      talentCounter(player, Talent::LastStand) == 0) {
    lastStood = true;
    talentCounter(player, Talent::LastStand) = 1;
    damage = std::max(0.0f, player.shield + player.health - 1.0f);
    healthAfter = 1.0f;
    player.intangibleMs = std::max(player.intangibleMs, 1500.0f);
    GameEventPacket bubble;
    bubble.kind = GameEventKind::ShieldBubble;
    bubble.sourceId = player.id;
    bubble.x = player.x;
    bubble.y = player.y;
    bubble.value2 = 1500.0f;
    bubble.param2 = static_cast<uint8_t>(VfxTheme::Fire);
    sys.emit(bubble);
  }
  float& cryo = player.gradeTimer[DEFENSE][FRIGIDARIANT];
  if (has(player, Genus::Frigidariant, InfusionTree::Defense, EXALTED) &&
      cryo <= 0.0f && healthAfter > 0.0f && !lastStood &&
      healthAfter < player.maxHealth * 0.5f) {
    // Cryo Sanctuary: an ice dome that roots enemies and shelters allies.
    // Not on the blow Last Stand just caught: one big save per hit.
    cryo = CRYO_COOLDOWN_MS;
    uint32_t playerId = player.id;
    AbilitySystem* s = &sys;
    AbilitySystem::Zone dome;
    dome.ownerId = playerId;
    dome.x = player.x;
    dome.y = player.y;
    dome.radius = 160.0f;
    dome.lifeMs = 5000.0f;
    dome.tickMs = 250.0f;
    dome.theme = VfxTheme::Ice;
    dome.variant = 2;
    dome.onEnemyTick = [s, playerId](AbilitySystem::Zone&, Enemy& enemy) {
      s->applyToEnemy(enemy, EffectType::Snared, 1, playerId, 600.0f);
    };
    dome.onPlayerTick = [s, playerId](AbilitySystem::Zone&, Player& ally) {
      if (ally.isDead()) return;
      // Fortified is 10% a stack: two is the dome's 20%.
      s->applyToPlayer(ally, EffectType::Fortified, 2, playerId, 400.0f);
    };
    sys.spawnZone(dome);
    sys.fxRing(player.x, player.y, 160.0f, VfxTheme::Ice);
  }

  // Backlash: the barrier breaking stuns whatever broke it.
  float& backlash = player.gradeTimer[DEFENSE][CEREBRIAN];
  if (has(player, Genus::Cerebrian, InfusionTree::Defense, REFINED) &&
      backlash <= 0.0f && player.shield > 0.0f && damage >= player.shield) {
    backlash = 4000.0f;
    uint32_t playerId = player.id;
    sys.fxBurst(player.x, player.y, 150.0f, VfxTheme::Psychic);
    sys.enemiesInCircle(player.x, player.y, 150.0f, [&](Enemy& enemy) {
      sys.applyToEnemy(enemy, EffectType::Stunned, 1, playerId, 1000.0f);
    });
  }

  bool wasAbove35 = player.health >= player.maxHealth * 0.35f;
  bool wasAbove40 = player.health >= player.maxHealth * 0.40f;
  if (player.hasTalent(Talent::SecondWind) && wasAbove35 &&
      healthAfter < player.maxHealth * 0.35f && healthAfter > 0.0f &&
      talentTimer(player, Talent::SecondWind) <= 0.0f) {
    talentTimer(player, Talent::SecondWind) = 30000.0f;
    sys.grantShield(player, player.maxHealth * 0.25f, 6000.0f);
  }
  if (player.hasTalent(Talent::EscapeArtist) && wasAbove40 &&
      healthAfter < player.maxHealth * 0.40f && healthAfter > 0.0f &&
      talentTimer(player, Talent::EscapeArtist) <= 0.0f) {
    talentTimer(player, Talent::EscapeArtist) = 20000.0f;
    sys.applyToPlayer(player, EffectType::Haste, 2, player.id, 3000.0f);
    sys.applyToPlayer(player, EffectType::Unbounded, 1, player.id, 3000.0f);
  }
  return damage;
}

void onHardCrowdControl(AbilitySystem& sys, Player& player, Enemy& enemy) {
  if (has(player, Genus::Frigidariant, InfusionTree::Genealogy, REFINED) &&
      !sys.effects().getEnemyEffects(enemy.id).hasEffect(
          EffectType::Vulnerable)) {
    // Brittle: what you lock down, the team hits harder.
    sys.applyToEnemy(enemy, EffectType::Vulnerable, 1, player.id, 2000.0f);
  }
  int frost = level(player, Genus::Frigidariant, InfusionTree::Genealogy);
  if (frost <= 0) return;
  // Unbreakable: at most one shield refresh per half second.
  float& icd = player.infusionTimer[GENEALOGY][FRIGIDARIANT];
  if (icd > 0.0f) return;
  icd = 500.0f;
  sys.grantShield(player, 22.0f * frost, 4000.0f);
}

void onKill(AbilitySystem& sys, Player& player, const Enemy& enemy) {
  (void)enemy;
  int molten = level(player, Genus::Magnarok, InfusionTree::Mobility);
  if (molten > 0) {
    // Burning Rush
    int& kills = player.infusionCounter[MOBILITY][MAGNAROK];
    kills++;
    if (kills >= BURNING_RUSH_KILLS) {
      kills = 0;
      player.infusionTimer[MOBILITY][MAGNAROK] =
          5000.0f * (1.0f + 0.3f * (molten - 1));
      sys.fxRing(player.x, player.y, 90.0f, VfxTheme::Fire);
    }
  }
  if (player.hasTalent(Talent::Bloodlust) &&
      sys.effects().getPlayerEffects(player.id).getStacks(
          EffectType::Empowered) < 3) {
    sys.applyToPlayer(player, EffectType::Empowered, 1, player.id, 3000.0f);
  }
  if (player.hasTalent(Talent::HitAndRun)) {
    talentTimer(player, Talent::HitAndRun) = 2000.0f;
  }
  if (player.hasTalent(Talent::FeedbackLoop)) {
    float& cooldown = player.cooldownMs[CORE_SLOT];
    cooldown = std::max(0.0f, cooldown - 500.0f);
  }
}

void onEnemyDied(AbilitySystem& sys, const Enemy& enemy, uint32_t killerId) {
  const ActiveEffects& fx = sys.effects().getEnemyEffects(enemy.id);
  const EffectInstance* poison = fx.findEffect(EffectType::Poison);
  bool burning = fx.hasEffect(EffectType::Wound);
  uint32_t deadId = enemy.id;
  float ex = enemy.x, ey = enemy.y;
  int stacks = poison ? poison->stacks : 0;
  uint32_t poisoner = poison ? poison->sourceId : 0;

  for (auto& [id, player] : sys.allPlayers()) {
    if (player.characterId == 0 || player.isDead()) continue;
    uint32_t playerId = player.id;
    if (stacks > 0 && poisoner == playerId &&
        has(player, Genus::Venoxarid, InfusionTree::Offense, EXALTED)) {
      // Plague Carrier: the poison jumps to the two nearest enemies.
      for (int jump = 0; jump < 2; jump++) {
        Enemy* next = nullptr;
        float best = 250.0f;
        sys.enemiesInCircle(ex, ey, 250.0f, [&](Enemy& other) {
          if (other.id == deadId) return;
          if (sys.effects().getEnemyEffects(other.id).getStacks(
                  EffectType::Poison) >= stacks) {
            return;  // already carries as much: try someone else
          }
          float d = std::hypot(other.x - ex, other.y - ey);
          if (d < best) {
            best = d;
            next = &other;
          }
        });
        if (!next) break;
        sys.fxBeam(ex, ey, next->x, next->y, 8.0f, 220.0f, VfxTheme::Poison);
        int held = sys.effects().getEnemyEffects(next->id).getStacks(
            EffectType::Poison);
        sys.applyToEnemy(*next, EffectType::Poison, stacks - held, playerId);
      }
    }
    if (stacks > 0 &&
        has(player, Genus::Venoxarid, InfusionTree::Defense, EXALTED) &&
        std::hypot(player.x - ex, player.y - ey) <= 200.0f) {
      // Symbiote: a poisoned death near you feeds you and your crew.
      sys.healPlayer(playerId, player, player.maxHealth * 0.02f);
      forAllies(sys, player, 200.0f, [&](Player& ally) {
        sys.healPlayer(playerId, ally, ally.maxHealth * 0.02f);
      });
    }
    if (burning && playerId == killerId &&
        has(player, Genus::Magnarok, InfusionTree::Defense, REFINED)) {
      // Cauterizing Wounds
      player.infusionTimer[DEFENSE][MAGNAROK] *= 0.7f;
    }
  }
}

uint32_t puppeteerOf(AbilitySystem& sys, const Enemy& enemy) {
  const EffectInstance* confused =
      sys.effects().getEnemyEffects(enemy.id).findEffect(EffectType::Confused);
  if (!confused) return 0;
  Player* owner = sys.findPlayer(confused->sourceId);
  if (!owner || owner->isDead() ||
      !has(*owner, Genus::Cerebrian, InfusionTree::Offense, EXALTED)) {
    return 0;
  }
  return owner->id;
}

void onDash(AbilitySystem& sys, Player& player, float x0, float y0, float x1,
            float y1) {
  uint32_t playerId = player.id;
  int psychic = level(player, Genus::Cerebrian, InfusionTree::Mobility);
  if (psychic > 0) {
    // Phase Shift: everything along the path takes psychic damage.
    sys.fxBeam(x0, y0, x1, y1, 26.0f, 260.0f, VfxTheme::Psychic);
    sys.enemiesInLine(x0, y0, x1, y1, 30.0f, [&](Enemy& enemy) {
      sys.damageEnemy(playerId, enemy, 18.0f * psychic, DamageFlag::Proc);
    });
  }
  if (has(player, Genus::Cerebrian, InfusionTree::Mobility, REFINED)) {
    // Ghost Step: enemies lose you for a moment.
    player.hiddenMs = std::max(player.hiddenMs, 1500.0f);
  }
  if (has(player, Genus::Magnarok, InfusionTree::Mobility, REFINED)) {
    // Eruption Dash: land in a molten burst.
    sys.fxBurst(x1, y1, 90.0f, VfxTheme::Fire);
    sys.enemiesInCircle(x1, y1, 90.0f, [&](Enemy& enemy) {
      sys.damageEnemy(playerId, enemy, 20.0f,
                      DamageFlag::Proc | DamageFlag::Explosion);
    });
    spawnMoltenPatch(sys, playerId, x1, y1, 70.0f, 2500.0f, 5.0f);
  }
}

void onAbilityUsed(AbilitySystem& sys, Player& player, int slot) {
  if (slot != SHIFT_SLOT) return;
  if (player.hasTalent(Talent::Slipstream)) {
    sys.applyToPlayer(player, EffectType::Haste, 1, player.id, 2000.0f);
  }
  if (player.hasTalent(Talent::Tailwind)) {
    forAllies(sys, player, 250.0f, [&](Player& ally) {
      sys.applyFrom(player, ally, EffectType::Haste, 1);
    });
  }
  if (player.hasTalent(Talent::Blitz)) {
    talentTimer(player, Talent::Blitz) = 2000.0f;
  }
}

void tick(AbilitySystem& sys, Player& player, float dtMs) {
  uint32_t playerId = player.id;
  AbilitySystem* s = &sys;
  auto countDown = [dtMs](float& timer) {
    timer = std::max(0.0f, timer - dtMs);
  };

  for (int g = 1; g <= Arena::GENUS_COUNT; g++) {
    countDown(player.infusionTimer[OFFENSE][g]);
  }
  countDown(player.infusionTimer[GENEALOGY][MAGNAROK]);
  countDown(player.infusionTimer[GENEALOGY][FRIGIDARIANT]);
  // Refined/Exalted cooldowns and pulses (Heat and Glacier/Avalanche
  // timers are not countdowns).
  countDown(player.gradeTimer[OFFENSE][VENOXARID]);
  countDown(player.gradeTimer[OFFENSE][MAGNAROK]);
  countDown(player.gradeTimer[DEFENSE][CEREBRIAN]);
  countDown(player.gradeTimer[DEFENSE][FRIGIDARIANT]);

  // ---- talents --------------------------------------------------------------
  for (Talent cooldown : {Talent::SecondWind, Talent::EscapeArtist,
                          Talent::HitAndRun, Talent::Blitz}) {
    countDown(talentTimer(player, cooldown));
  }
  if (player.hasTalent(Talent::Relentless)) {
    float& window = talentTimer(player, Talent::Relentless);
    countDown(window);
    if (window <= 0.0f) talentCounter(player, Talent::Relentless) = 0;
  }
  if (player.hasTalent(Talent::Regrowth)) {
    // Time since the last hit; heals once a second after three seconds.
    float& quiet = talentTimer(player, Talent::Regrowth);
    quiet += dtMs;
    if (quiet >= 4000.0f) {
      quiet -= 1000.0f;
      if (player.health < player.maxHealth) {
        sys.healPlayer(playerId, player, player.maxHealth * 0.03f, false);
      }
    }
  }

  // Diamond Fortress wears off on its own with a harmless crack.
  if (player.coreTimerMs > 0.0f) {
    player.coreTimerMs -= dtMs;
    if (player.coreTimerMs <= 0.0f) {
      player.coreTimerMs = 0.0f;
      sys.fxRing(player.x, player.y, 70.0f, VfxTheme::Ice);
    }
  }

  // ---- Defense ------------------------------------------------------------
  int barrierLevel = level(player, Genus::Cerebrian, InfusionTree::Defense);
  if (barrierLevel > 0) {
    // Mind Barrier: recharges after six seconds without taking a hit.
    float& sinceHit = player.infusionTimer[DEFENSE][CEREBRIAN];
    sinceHit += dtMs;
    float barrier = 28.0f * barrierLevel;
    if (sinceHit >= 6000.0f && player.shield < barrier) {
      sys.grantShield(player, barrier, 600000.0f);
      sinceHit = 0.0f;
      if (has(player, Genus::Cerebrian, InfusionTree::Defense, EXALTED)) {
        // Psychic Link: the crew nearby gets half a barrier.
        forAllies(sys, player, 250.0f, [&](Player& ally) {
          if (ally.shield >= barrier * 0.5f) return;
          sys.fxBeam(player.x, player.y, ally.x, ally.y, 6.0f, 200.0f,
                     VfxTheme::Psychic);
          sys.grantShield(ally, barrier * 0.5f, 600000.0f);
        });
      }
    }
  }
  if (has(player, Genus::Venoxarid, InfusionTree::Defense, REFINED)) {
    // Corrosion: poisoned enemies near you are Weakened.
    float& pulse = player.gradeTimer[DEFENSE][VENOXARID];
    pulse -= dtMs;
    if (pulse <= 0.0f) {
      pulse = 500.0f;
      sys.enemiesInCircle(player.x, player.y, 200.0f, [&](Enemy& enemy) {
        if (sys.effects().getEnemyEffects(enemy.id).hasEffect(
                EffectType::Poison)) {
          sys.applyToEnemy(enemy, EffectType::Weakened, 1, playerId, 1000.0f);
        }
      });
    }
  }
  if (has(player, Genus::Frigidariant, InfusionTree::Defense, REFINED)) {
    // Permafrost: a standing chill around you. (Its pulse uses the unused
    // genus-None column; Cryo Sanctuary owns the Frigidariant one.)
    float& pulse = player.gradeTimer[DEFENSE][0];
    pulse -= dtMs;
    if (pulse <= 0.0f) {
      pulse = 400.0f;
      sys.enemiesInCircle(player.x, player.y, 120.0f, [&](Enemy& enemy) {
        sys.applyToEnemy(enemy, EffectType::Slow, 1, playerId, 700.0f);
      });
    }
  }
  if (level(player, Genus::Magnarok, InfusionTree::Defense) > 0) {
    // Slow Burn: drain the deferred damage. It can hurt but never kill.
    float& pool = player.infusionTimer[DEFENSE][MAGNAROK];
    if (pool > 0.0f) {
      float drain = std::min(pool, (pool * 0.5f + 4.0f) * dtMs / 1000.0f);
      pool -= drain;
      player.health = std::max(1.0f, player.health - drain);
    }
  }

  // ---- Mobility -----------------------------------------------------------
  bool moving = std::abs(player.vx) + std::abs(player.vy) > 1.0f;

  if (has(player, Genus::Frigidariant, InfusionTree::Offense, EXALTED)) {
    // Glacier: time standing still; planted, you are Fortified.
    float& still = player.gradeTimer[OFFENSE][FRIGIDARIANT];
    bool wasPlanted = still >= GLACIER_STILL_MS;
    still = moving ? 0.0f : still + dtMs;
    if (still >= GLACIER_STILL_MS) {
      if (!wasPlanted) sys.fxRing(player.x, player.y, 50.0f, VfxTheme::Ice);
      // Fortified is 10% a stack: four is Glacier's 40%.
      sys.applyToPlayer(player, EffectType::Fortified, 4, playerId, 300.0f);
    }
  }
  if (has(player, Genus::Venoxarid, InfusionTree::Mobility, REFINED)) {
    // Toxic Wake: running past enemies poisons them, once a second.
    float& pulse = player.gradeTimer[MOBILITY][VENOXARID];
    pulse -= dtMs;
    if (moving && pulse <= 0.0f) {
      int touched = 0;
      sys.enemiesInCircle(player.x, player.y, 60.0f, [&](Enemy& enemy) {
        sys.applyToEnemy(enemy, EffectType::Poison, 1, playerId);
        touched++;
      });
      if (touched > 0) pulse = 1000.0f;
    }
  }
  if (swarmRiding(player)) {
    sys.applyToPlayer(player, EffectType::Unbounded, 1, playerId, 300.0f);
  }
  if (has(player, Genus::Cerebrian, InfusionTree::Mobility, EXALTED)) {
    // Phantom: touching enemies hurts them.
    float& pulse = player.gradeTimer[MOBILITY][CEREBRIAN];
    pulse -= dtMs;
    if (pulse <= 0.0f) {
      pulse = 500.0f;
      sys.enemiesInCircle(
          player.x, player.y, Arena::Tuning::PLAYER_RADIUS + 12.0f,
          [&](Enemy& enemy) {
            sys.fxRing(enemy.x, enemy.y, 28.0f, VfxTheme::Psychic);
            sys.damageEnemy(playerId, enemy, 12.0f, DamageFlag::Proc);
          });
    }
  }
  if (has(player, Genus::Frigidariant, InfusionTree::Mobility, REFINED)) {
    int& flags = player.gradeCounter[MOBILITY][FRIGIDARIANT];
    // Ice Skate: on your own ice you are fast and cannot be slowed.
    bool onIce = inOwnZone(sys, player, VfxTheme::Ice, 1);
    flags = onIce ? (flags | ON_ICE) : (flags & ~ON_ICE);
    if (onIce) {
      sys.applyToPlayer(player, EffectType::Unbounded, 1, playerId, 300.0f);
    }
    if (has(player, Genus::Frigidariant, InfusionTree::Mobility, EXALTED) &&
        !(flags & MOMENTUM_READY)) {
      // Avalanche: two seconds on the move builds momentum.
      float& moved = player.gradeTimer[MOBILITY][FRIGIDARIANT];
      moved = moving ? moved + dtMs : 0.0f;
      if (moved >= AVALANCHE_MOVE_MS) {
        flags |= MOMENTUM_READY;
        sys.fxRing(player.x, player.y, 60.0f, VfxTheme::Ice);
      }
    }
  }
  if (has(player, Genus::Frigidariant, InfusionTree::Genealogy, EXALTED) &&
      player.shield > 0.0f && player.shieldMs > 0.0f) {
    // Diamond Heart: shields hold while something frozen is near.
    int frozen =
        sys.countEnemies(player.x, player.y, 300.0f, [&](const Enemy& enemy) {
          return sys.effects().getEnemyEffects(enemy.id).hasEffect(
              EffectType::Stunned);
        });
    if (frozen > 0) player.shieldMs = std::max(player.shieldMs, 250.0f);
  }
  if (has(player, Genus::Magnarok, InfusionTree::Offense, EXALTED) &&
      inOwnZone(sys, player, VfxTheme::Fire, 1)) {
    // Inferno Heart: standing on your molten ground Empowers you.
    sys.applyToPlayer(player, EffectType::Empowered, 1, playerId, 300.0f);
  }
  if (has(player, Genus::Venoxarid, InfusionTree::Genealogy, EXALTED)) {
    // Miasma Heart: a small cloud that never leaves you.
    float& pulse = player.gradeTimer[GENEALOGY][VENOXARID];
    pulse -= dtMs;
    if (pulse <= 0.0f) {
      pulse = 1000.0f;
      sys.fxRing(player.x, player.y, 90.0f, VfxTheme::Poison);
      sys.enemiesInCircle(player.x, player.y, 90.0f, [&](Enemy& enemy) {
        sys.applyToEnemy(enemy, EffectType::Poison, 1, playerId);
      });
    }
  }
  if (has(player, Genus::Magnarok, InfusionTree::Mobility, EXALTED)) {
    // Firewalker: among burning enemies, always in Burning Rush.
    float& recount = player.gradeTimer[MOBILITY][MAGNAROK];
    recount -= dtMs;
    if (recount <= 0.0f) {
      recount = 250.0f;
      int burning =
          sys.countEnemies(player.x, player.y, 300.0f, [&](const Enemy& enemy) {
            return sys.effects().getEnemyEffects(enemy.id).hasEffect(
                EffectType::Wound);
          });
      if (burning >= 3) {
        float& rushLeft = player.infusionTimer[MOBILITY][MAGNAROK];
        rushLeft = std::max(rushLeft, 400.0f);
      }
    }
  }

  if (level(player, Genus::Venoxarid, InfusionTree::Mobility) > 0) {
    // Speed Siphon: recount poisoned enemies a few times per second.
    float& recount = player.infusionTimer[MOBILITY][VENOXARID];
    recount -= dtMs;
    if (recount <= 0.0f) {
      recount = 250.0f;
      int poisoned =
          sys.countEnemies(player.x, player.y, 420.0f, [&](const Enemy& enemy) {
            return sys.effects().getEnemyEffects(enemy.id).hasEffect(
                EffectType::Poison);
          });
      player.infusionCounter[MOBILITY][VENOXARID] = std::min(poisoned, 8);
    }
  }
  int frost = level(player, Genus::Frigidariant, InfusionTree::Mobility);
  if (frost > 0) {
    // Glacial Path: drop ice behind the player while moving.
    float& drop = player.infusionTimer[MOBILITY][FRIGIDARIANT];
    drop -= dtMs;
    if (moving && drop <= 0.0f) {
      drop = 220.0f;
      AbilitySystem::Zone ice;
      ice.ownerId = playerId;
      ice.x = player.x;
      ice.y = player.y;
      ice.radius = 42.0f;
      ice.lifeMs = 2400.0f + 500.0f * frost;
      ice.tickMs = 450.0f;
      ice.theme = VfxTheme::Ice;
      ice.variant = 1;
      ice.onEnemyTick = [s, playerId, frost](AbilitySystem::Zone&,
                                             Enemy& enemy) {
        s->applyToEnemy(enemy, EffectType::Slow, frost, playerId);
      };
      sys.spawnZone(ice);
    }
  }
  int molten = level(player, Genus::Magnarok, InfusionTree::Mobility);
  float& rush = player.infusionTimer[MOBILITY][MAGNAROK];
  if (molten > 0 && rush > 0.0f) {
    // Burning Rush: the timer is the time left; patches drop every 200 ms.
    float before = rush;
    rush = std::max(0.0f, before - dtMs);
    bool crossed =
        static_cast<int>(before / 200.0f) != static_cast<int>(rush / 200.0f);
    if (moving && crossed) {
      spawnMoltenPatch(sys, playerId, player.x, player.y, 44.0f, 2500.0f,
                       5.0f * molten);
    }
  }
}

float speedMultiplier(const Player& player) {
  float mult = 1.0f + partTotal(player, &Arena::InfusionPartInfo::speedBonus);
  if (player.hasTalent(Talent::LightFrame)) mult += 0.06f;
  if (player.hasTalent(Talent::HitAndRun) &&
      player.talentTimer[t(Talent::HitAndRun)] > 0.0f) {
    mult += 0.20f;
  }
  int siphon = level(player, Genus::Venoxarid, InfusionTree::Mobility);
  if (siphon > 0) {
    mult *= 1.0f + 0.04f * siphon * player.infusionCounter[MOBILITY][VENOXARID];
  }
  if (level(player, Genus::Magnarok, InfusionTree::Mobility) > 0 &&
      player.infusionTimer[MOBILITY][MAGNAROK] > 0.0f) {
    mult *= 1.4f;
  }
  if (player.gradeCounter[MOBILITY][FRIGIDARIANT] & ON_ICE) {
    mult *= 1.2f;  // Ice Skate
  }
  return mult;
}

bool blocksCrowdControl(const Player& player) {
  if (player.coreGenus == Genus::Frigidariant && player.coreTimerMs > 0.0f) {
    return true;  // nothing moves a diamond
  }
  return level(player, Genus::Cerebrian, InfusionTree::Defense) > 0 &&
         player.shield > 0.0f;
}

}  // namespace Infusions
