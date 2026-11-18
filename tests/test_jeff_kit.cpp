// Jeff's kit (G3F-0427) on the real combat systems: Thermal Disassembly
// strikes the first enemy in line and overheats into a reload; slain
// enemies leave biomass that Acquire Biomass vacuums up for Resonance, with
// a reach that grows with the hold; Resonance speeds him up and raises his
// damage until Reverse of Installation discharges it into a wide piercing
// beam; They See Me Rollin' swaps legs for treads (double speed, +2 Haste,
// no harvesting) and back.

#include <cassert>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "AbilitySystem.h"
#include "ArenaData.h"
#include "EffectManager.h"
#include "EnemySystem.h"
#include "Logger.h"

namespace {

constexpr float TICK_MS = 1000.0f / 60.0f;
constexpr uint8_t LMB = 1 << 0, RMB = 1 << 1, Q = 1 << 2, SHIFT = 1 << 3;
constexpr uint32_t JEFF_ID = 1;
constexpr float CRIT_MAX = 2.0f;  // a hit may crit; never more than this

struct World {
  std::unordered_map<uint32_t, Player> players;
  std::vector<EnemySpawn> noSpawns;
  EnemySystem enemies{noSpawns};
  EffectManager effects;
  std::vector<GameEventPacket> events;
  AbilitySystem combat{
      players,
      &enemies,
      &effects,
      nullptr,
      8000.0f,
      4000.0f,
      [this](const GameEventPacket& p) { events.push_back(p); }};
  float aimX = 300.0f, aimY = 0.0f;

  World() {
    enemies.setCombat(&combat);
    addJeff(JEFF_ID, 0.0f, 0.0f);
  }

  void addJeff(uint32_t id, float x, float y) {
    Player jeff;
    jeff.id = id;
    jeff.characterId = Arena::CharacterId::Jeff;
    jeff.x = x;
    jeff.y = y;
    players[id] = jeff;
    combat.applyKitStats(players[id]);
  }

  Player& jeff() { return players[JEFF_ID]; }

  uint32_t foe(float x, float y, float health = 5000.0f) {
    uint32_t id = enemies.spawn(Arena::EnemyKind::VenoxaridGrunt, x, y);
    Enemy& e = enemies.getEnemies().at(id);
    e.health = e.maxHealth = health;
    e.speed = 0.0f;  // stand still for the test
    return id;
  }
  Enemy& enemy(uint32_t id) { return enemies.getEnemies().at(id); }
  bool alive(uint32_t id) {
    auto it = enemies.getEnemies().find(id);
    return it != enemies.getEnemies().end() &&
           it->second.state != EnemyState::Dead;
  }

  void run(float ms, uint8_t buttons) {
    for (float t = 0.0f; t < ms; t += TICK_MS) {
      combat.setInput(jeff(), buttons, aimX, aimY);
      combat.update(TICK_MS);
      enemies.update(TICK_MS, players, &effects);
      // As the server does (ServerGameState::handleEnemyDeaths).
      for (const auto& death : enemies.takeDeaths()) {
        if (const Enemy* e = enemies.find(death.enemyId)) {
          combat.onEnemyDied(*e, death.killerId);
        }
      }
    }
  }
  void tap(uint8_t button) {
    run(TICK_MS, button);
    run(TICK_MS, 0);
  }
  // Kill an enemy where it stands, leaving its biomass.
  void kill(uint32_t id) {
    combat.damageEnemy(0, enemy(id), 1.0e6f);
    run(TICK_MS, 0);
  }
  uint32_t corpse(float x, float y) {
    uint32_t id = foe(x, y, 10.0f);
    kill(id);
    return id;
  }
  int piles() {
    int n = 0;
    for (const AbilitySystem::Zone& z : combat.allZones()) {
      if (z.variant == Arena::JeffMode::BiomassZone && z.lifeMs > 0.0f) n++;
    }
    return n;
  }
  int resonance() {
    return effects.getPlayerEffects(JEFF_ID).getStacks(EffectType::Resonance);
  }
  void give(int stacks) {
    combat.applyToPlayer(jeff(), EffectType::Resonance, stacks, JEFF_ID,
                         20000.0f);
    run(TICK_MS, 0);
  }
};

void test_Beam_HitsOnlyTheFirstEnemyInLine() {
  World w;
  uint32_t near = w.foe(150.0f, 0.0f);
  uint32_t far = w.foe(300.0f, 0.0f);
  w.tap(LMB);
  assert(w.enemy(near).health < 5000.0f);
  float dealt = 5000.0f - w.enemy(near).health;
  assert(dealt >= 12.0f && dealt <= 12.0f * CRIT_MAX);
  assert(w.enemy(far).health == 5000.0f);
}

void test_Beam_Overheats_ThenReloads() {
  World w;
  w.foe(150.0f, 0.0f);
  // Held fire: heat climbs until the vents blow.
  int guard = 0;
  while (!Arena::JeffMode::overheated(w.jeff().kitMode) && guard++ < 600) {
    w.run(TICK_MS, LMB);
  }
  assert(Arena::JeffMode::overheated(w.jeff().kitMode));
  assert(w.jeff().resource > 200);
  // Overheated: holding the trigger does nothing.
  int castsBefore = 0;
  for (const auto& e : w.events) {
    if (e.kind == GameEventKind::AbilityCast && e.param == 0) castsBefore++;
  }
  w.run(800.0f, LMB);
  int castsDuring = 0;
  for (const auto& e : w.events) {
    if (e.kind == GameEventKind::AbilityCast && e.param == 0) castsDuring++;
  }
  assert(castsDuring == castsBefore);
  // The reload ends, heat empty, and he can fire again.
  w.run(1000.0f, 0);
  assert(!Arena::JeffMode::overheated(w.jeff().kitMode));
  assert(w.jeff().resource == 0);
  w.tap(LMB);
  assert(w.jeff().resource > 0);
}

void test_Heat_DrainsWhenNotFiring() {
  World w;
  w.run(600.0f, LMB);
  uint8_t hot = w.jeff().resource;
  assert(hot > 0 && !Arena::JeffMode::overheated(w.jeff().kitMode));
  w.run(3000.0f, 0);
  assert(w.jeff().resource == 0);
}

void test_SlainEnemiesLeaveBiomass_OnePileEach() {
  World w;
  w.addJeff(2, 500.0f, 500.0f);  // a second Jeff does not double the piles
  w.corpse(100.0f, 0.0f);
  w.corpse(2500.0f, 0.0f);  // far away: still leaves remains
  assert(w.piles() == 2);
  // Remains rot.
  w.run(12500.0f, 0);
  assert(w.piles() == 0);
}

void test_Harvest_ReachGrowsWithHold() {
  World w;
  w.corpse(60.0f, 0.0f);   // inside the shortest reach
  w.corpse(240.0f, 0.0f);  // only a long hold reaches it
  // A quick tap takes the near pile only.
  w.tap(RMB);
  assert(w.resonance() == 1);
  assert(w.piles() == 1);
  w.run(1000.0f, 0);  // cooldown
  w.run(1500.0f, RMB);
  w.run(TICK_MS, 0);
  assert(w.resonance() == 2);
  assert(w.piles() == 0);
}

void test_Harvest_NothingInReach_RefundsCooldown() {
  World w;
  w.tap(RMB);
  assert(w.jeff().cooldownMs[1] <= 300.0f);
  assert(w.resonance() == 0);
}

void test_Resonance_CapsAtTen() {
  World w;
  for (int i = 0; i < 12; i++) w.corpse(40.0f + 5.0f * i, 0.0f);
  w.tap(RMB);
  assert(w.resonance() == Arena::JeffMode::MAX_RESONANCE);
  assert(w.piles() == 0);
}

void test_Resonance_FasterAndHarder() {
  World w;
  w.run(TICK_MS, 0);
  float baseSpeed = w.jeff().speedMult;
  w.give(10);
  w.run(TICK_MS, 0);
  assert(w.jeff().speedMult > baseSpeed * 1.25f);
  uint32_t target = w.foe(150.0f, 0.0f);
  w.tap(LMB);
  float dealt = 5000.0f - w.enemy(target).health;
  assert(dealt >= 12.0f * 1.5f - 0.01f);
}

void test_Q_PiercesAll_AndDischargesResonance() {
  World w;
  uint32_t a = w.foe(120.0f, 0.0f);
  uint32_t b = w.foe(300.0f, 20.0f);
  uint32_t c = w.foe(500.0f, -20.0f);
  uint32_t off = w.foe(0.0f, 300.0f);  // not in the line
  w.tap(Q);
  float plain = 5000.0f - w.enemy(a).health;
  assert(plain >= 40.0f && plain <= 40.0f * CRIT_MAX);
  assert(w.enemy(b).health < 5000.0f);
  assert(w.enemy(c).health < 5000.0f);
  assert(w.enemy(off).health == 5000.0f);

  World charged;
  charged.give(10);
  uint32_t t = charged.foe(200.0f, 0.0f);
  charged.tap(Q);
  float big = 5000.0f - charged.enemy(t).health;
  // 40 + 16 * 10, and the stacks are gone before it lands (no passive bonus).
  assert(big >= 200.0f - 0.01f && big <= 200.0f * CRIT_MAX);
  assert(charged.resonance() == 0);
}

void test_Treads_DoubleSpeed_Haste_NoHarvest_AndBack() {
  World w;
  w.run(TICK_MS, 0);
  float legs = w.jeff().speedMult;
  w.tap(SHIFT);
  assert(Arena::JeffMode::treads(w.jeff().kitMode));
  w.run(5000.0f, 0);  // the Haste holds for as long as he rolls
  int haste = w.effects.getPlayerEffects(JEFF_ID).getStacks(EffectType::Haste);
  assert(haste >= 2);
  assert(w.jeff().speedMult >= legs * 2.0f * 1.39f);
  // No harvesting on treads.
  w.corpse(60.0f, 0.0f);
  w.tap(RMB);
  assert(w.resonance() == 0 && w.piles() == 1);
  // Still fights.
  uint32_t target = w.foe(150.0f, 0.0f);
  w.tap(LMB);
  assert(w.enemy(target).health < 5000.0f);
  // Legs again.
  w.run(1300.0f, 0);
  w.tap(SHIFT);
  assert(!Arena::JeffMode::treads(w.jeff().kitMode));
  w.run(1000.0f, 0);
  assert(w.effects.getPlayerEffects(JEFF_ID).getStacks(EffectType::Haste) == 0);
  w.tap(RMB);
  assert(w.resonance() == 1);
}

}  // namespace

int main() {
  Logger::init();
  test_Beam_HitsOnlyTheFirstEnemyInLine();
  test_Beam_Overheats_ThenReloads();
  test_Heat_DrainsWhenNotFiring();
  test_SlainEnemiesLeaveBiomass_OnePileEach();
  test_Harvest_ReachGrowsWithHold();
  test_Harvest_NothingInReach_RefundsCooldown();
  test_Resonance_CapsAtTen();
  test_Resonance_FasterAndHarder();
  test_Q_PiercesAll_AndDischargesResonance();
  test_Treads_DoubleSpeed_Haste_NoHarvest_AndBack();
  std::cout << "Jeff kit passed" << std::endl;
  return 0;
}
