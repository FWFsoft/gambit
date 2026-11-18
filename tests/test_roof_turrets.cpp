// The ship's roof turrets and its hull, on the real server systems:
// repairing a Derelict Turret stands one up; it swivels to the nearest enemy
// in range, hits hard, and a kill goes to whoever last hit the enemy; and
// the hull is solid, with raiders chewing on its nearest face.

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

  explicit World(const CollisionSystem* solid = nullptr)
      : combat(players, &enemies, &effects, solid, 8000.0f, 4000.0f,
               [this](const GameEventPacket& p) { events.push_back(p); }),
        director(players, &enemies, &combat, 0.0f, 0.0f, 8000.0f, 4000.0f,
                 MatchDirector::Hooks{}) {
    enemies.setCombat(&combat);
    Player hero;
    hero.id = HERO;
    players[HERO] = hero;
    director.onPlayerJoined(players[HERO]);
    director.onCharacterSelected(players[HERO], Arena::CharacterId::Wade);
    director.onReady(players[HERO], true);
    run(T::LOBBY_COUNTDOWN_SECONDS * 1000.0f + 500.0f);
    assert(director.phase() == Arena::MatchPhase::Round);
    // Out of the way of everything below.
    players[HERO].x = 3000.0f;
    players[HERO].y = 3000.0f;
  }

  Player& hero() { return players[HERO]; }
  float mountX(int i) const { return T::ROOF_TURRET_MOUNT_X[i]; }
  float mountY(int i) const { return T::ROOF_TURRET_MOUNT_Y[i]; }
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
  assert(grunt && grunt->health <= T::ROOF_TURRET_DAMAGE);
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
                                T::ROOF_TURRET_RANGE + 400.0f, 0.0f);
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
  assert(CollisionSystem::isHull(hull) && !CollisionSystem::isTerrain(hull));

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

}  // namespace

int main() {
  Logger::init();
  std::cout << "Roof turrets and hull\n";
  turretsStandUpOneRepairEach();
  turretSwivelsAndKillCreditGoesToTheLastHitter();
  untouchedKillsGoToNoOne();
  outOfRangeIsLeftAlone();
  hullIsSolidAndRaidersChewOnIt();
  std::cout << "All roof turret tests passed\n";
  return 0;
}
