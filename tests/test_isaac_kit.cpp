// Isaac's kit on the real combat systems, with terrain on the map: the Ray
// slows and heals but deals no damage (Wingers have no base damage), and
// Lifeline bonds to an ally, a rock or the ship's hull near the cursor,
// heals, and pulls him there on release. With nothing in reach it does nothing
// and most of the cooldown comes back. Enemies slide off terrain.

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

namespace {

constexpr float TICK_MS = 1000.0f / 60.0f;
constexpr uint8_t LMB = 1 << 0, RMB = 1 << 1;
constexpr uint32_t ISAAC_ID = 1;
constexpr uint32_t WADE_ID = 2;

CollisionShape rock(float x, float y, float w, float h,
                    const char* type = "terrain") {
  CollisionShape s;
  s.type = CollisionShape::Type::Rectangle;
  s.aabb = {x, y, w, h};
  s.name = "Rock";
  s.objectType = type;
  return s;
}

struct World {
  // A rock 400 to the east, in reach; another 1100 east, out of reach. The
  // ship's hull 320 to the north (tier 0) and, for its next tier, 320 to
  // the south.
  std::vector<CollisionShape> shapes{
      rock(400.0f, -40.0f, 80.0f, 80.0f), rock(1100.0f, -40.0f, 80.0f, 80.0f),
      rock(-40.0f, -400.0f, 80.0f, 80.0f, "hull"),
      rock(-40.0f, 320.0f, 80.0f, 80.0f, "hull1")};
  CollisionSystem solid{shapes};
  std::unordered_map<uint32_t, Player> players;
  std::vector<EnemySpawn> noSpawns;
  EnemySystem enemies{noSpawns};
  EffectManager effects;
  std::vector<GameEventPacket> events;
  AbilitySystem combat{
      players,
      &enemies,
      &effects,
      &solid,
      8000.0f,
      4000.0f,
      [this](const GameEventPacket& p) { events.push_back(p); }};
  float aimX = 100.0f, aimY = 0.0f;

  World() {
    enemies.setCombat(&combat);
    Player isaac;
    isaac.id = ISAAC_ID;
    isaac.characterId = Arena::CharacterId::Isaac;
    players[ISAAC_ID] = isaac;
    combat.applyKitStats(players[ISAAC_ID]);
  }

  Player& isaac() { return players[ISAAC_ID]; }
  Player& wade() { return players[WADE_ID]; }

  void addWade(float x, float y) {
    Player wade;
    wade.id = WADE_ID;
    wade.characterId = Arena::CharacterId::Wade;
    wade.x = x;
    wade.y = y;
    players[WADE_ID] = wade;
    combat.applyKitStats(players[WADE_ID]);
  }

  uint32_t foe(float x, float y) {
    uint32_t id = enemies.spawn(Arena::EnemyKind::VenoxaridGrunt, x, y);
    Enemy& e = enemies.getEnemies().at(id);
    e.health = e.maxHealth = 5000.0f;
    return id;
  }
  Enemy& enemy(uint32_t id) { return enemies.getEnemies().at(id); }

  void run(float ms, uint8_t buttons, bool moveEnemies = false) {
    for (float t = 0.0f; t < ms; t += TICK_MS) {
      combat.setInput(isaac(), buttons, aimX, aimY);
      combat.update(TICK_MS);
      if (moveEnemies) enemies.update(TICK_MS, players, &effects);
    }
  }
  void tap(uint8_t button) {
    run(TICK_MS, button);
    run(TICK_MS, 0);
  }
};

void test_Ray_SlowsAndHealsButDealsNoDamage() {
  World w;
  w.addWade(-60.0f, 0.0f);
  w.wade().health = 50.0f;
  uint32_t a = w.foe(100.0f, 0.0f);
  w.tap(LMB);
  w.run(200.0f, 0);
  assert(w.enemy(a).health == 5000.0f);
  assert(w.effects.getEnemyEffects(a).getStacks(EffectType::Slow) == 1);
  assert(w.wade().health > 50.0f);
}

void test_Lifeline_BondsToTheRockAndPullsToItsFace() {
  World w;
  w.isaac().health = 50.0f;
  w.aimX = 440.0f;  // on the rock
  w.run(1000.0f, RMB);
  assert(w.isaac().health > 50.0f);  // the bond heals him
  w.run(700.0f, 0);                  // release: pulled across
  assert(w.isaac().x > 300.0f);
  // Stopped against the rock, not inside it.
  assert(w.solid.isPositionValid(w.isaac().x, w.isaac().y,
                                 Arena::Tuning::PLAYER_RADIUS));
  assert(w.isaac().cooldownMs[1] > 5000.0f);
}

void test_Lifeline_BondsToTheShipHull() {
  World w;
  w.isaac().health = 50.0f;
  w.aimX = 0.0f;
  w.aimY = -360.0f;  // on the ship
  w.run(1000.0f, RMB);
  assert(w.isaac().health > 50.0f);
  w.run(700.0f, 0);  // pulled to the hull's face
  assert(w.isaac().y < -200.0f);
  assert(w.solid.isPositionValid(w.isaac().x, w.isaac().y,
                                 Arena::Tuning::PLAYER_RADIUS));
}

void test_Lifeline_OnlyTheShipsCurrentTier() {
  // The next tier's footprint is not there yet: nothing to bond to.
  World w;
  w.aimX = 0.0f;
  w.aimY = 360.0f;
  w.run(1000.0f, RMB);
  w.run(700.0f, 0);
  assert(std::abs(w.isaac().y) < 1.0f);
  // Once the ship is upgraded, it is.
  World up;
  up.solid.setHullTier(1);
  up.aimX = 0.0f;
  up.aimY = 360.0f;
  up.run(1000.0f, RMB);
  up.run(700.0f, 0);
  assert(up.isaac().y > 200.0f);
}

void test_Lifeline_TerrainHealsIsaacMoreThanAnAlly() {
  World onRock;
  onRock.isaac().health = 50.0f;
  onRock.aimX = 440.0f;
  onRock.run(1000.0f, RMB);
  float rockHeal = onRock.isaac().health - 50.0f;

  World onAlly;
  onAlly.addWade(200.0f, 200.0f);
  onAlly.isaac().health = 50.0f;
  onAlly.aimX = 200.0f;
  onAlly.aimY = 200.0f;
  onAlly.run(1000.0f, RMB);
  float allyHeal = onAlly.isaac().health - 50.0f;
  assert(rockHeal > allyHeal && allyHeal > 0.0f);
}

void test_Lifeline_BondsToAnAllyAndHealsThemToo() {
  World w;
  w.addWade(0.0f, 300.0f);
  w.wade().health = 50.0f;
  w.aimX = 0.0f;
  w.aimY = 300.0f;
  w.run(1000.0f, RMB);
  assert(w.wade().health > 50.0f);
  w.run(700.0f, 0);
  assert(w.isaac().y > 200.0f);  // pulled to his side
}

void test_Lifeline_NothingInReach_StaysAndRefunds() {
  World w;
  w.isaac().health = 50.0f;
  w.aimX = 0.0f;  // open ground
  w.aimY = 400.0f;
  w.run(1000.0f, RMB);
  assert(w.isaac().health == 50.0f);
  w.run(700.0f, 0);
  assert(std::abs(w.isaac().x) < 1.0f && std::abs(w.isaac().y) < 1.0f);
  assert(w.isaac().cooldownMs[1] <= 1500.0f);
}

void test_Lifeline_RockOutOfReach() {
  World w;
  w.aimX = 1140.0f;  // the far rock: 1100 from Isaac
  w.run(1000.0f, RMB);
  w.run(700.0f, 0);
  assert(std::abs(w.isaac().x) < 1.0f);
  assert(w.isaac().cooldownMs[1] <= 1500.0f);
}

void test_Enemies_SlideOffTerrain() {
  World w;
  uint32_t a = w.foe(440.0f, 0.0f);  // spawned inside the rock
  w.run(100.0f, 0, true);
  assert(w.solid.isPositionValid(w.enemy(a).x, w.enemy(a).y, 15.0f));
}

void test_TerrainQueries() {
  World w;
  const CollisionShape* near = w.solid.terrainNear(380.0f, 0.0f, 50.0f);
  assert(near && near->aabb.x == 400.0f);
  assert(!w.solid.terrainNear(0.0f, 0.0f, 100.0f));
  float x, y;
  CollisionSystem::closestPoint(near->aabb, 0.0f, 10.0f, x, y);
  assert(x == 400.0f && y == 10.0f);
}

}  // namespace

int main() {
  Logger::init();
  test_TerrainQueries();
  test_Ray_SlowsAndHealsButDealsNoDamage();
  test_Lifeline_BondsToTheRockAndPullsToItsFace();
  test_Lifeline_TerrainHealsIsaacMoreThanAnAlly();
  test_Lifeline_BondsToTheShipHull();
  test_Lifeline_OnlyTheShipsCurrentTier();
  test_Lifeline_BondsToAnAllyAndHealsThemToo();
  test_Lifeline_NothingInReach_StaysAndRefunds();
  test_Lifeline_RockOutOfReach();
  test_Enemies_SlideOffTerrain();
  std::cout << "Isaac kit passed\n";
}
