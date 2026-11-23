// Suit infusions: the item / core / tiered-tree rules, and a few effects on
// the real combat systems.

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "AbilitySystem.h"
#include "ArenaData.h"
#include "EffectManager.h"
#include "EnemySystem.h"
#include "Infusions.h"
#include "Logger.h"

// Always evaluated, also in builds where CHECK() is compiled out.
#define CHECK(cond)                                                \
  do {                                                             \
    if (!(cond)) {                                                 \
      std::cerr << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
      std::abort();                                                \
    }                                                              \
  } while (0)

namespace {

using Arena::Genus;
using Arena::InfusionPart;
using Arena::InfusionTree;
using Arena::NodeState;
using Arena::Talent;

constexpr uint32_t P1 = 1, P2 = 2;

struct World {
  std::unordered_map<uint32_t, Player> players;
  std::vector<EnemySpawn> noSpawns;
  EnemySystem enemies{noSpawns};
  EffectManager effects;
  AbilitySystem combat{players,
                       &enemies,
                       &effects,
                       nullptr,
                       8000.0f,
                       4000.0f,
                       [](const GameEventPacket &) {}};

  World() {
    enemies.setCombat(&combat);
    for (uint32_t id : {P1, P2}) {
      Player p;
      p.id = id;
      p.characterId = Arena::CharacterId::Wade;
      p.x = id == P1 ? 0.0f : 60.0f;
      p.y = 0.0f;
      players[id] = p;
      combat.applyKitStats(players[id]);
    }
  }
  Player &p(uint32_t id) { return players[id]; }

  Enemy &spawnEnemy(float x, float y) {
    uint32_t id = enemies.spawn(Arena::EnemyKind::VenoxaridGrunt, x, y);
    Enemy &enemy = enemies.getEnemies().at(id);
    enemy.maxHealth = enemy.health = 1000.0f;
    return enemy;
  }
};

int node(Talent talent) { return static_cast<int>(talent); }

// Pick up and slot one item; asserts the slot is accepted.
void slot(Player &player, Genus genus, InfusionPart part, int where) {
  bool added = Infusions::addToInventory(player, genus, part);
  bool ok = added && Infusions::slotInfusion(player, genus, part, where);
  if (!ok) {
    std::cerr << "slot refused: node " << where << "\n";
    std::abort();
  }
}

// Depth is paced by the match: on the first night a tree stops at tier 2.
void testTierCapByRound() {
  World w;
  Player &me = w.p(P1);
  CHECK(Arena::suitTierCap(1) == 2 && Arena::suitTierCap(2) == 3 &&
        Arena::suitTierCap(3) == 4 && Arena::suitTierCap(4) == 4);
  slot(me, Genus::Venoxarid, InfusionPart::Talon, Arena::CORE_NODE);
  slot(me, Genus::Venoxarid, InfusionPart::Talon, node(Talent::KillerInstinct));
  slot(me, Genus::Venoxarid, InfusionPart::Talon, node(Talent::Relentless));
  const int t3 = node(Talent::Affliction);
  CHECK(Arena::nodeState(me.nodeGenus, true, t3, Arena::suitTierCap(1)) ==
        NodeState::Later);
  CHECK(Infusions::addToInventory(me, Genus::Venoxarid, InfusionPart::Sinew));
  CHECK(!Infusions::slotInfusion(me, Genus::Venoxarid, InfusionPart::Sinew, t3,
                                 Arena::suitTierCap(1)));
  CHECK(Infusions::slotInfusion(me, Genus::Venoxarid, InfusionPart::Sinew, t3,
                                Arena::suitTierCap(2)));
  std::cout << "  tier cap by round ok\n";
}

void testLayout() {
  // Tier 1 is one leg per tree; every later tier is a real choice; tiers
  // run 1..N with no gaps; four tiers in each tree for now.
  for (int t = 0; t < Arena::TREE_COUNT; t++) {
    InfusionTree tree = static_cast<InfusionTree>(t);
    int tiers = Arena::treeTierCount(tree);
    CHECK(tiers == 4);
    for (int tier = 1; tier <= tiers; tier++) {
      int options = 0;
      for (int i = 0; i < Arena::suitNodeCount(); i++) {
        const Arena::SuitNode &n = Arena::suitNode(i);
        if (n.tree == tree && n.tier == tier) options++;
      }
      CHECK(tier == 1 ? options == 1 : options >= 2);
    }
  }
  for (int i = 0; i < Arena::suitNodeCount(); i++) {
    CHECK(static_cast<int>(Arena::suitNode(i).talent) == i);
  }
  // Pickup ids carry genus and part.
  for (int g = 1; g <= Arena::GENUS_COUNT; g++) {
    for (int p = 0; p < Arena::PART_COUNT; p++) {
      uint32_t id = Arena::PickupId::infusion(static_cast<Genus>(g),
                                              static_cast<InfusionPart>(p));
      CHECK(Arena::PickupId::isInfusion(id));
      CHECK(Arena::PickupId::infusionGenus(id) == static_cast<Genus>(g));
      CHECK(Arena::PickupId::infusionPart(id) == static_cast<InfusionPart>(p));
    }
  }
  CHECK(!Arena::PickupId::isInfusion(Arena::PickupId::scrap(1)));
  CHECK(!Arena::PickupId::isInfusion(
      Arena::PickupId::scrap(Arena::PickupId::SCRAP_PICKUP_MAX)));
  CHECK(Arena::PickupId::isScrap(Arena::PickupId::scrap(7)));
  CHECK(Arena::PickupId::scrapAmount(Arena::PickupId::scrap(7)) == 7);
  CHECK(!Arena::PickupId::isScrap(Arena::PickupId::Medkit));
  CHECK(!Arena::PickupId::isInfusion(Arena::PickupId::Medkit));
  CHECK(!Arena::PickupId::isInfusion(Arena::PickupId::InfusionBase));
  std::cout << "  layout ok\n";
}

void testSlotRules() {
  World w;
  Player &me = w.p(P1);
  const int t1 = node(Talent::KillerInstinct);
  const int t2a = node(Talent::OpeningStrike);
  const int t2b = node(Talent::Relentless);
  const int t3 = node(Talent::Affliction);

  // Nothing in the pack: nothing to slot.
  CHECK(!Infusions::slotInfusion(me, Genus::Venoxarid, InfusionPart::Talon,
                                 Arena::CORE_NODE));
  // Trees stay locked until the core is filled.
  CHECK(Infusions::addToInventory(me, Genus::Venoxarid, InfusionPart::Talon));
  CHECK(Arena::nodeState(me.nodeGenus, false, t1) == NodeState::Locked);
  CHECK(
      !Infusions::slotInfusion(me, Genus::Venoxarid, InfusionPart::Talon, t1));
  CHECK(me.infusionInventory[0][0] == 1);
  CHECK(Infusions::slotInfusion(me, Genus::Venoxarid, InfusionPart::Talon,
                                Arena::CORE_NODE));
  CHECK(me.coreGenus == Genus::Venoxarid);
  CHECK(me.infusionInventory[0][0] == 0);
  // The core is permanent.
  CHECK(Infusions::addToInventory(me, Genus::Magnarok, InfusionPart::Eye));
  CHECK(!Infusions::slotInfusion(me, Genus::Magnarok, InfusionPart::Eye,
                                 Arena::CORE_NODE));

  // Tier 2 waits for tier 1.
  CHECK(Arena::nodeState(me.nodeGenus, true, t2a) == NodeState::Locked);
  CHECK(!Infusions::slotInfusion(me, Genus::Magnarok, InfusionPart::Eye, t2a));
  CHECK(Arena::nodeState(me.nodeGenus, true, t1) == NodeState::Open);
  CHECK(Infusions::slotInfusion(me, Genus::Magnarok, InfusionPart::Eye, t1));
  CHECK(!Infusions::slotInfusion(me, Genus::Magnarok, InfusionPart::Eye, t1));

  // Tier 2 is a choice: taking one closes the other.
  slot(me, Genus::Venoxarid, InfusionPart::Hide, t2b);
  CHECK(Arena::nodeState(me.nodeGenus, true, t2a) == NodeState::Closed);
  CHECK(Infusions::addToInventory(me, Genus::Venoxarid, InfusionPart::Hide));
  CHECK(
      !Infusions::slotInfusion(me, Genus::Venoxarid, InfusionPart::Hide, t2a));
  CHECK(Arena::nodeState(me.nodeGenus, true, t3) == NodeState::Open);


  // Other trees are untouched by this one.
  CHECK(Arena::nodeState(me.nodeGenus, true, node(Talent::Plating)) ==
        NodeState::Open);
  CHECK(Arena::nodeState(me.nodeGenus, true, node(Talent::SecondWind)) ==
        NodeState::Locked);

  // Levels count infusions of a genus per tree; genera can share a tree.
  CHECK(me.infusionLevel(Genus::Magnarok, InfusionTree::Offense) == 1);
  CHECK(me.infusionLevel(Genus::Venoxarid, InfusionTree::Offense) == 1);
  CHECK(me.infusionLevel(Genus::Venoxarid, InfusionTree::Defense) == 0);
  slot(me, Genus::Venoxarid, InfusionPart::Sinew, t3);
  CHECK(me.infusionLevel(Genus::Venoxarid, InfusionTree::Offense) == 2);
  CHECK(me.hasTalent(Talent::KillerInstinct));
  CHECK(me.hasTalent(Talent::Relentless));
  CHECK(!me.hasTalent(Talent::OpeningStrike));

  // Bad requests change nothing.
  CHECK(!Infusions::slotInfusion(me, Genus::None, InfusionPart::Hide, t1));
  CHECK(
      !Infusions::slotInfusion(me, Genus::Venoxarid, InfusionPart::Hide, 200));

  // A new match empties everything.
  Infusions::resetLoadout(me);
  CHECK(me.coreGenus == Genus::None && !me.hasTalent(Talent::KillerInstinct));
  CHECK(me.infusionInventory[0][1] == 0);
  CHECK(me.infusionLevel(Genus::Venoxarid, InfusionTree::Offense) == 0);
  std::cout << "  slot rules ok\n";
}

void testItemStats() {
  World w;
  Player &me = w.p(P1);
  float baseHealth = me.maxHealth;
  slot(me, Genus::Cerebrian, InfusionPart::Hide, Arena::CORE_NODE);
  slot(me, Genus::Cerebrian, InfusionPart::Hide, node(Talent::Plating));
  float hide = Arena::partInfo(InfusionPart::Hide).maxHealthBonus;
  CHECK(me.maxHealth == baseHealth + 2 * hide);
  CHECK(me.health == me.maxHealth);
  // Kit stats are re-applied at times (character swap); the bonus survives.
  w.combat.applyKitStats(me);
  CHECK(me.maxHealth == baseHealth + 2 * hide);

  // A Talon adds damage wherever it is slotted.
  slot(me, Genus::Cerebrian, InfusionPart::Talon, node(Talent::KillerInstinct));
  w.combat.applyToPlayer(me, EffectType::Dulled, 1, me.id,
                         100000.0f);  // no crits
  Enemy &enemy = w.spawnEnemy(2000.0f, 2000.0f);
  enemy.health = 500.0f;  // below full: Opening Strike is not in play anyway
  float dealt = w.combat.damageEnemy(me.id, enemy, 100.0f, DamageFlag::Proc);
  float talon = Arena::partInfo(InfusionPart::Talon).damageBonus;
  CHECK(std::fabs(dealt - 100.0f * (1.0f + talon)) < 0.01f);
  std::cout << "  item stats ok\n";
}

void testApexMutation() {
  World w;
  Player &me = w.p(P1);
  slot(me, Genus::Frigidariant, InfusionPart::Eye, Arena::CORE_NODE);
  slot(me, Genus::Frigidariant, InfusionPart::Eye, node(Talent::Bloodline));
  slot(me, Genus::Magnarok, InfusionPart::Eye, node(Talent::Purebred));
  slot(me, Genus::Magnarok, InfusionPart::Eye, node(Talent::Overcharge));
  slot(me, Genus::Venoxarid, InfusionPart::Eye, node(Talent::ApexMutation));
  // Only the core's genus gets the extra level.
  CHECK(me.infusionLevel(Genus::Frigidariant, InfusionTree::Genealogy) == 2);
  CHECK(me.infusionLevel(Genus::Magnarok, InfusionTree::Genealogy) == 2);
  CHECK(me.infusionLevel(Genus::Venoxarid, InfusionTree::Genealogy) == 1);
  // ...and only where it is slotted at all.
  CHECK(me.infusionLevel(Genus::Frigidariant, InfusionTree::Offense) == 0);
  std::cout << "  apex mutation ok\n";
}

void testMixedGeneraInOneTree() {
  // Venoxarid and Cerebrian in Offense both fire on the same hit.
  World w;
  Player &me = w.p(P1);
  slot(me, Genus::Venoxarid, InfusionPart::Talon, Arena::CORE_NODE);
  slot(me, Genus::Venoxarid, InfusionPart::Talon, node(Talent::KillerInstinct));
  slot(me, Genus::Cerebrian, InfusionPart::Talon, node(Talent::OpeningStrike));
  Enemy &target = w.spawnEnemy(100.0f, 0.0f);
  Enemy &other = w.spawnEnemy(130.0f, 0.0f);
  uint32_t targetId = target.id, otherId = other.id;
  w.combat.damageEnemy(me.id, target, 10.0f, DamageFlag::Basic);
  CHECK(w.effects.getEnemyEffects(targetId).hasEffect(EffectType::Poison));
  // The shockwave hit the neighbour (and Toxic Blade's splash poisoned it).
  CHECK(w.enemies.getEnemies().at(otherId).health < 1000.0f);
  CHECK(w.effects.getEnemyEffects(otherId).hasEffect(EffectType::Poison));
  std::cout << "  mixed genera ok\n";
}

void testShareTheLoad() {
  World w;
  Player &tank = w.p(P2);
  slot(tank, Genus::Frigidariant, InfusionPart::Hide, Arena::CORE_NODE);
  slot(tank, Genus::Frigidariant, InfusionPart::Hide, node(Talent::Plating));
  slot(tank, Genus::Frigidariant, InfusionPart::Hide, node(Talent::Regrowth));
  slot(tank, Genus::Frigidariant, InfusionPart::Hide, node(Talent::Thorns));
  slot(tank, Genus::Frigidariant, InfusionPart::Hide,
       node(Talent::ShareTheLoad));
  Player &ally = w.p(P1);
  float allyBefore = ally.health, tankBefore = tank.health;
  w.combat.damagePlayer(ally, 100.0f, 0);
  float allyTook = allyBefore - ally.health;
  float tankTook = tankBefore - tank.health;
  CHECK(std::fabs(allyTook - 75.0f) < 0.01f);
  // The tank's own Plating applies to the share it takes on.
  CHECK(std::fabs(tankTook - 25.0f * 0.92f) < 0.01f);
  // Out of range, nothing is shared.
  ally.x = 2000.0f;
  allyBefore = ally.health;
  w.combat.damagePlayer(ally, 40.0f, 0);
  CHECK(std::fabs((allyBefore - ally.health) - 40.0f) < 0.01f);
  std::cout << "  share the load ok\n";
}

void testLastStand() {
  World w;
  Player &me = w.p(P1);
  slot(me, Genus::Magnarok, InfusionPart::Sinew, Arena::CORE_NODE);
  slot(me, Genus::Venoxarid, InfusionPart::Sinew, node(Talent::Plating));
  slot(me, Genus::Venoxarid, InfusionPart::Sinew, node(Talent::Regrowth));
  slot(me, Genus::Venoxarid, InfusionPart::Sinew, node(Talent::Guardian));
  slot(me, Genus::Venoxarid, InfusionPart::Sinew, node(Talent::LastStand));
  w.combat.damagePlayer(me, 10000.0f, 0);
  CHECK(!me.isDead() && me.health == 1.0f && me.intangibleMs > 0.0f);
  // Once per round.
  me.intangibleMs = 0.0f;
  w.combat.damagePlayer(me, 10000.0f, 0);
  CHECK(me.health <= 0.0f);
  std::cout << "  last stand ok\n";
}

}  // namespace

int main() {
  Logger::init();
  std::cout << "Infusions:\n";
  testLayout();
  testSlotRules();
  testTierCapByRound();
  testItemStats();
  testApexMutation();
  testMixedGeneraInOneTree();
  testShareTheLoad();
  testLastStand();
  std::cout << "All infusion tests passed\n";
  return 0;
}
