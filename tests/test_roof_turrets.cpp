// The ship's roof turrets, hull and tiers, on the real server systems:
// repairing a Derelict Turret stands one up; it swivels to the nearest enemy
// in range, hits hard, and a kill goes to whoever last hit the enemy; the
// hull is solid, with raiders chewing on its nearest face; and banked scrap
// buys the ship's next tier between rounds.

#include <cassert>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "AbilitySystem.h"
#include "ArenaData.h"
#include "CollisionSystem.h"
#include "EffectManager.h"
#include "EnemySystem.h"
#include "Logger.h"
#include "MatchDirector.h"

namespace {

constexpr float TICK_MS = 1000.0f / 60.0f;
constexpr uint32_t HERO = 1;
namespace T = Arena::Tuning;

struct World {
  std::unordered_map<uint32_t, Player> players;
  std::vector<EnemySpawn> noSpawns;
  EnemySystem enemies{noSpawns};
  EffectManager effects;
  std::vector<GameEventPacket> events;
  AbilitySystem combat;
  MatchDirector director;
  std::vector<EnemySystem::EnemyDeath> deaths;

  std::vector<int> tiersSet;

  explicit World(const CollisionSystem* solid = nullptr)
      : combat(players, &enemies, &effects, solid, 8000.0f, 4000.0f,
               [this](const GameEventPacket& p) { events.push_back(p); }),
        director(players, &enemies, &combat, 0.0f, 0.0f, 8000.0f, 4000.0f,
                 hooks()) {
    enemies.setCombat(&combat);
    Player hero;
    hero.id = HERO;
    players[HERO] = hero;
    director.onPlayerJoined(players[HERO]);
    director.onCharacterSelected(players[HERO], Arena::CharacterId::Wade);
    director.onReady(players[HERO], true);
    run(Arena::Intro::LENGTH * 1000.0f + 500.0f);
    assert(director.phase() == Arena::MatchPhase::Round);
    // Out of the way of everything below.
    players[HERO].x = 3000.0f;
    players[HERO].y = 3000.0f;
  }

  MatchDirector::Hooks hooks() {
    MatchDirector::Hooks h;
    h.setShipTier = [this](int tier) { tiersSet.push_back(tier); };
    return h;
  }
  Player& hero() { return players[HERO]; }
  int tier() const { return director.shipTier(); }
  float mountX(int i) const { return T::ROOF_TURRET_MOUNT_X[tier()][i]; }
  float mountY(int i) const { return T::ROOF_TURRET_MOUNT_Y[tier()][i]; }
  void run(float ms, bool moveEnemies = false) {
    for (float t = 0.0f; t < ms; t += TICK_MS) {
      combat.update(TICK_MS);
      if (moveEnemies) enemies.update(TICK_MS, players, &effects);
      director.update(TICK_MS);
      for (const auto& death : enemies.takeDeaths()) {
        deaths.push_back(death);
        if (const Enemy* e = enemies.find(death.enemyId)) {
          director.onEnemyKilled(*e, death.killerId);
        }
      }
    }
  }
};

void turretsStandUpOneRepairEach() {
  World w;
  assert(w.director.buildState().roofTurrets == 0);
  assert(w.director.onTurretRepaired(w.hero()));
  assert(w.director.buildState().roofTurrets == 0b01);
  assert(w.director.onTurretRepaired(w.hero()));
  assert(w.director.buildState().roofTurrets == 0b11);
  // Both are up: a third repair stands nothing more.
  assert(!w.director.onTurretRepaired(w.hero()));
  std::cout << "  repairs: one roof turret each, two at most\n";
}

void turretSwivelsAndKillCreditGoesToTheLastHitter() {
  World w;
  w.director.onTurretRepaired(w.hero());
  // A grunt due east of turret 0, in range. The hero tagged it once.
  uint32_t id = w.enemies.spawn(Arena::EnemyKind::VenoxaridGrunt,
                                w.mountX(0) + 300.0f, w.mountY(0));
  Enemy* grunt = w.enemies.find(id);
  assert(grunt && grunt->health <= T::ROOF_TURRET_DAMAGE[0]);
  w.combat.damageEnemy(HERO, *grunt, 1.0f, 0);
  int killsBefore = w.hero().kills;

  w.run(2500.0f);
  assert(w.deaths.size() == 1 && w.deaths[0].enemyId == id);
  assert(w.deaths[0].killerId == HERO);
  assert(w.hero().kills == killsBefore + 1);
  // It turned to face east (a quarter turn clockwise from straight down).
  uint8_t aim = w.director.buildState().roofAim[0];
  assert(std::abs(static_cast<int>(aim) - 64) <= 9);  // fires within 12 deg
  std::cout << "  turret: swivels east, one shot, the kill is the hero's\n";
}

void untouchedKillsGoToNoOne() {
  World w;
  w.director.onTurretRepaired(w.hero());
  uint32_t id = w.enemies.spawn(Arena::EnemyKind::VenoxaridGrunt,
                                w.mountX(0) - 200.0f, w.mountY(0) + 100.0f);
  int killsBefore = w.hero().kills;
  w.run(2500.0f);
  assert(w.deaths.size() == 1 && w.deaths[0].enemyId == id);
  assert(w.deaths[0].killerId == 0);
  assert(w.hero().kills == killsBefore);
  std::cout << "  turret: a kill nobody earned goes to nobody\n";
}

void outOfRangeIsLeftAlone() {
  World w;
  w.director.onTurretRepaired(w.hero());
  w.director.onTurretRepaired(w.hero());
  uint32_t id = w.enemies.spawn(Arena::EnemyKind::VenoxaridGrunt,
                                T::ROOF_TURRET_RANGE[0] + 400.0f, 0.0f);
  float health = w.enemies.find(id)->health;
  w.run(3000.0f);
  assert(w.deaths.empty());
  assert(w.enemies.find(id)->health == health);
  std::cout << "  turret: holds fire beyond its range\n";
}

void hullIsSolidAndRaidersChewOnIt() {
  // A hull box north of the ship's centre, as on arena.tmx.
  CollisionShape hull;
  hull.type = CollisionShape::Type::Rectangle;
  hull.aabb = {-150.0f, -250.0f, 300.0f, 200.0f};  // y -250 .. -50
  hull.name = "ShipHull_1";
  hull.objectType = "hull";
  std::vector<CollisionShape> shapes = {hull};
  CollisionSystem solid(shapes);
  assert(CollisionSystem::hullTierOf(hull) == 0 &&
         !CollisionSystem::isTerrain(hull));

  // Players stop at it.
  float x = 0.0f, y = -100.0f;
  solid.checkMovement(0.0f, 40.0f, x, y);
  assert(y > -50.0f + 15.0f);

  World w(&solid);
  float before = w.director.shipHealth();
  // A raider coming from the far (north) side of the hull.
  EnemySystem::SpawnOptions raid;
  raid.targetShip = true;
  uint32_t id =
      w.enemies.spawn(Arena::EnemyKind::VenoxaridGrunt, 0.0f, -420.0f, raid);
  bool everInside = false;
  for (int i = 0; i < 360; i++) {
    w.run(TICK_MS * 1.01f, true);
    const Enemy* e = w.enemies.find(id);
    if (!e || e->state == EnemyState::Dead) break;
    everInside =
        everInside || (e->x > -150.0f + 10.0f && e->x < 150.0f - 10.0f &&
                       e->y > -250.0f + 10.0f && e->y < -50.0f - 10.0f);
  }
  assert(!everInside);
  // It reached the hull's north face and hurt the ship from there.
  assert(w.director.shipHealth() < before);
  std::cout << "  hull: solid to players and raiders; raiders hit its face\n";
}

void hullFootprintFollowsTheTier() {
  auto box = [](const char* type, float x) {
    CollisionShape s;
    s.type = CollisionShape::Type::Rectangle;
    s.aabb = {x, 0.0f, 50.0f, 50.0f};
    s.objectType = type;
    return s;
  };
  std::vector<CollisionShape> shapes = {box("hull", 0.0f), box("hull1", 200.0f),
                                        box("hull2", 400.0f),
                                        box("terrain", 600.0f)};
  CollisionSystem solid(shapes);
  assert(CollisionSystem::hullTierOf(shapes[2]) == 2);
  assert(CollisionSystem::hullTierOf(shapes[3]) == -1);
  for (int tier = 0; tier < T::SHIP_TIER_COUNT; tier++) {
    solid.setHullTier(tier);
    for (int i = 0; i < 3; i++) {
      bool blocked = !solid.isPositionValid(shapes[i].aabb.x + 25.0f, 25.0f);
      assert(blocked == (i == tier));
    }
    assert(!solid.isPositionValid(625.0f, 25.0f));  // terrain always
  }
  std::cout << "  hull: only the current tier's footprint is solid\n";
}

void scrapBuysTiersBetweenRounds() {
  World w;
  w.hero().x = 0.0f;
  w.hero().y = 150.0f;  // home for the night
  assert(w.tier() == 0);
  assert(w.director.shipHealth() == T::SHIP_START_HEALTH);
  // The playtest cheat banks enough scrap for both tiers.
  w.director.onDebugCommand(w.hero(), DebugCommandKind::RoofTurrets, 0);
  int bank = w.director.scrapInBank();
  assert(bank == T::SHIP_TIER_COST[1] + T::SHIP_TIER_COST[2]);

  // Not mid-round: upgrades are a between-rounds decision.
  assert(!w.director.onShipUpgrade(w.hero()));
  assert(w.tier() == 0);

  w.director.onDebugCommand(w.hero(), DebugCommandKind::SkipPhase, 0);
  w.run(100.0f);
  assert(w.director.phase() == Arena::MatchPhase::Intermission);
  assert(w.director.onShipUpgrade(w.hero()));
  assert(w.tier() == 1 &&
         w.director.scrapInBank() == bank - T::SHIP_TIER_COST[1]);
  // A full hull at the new tier's maximum, and the footprint changes.
  assert(w.director.shipHealth() == T::SHIP_TIER_MAX_HEALTH[1]);
  assert(w.director.buildState().shipTier == 1);
  assert(w.director.buildState().shipMaxHealth == T::SHIP_TIER_MAX_HEALTH[1]);
  assert(w.director.safeRadius() == T::SHIP_TIER_SAFE_RADIUS[1]);
  assert(!w.tiersSet.empty() && w.tiersSet.back() == 1);

  assert(w.director.onShipUpgrade(w.hero()));
  assert(w.tier() == 2 && w.director.scrapInBank() == 0);
  assert(w.director.shipHealth() == T::SHIP_TIER_MAX_HEALTH[2]);
  // The top tier, and nothing left to spend.
  assert(!w.director.onShipUpgrade(w.hero()));
  std::cout << "  tiers: bought at intermission, full repair, new footprint\n";
}

void notEnoughScrapBuysNothing() {
  World w;
  w.hero().x = 0.0f;
  w.hero().y = 150.0f;
  // Hauled home: one short of the first tier.
  w.director.addScrap(w.hero(), T::SHIP_TIER_COST[1] - 1);
  w.run(100.0f);
  assert(w.hero().scrapCarried == 0);
  assert(w.director.scrapInBank() == T::SHIP_TIER_COST[1] - 1);
  w.director.onDebugCommand(w.hero(), DebugCommandKind::SkipPhase, 0);
  w.run(100.0f);
  assert(w.director.phase() == Arena::MatchPhase::Intermission);
  assert(!w.director.onShipUpgrade(w.hero()));
  assert(w.tier() == 0 && w.tiersSet.empty());
  std::cout << "  tiers: short of the price, nothing is spent\n";
}

void repairNoiseDrawsAPack() {
  World w;
  int before = 0;
  for (const auto& [id, e] : w.enemies.getEnemies()) {
    if (e.state != EnemyState::Dead) before++;
  }
  w.director.onRepairNoise(300.0f, 300.0f);
  int after = 0;
  for (const auto& [id, e] : w.enemies.getEnemies()) {
    if (e.state != EnemyState::Dead) after++;
  }
  assert(after > before);
  std::cout << "  repair: the noise draws a pack\n";
}

void strongerTurretsAtHigherTiers() {
  for (int t = 1; t < T::SHIP_TIER_COUNT; t++) {
    assert(T::ROOF_TURRET_DAMAGE[t] > T::ROOF_TURRET_DAMAGE[t - 1]);
    assert(T::ROOF_TURRET_RANGE[t] > T::ROOF_TURRET_RANGE[t - 1]);
    assert(T::SHIP_TIER_MAX_HEALTH[t] > T::SHIP_TIER_MAX_HEALTH[t - 1]);
    assert(T::SHIP_TIER_SAFE_RADIUS[t] > T::SHIP_TIER_SAFE_RADIUS[t - 1]);
  }
  std::cout << "  tiers: every tier is tougher, wider and hits harder\n";
}

}  // namespace

int main() {
  Logger::init();
  std::cout << "Roof turrets and hull\n";
  turretsStandUpOneRepairEach();
  turretSwivelsAndKillCreditGoesToTheLastHitter();
  untouchedKillsGoToNoOne();
  outOfRangeIsLeftAlone();
  hullIsSolidAndRaidersChewOnIt();
  hullFootprintFollowsTheTier();
  scrapBuysTiersBetweenRounds();
  notEnoughScrapBuysNothing();
  repairNoiseDrawsAPack();
  strongerTurretsAtHigherTiers();
  std::cout << "All roof turret tests passed\n";
  return 0;
}
