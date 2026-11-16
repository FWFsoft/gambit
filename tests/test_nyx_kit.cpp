// Nyx's kit, on the real combat systems: the swarm riding a foe or an ally
// (Hive Mother), what sends it home, the Slow on passing through it, Plague
// Bringer radiating from both Nyx and the swarm with Contaminate, Nanite
// Cloud, and Become the Swarm with its damage share and the trip back to
// the CRT.

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
constexpr uint32_t NYX_ID = 1;
constexpr uint32_t ALLY_ID = 2;

namespace Mode = Arena::NyxMode;

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
      [this](const GameEventPacket &p) { events.push_back(p); }};
  float aimX = 100.0f, aimY = 0.0f;

  World() {
    enemies.setCombat(&combat);
    Player nyx;
    nyx.id = NYX_ID;
    nyx.characterId = Arena::CharacterId::Nyx;
    players[NYX_ID] = nyx;
    combat.applyKitStats(players[NYX_ID]);
  }

  Player &nyx() { return players[NYX_ID]; }
  Player &ally() { return players[ALLY_ID]; }
  uint8_t swarm() { return Mode::swarm(nyx().kitMode); }
  uint8_t form() { return Mode::self(nyx().kitMode); }

  void addAlly(float x, float y) {
    Player ally;
    ally.id = ALLY_ID;
    ally.characterId = Arena::CharacterId::Wade;
    ally.x = x;
    ally.y = y;
    players[ALLY_ID] = ally;
    combat.applyKitStats(players[ALLY_ID]);
  }

  // A sturdy foe that will not die to the test's hits, and stands still.
  uint32_t foe(float x, float y) {
    uint32_t id = enemies.spawn(Arena::EnemyKind::VenoxaridGrunt, x, y);
    Enemy &e = enemies.getEnemies().at(id);
    e.health = e.maxHealth = 5000.0f;
    return id;
  }
  Enemy &enemy(uint32_t id) { return enemies.getEnemies().at(id); }
  int stacks(uint32_t enemyId, EffectType type) {
    return effects.getEnemyEffects(enemyId).getStacks(type);
  }
  int allyStacks(EffectType type) {
    return effects.getPlayerEffects(ALLY_ID).getStacks(type);
  }

  void run(float ms, uint8_t buttons = 0) {
    for (float t = 0.0f; t < ms; t += TICK_MS) {
      combat.setInput(nyx(), buttons, aimX, aimY);
      combat.update(TICK_MS);
      effects.update(TICK_MS / 1000.0f, players, enemies.getEnemies(),
                     &enemies);
    }
  }
  void tap(uint8_t button) {
    run(TICK_MS, button);
    run(TICK_MS, 0);
  }
  void aimAt(float x, float y) {
    aimX = x;
    aimY = y;
  }
};

void test_Selectable() {
  assert(Arena::isPlayable(Arena::CharacterId::Nyx));
  assert(makeKit(Arena::CharacterId::Nyx));
  World w;
  w.run(50.0f);
  assert(w.swarm() == Mode::SwarmHome);
  assert(w.form() == Mode::Body);
}

void test_Swarm_RidesAFoe_SilencesAndStacks() {
  World w;
  uint32_t foe = w.foe(300.0f, 0.0f);
  w.aimAt(300.0f, 0.0f);
  w.tap(LMB);
  assert(w.swarm() == Mode::SwarmFlying);
  w.run(500.0f);  // 300 px at 900 px/s
  assert(w.swarm() == Mode::SwarmOnEnemy);
  assert(std::abs(w.nyx().kitX - 300.0f) < 1.0f);
  assert(w.stacks(foe, EffectType::Silenced) > 0);
  assert(w.stacks(foe, EffectType::Dulled) > 0);
  int weakened = w.stacks(foe, EffectType::Weakened);
  assert(weakened >= 1);
  w.run(2100.0f);  // two more pulses
  assert(w.stacks(foe, EffectType::Weakened) >= weakened + 2);
  w.run(4000.0f);  // capped at 5
  assert(w.stacks(foe, EffectType::Weakened) == 5);
  // The swarm holds Silence; it does not bank time on the foe.
  const EffectInstance *silence =
      w.effects.getEnemyEffects(foe).findEffect(EffectType::Silenced);
  assert(silence && silence->remainingDuration <= 1400.0f);
}

void test_Swarm_RidesAnAlly_FocusesAndSharpens() {
  World w;
  w.addAlly(250.0f, 0.0f);
  w.aimAt(250.0f, 10.0f);
  w.tap(LMB);
  w.run(400.0f);
  assert(w.swarm() == Mode::SwarmOnAlly);
  assert(w.allyStacks(EffectType::Focused) > 0);
  assert(w.allyStacks(EffectType::Sharpened) > 0);
  // Moving with the ally.
  w.ally().x = 400.0f;
  w.run(50.0f);
  assert(std::abs(w.nyx().kitX - 400.0f) < 1.0f);
}

void test_Swarm_CleansedFromTheHost_GoesHome() {
  World w;
  uint32_t foe = w.foe(300.0f, 0.0f);
  w.aimAt(300.0f, 0.0f);
  w.tap(LMB);
  w.run(500.0f);
  assert(w.swarm() == Mode::SwarmOnEnemy);
  w.effects.removeEffect(foe, true, EffectType::Silenced);  // a cleanse
  w.run(TICK_MS * 2);
  assert(w.swarm() == Mode::SwarmReturning);
  w.run(600.0f);
  assert(w.swarm() == Mode::SwarmHome);
}

void test_Swarm_PurgedFromAnAlly_GoesHome() {
  World w;
  w.addAlly(250.0f, 0.0f);
  w.aimAt(250.0f, 0.0f);
  w.tap(LMB);
  w.run(400.0f);
  assert(w.swarm() == Mode::SwarmOnAlly);
  w.effects.purgeBuff(ALLY_ID, EffectType::Focused);
  w.run(TICK_MS * 2);
  assert(w.swarm() == Mode::SwarmReturning);
}

void test_Swarm_LeashedToNyx() {
  World w;
  w.aimAt(500.0f, 0.0f);
  w.tap(LMB);
  w.run(700.0f);
  assert(w.swarm() == Mode::SwarmParked);
  w.nyx().x = -400.0f;  // 900 px away
  w.run(TICK_MS * 2);
  assert(w.swarm() == Mode::SwarmReturning);
}

void test_Swarm_SlowsFoesPassingThroughOncePerPass() {
  World w;
  uint32_t foe = w.foe(250.0f, 120.0f);  // beside the path, not the target
  w.aimAt(500.0f, 0.0f);
  w.tap(LMB);
  w.run(700.0f);
  assert(w.stacks(foe, EffectType::Slow) == 0);  // never touched it
  w.enemy(foe).y = 0.0f;  // now it walks into the parked swarm
  w.enemy(foe).x = 500.0f;
  w.run(200.0f);
  assert(w.stacks(foe, EffectType::Slow) == 1);
  w.run(1000.0f);  // standing in it does not stack more
  assert(w.stacks(foe, EffectType::Slow) == 1);
}

void test_PlagueBringer_RadiatesFromTheSwarmAndContaminates() {
  World w;
  uint32_t host = w.foe(500.0f, 0.0f);
  uint32_t near = w.foe(560.0f, 60.0f);   // by the swarm, far from Nyx
  uint32_t far = w.foe(1200.0f, 0.0f);    // out of everything
  w.addAlly(30.0f, 0.0f);
  w.aimAt(500.0f, 0.0f);
  w.tap(LMB);
  w.run(2700.0f);  // ride the host for a few pulses
  assert(w.swarm() == Mode::SwarmOnEnemy);
  assert(w.stacks(near, EffectType::Silenced) == 0);
  w.run(950.0f, RMB);  // full charge
  w.run(TICK_MS * 2, 0);
  assert(w.stacks(near, EffectType::Weakened) >= 2);
  assert(w.stacks(far, EffectType::Weakened) == 0);
  assert(w.allyStacks(EffectType::Empowered) == 1);
  // The host's Silence, Dulled and Weakened spread to the foe near it.
  assert(w.stacks(near, EffectType::Silenced) > 0);
  assert(w.stacks(near, EffectType::Dulled) > 0);
  bool spread = false;
  for (const GameEventPacket &e : w.events) {
    if (e.kind == GameEventKind::EffectVerb &&
        e.param == static_cast<uint8_t>(EffectVerb::Spread) &&
        e.extraId == host) {
      spread = true;
    }
  }
  assert(spread);
}

void test_PlagueBringer_ContaminateTakesAnAllysDebuffs() {
  World w;
  w.addAlly(200.0f, 0.0f);
  uint32_t foe = w.foe(260.0f, 40.0f);
  w.combat.applyToPlayer(w.ally(), EffectType::Wound, 3, 0);
  w.aimAt(200.0f, 0.0f);
  w.tap(LMB);
  w.run(400.0f);
  assert(w.swarm() == Mode::SwarmOnAlly);
  w.run(TICK_MS * 3, RMB);
  w.run(TICK_MS * 2, 0);
  assert(w.stacks(foe, EffectType::Wound) == 3);
  assert(w.allyStacks(EffectType::Wound) == 0);
}

void test_NaniteCloud_NeedsARideMarksFoes() {
  World w;
  uint32_t foe = w.foe(300.0f, 0.0f);
  w.aimAt(300.0f, 0.0f);
  w.tap(Q);
  assert(w.nyx().cooldownMs[2] == 0.0f);  // refused: nothing attached
  w.tap(LMB);
  w.run(500.0f);
  w.tap(Q);
  assert(w.nyx().cooldownMs[2] > 0.0f);
  assert(w.stacks(foe, EffectType::Marked) > 0);
}

void test_BecomeTheSwarm_RidesAnAllySharesDamageAndComesHome() {
  World w;
  w.addAlly(300.0f, 0.0f);
  w.nyx().x = 0.0f;
  w.aimAt(300.0f, 0.0f);
  w.tap(SHIFT);
  assert(w.form() == Mode::Dropping);
  assert(w.nyx().kitAnchored);
  assert(w.nyx().propX == 0.0f);  // the CRT stays where she stood
  w.run(300.0f);
  assert(w.form() == Mode::Flying);
  assert((w.nyx().stateFlags & PlayerFlags::Hidden) != 0);
  w.run(300.0f);
  assert(w.form() == Mode::OnAlly);
  assert(std::abs(w.nyx().x - 300.0f) < 1.0f);
  assert(w.allyStacks(EffectType::Focused) > 0);

  // She takes half of what hits her ride; direct hits on her do nothing.
  float nyxBefore = w.nyx().health;
  float allyBefore = w.ally().health;
  w.combat.damagePlayer(w.ally(), 40.0f, 0);
  assert(std::abs((allyBefore - w.ally().health) - 20.0f) < 0.5f);
  assert(std::abs((nyxBefore - w.nyx().health) - 20.0f) < 0.5f);
  nyxBefore = w.nyx().health;
  w.combat.damagePlayer(w.nyx(), 40.0f, 0);
  assert(w.nyx().health == nyxBefore);

  // She rides along, then streams home to the CRT and lifts it.
  w.ally().x = 600.0f;
  w.run(50.0f);
  assert(std::abs(w.nyx().x - 600.0f) < 1.0f);
  w.run(200.0f);  // the press lock from the first cast
  w.tap(SHIFT);
  assert(w.form() == Mode::Homing);
  w.run(500.0f);  // 600 px at 1500 px/s
  assert(w.form() == Mode::Reforming);
  assert(std::abs(w.nyx().x) < 1.0f);
  w.run(500.0f);
  assert(w.form() == Mode::Body);
  assert(!w.nyx().kitAnchored);
}

void test_BecomeTheSwarm_OnAFoe_SheShares_TheFoeTakesAll() {
  World w;
  uint32_t foe = w.foe(300.0f, 0.0f);
  w.addAlly(-200.0f, 0.0f);
  w.aimAt(300.0f, 0.0f);
  w.tap(SHIFT);
  w.run(700.0f);
  assert(w.form() == Mode::OnEnemy);
  assert(w.stacks(foe, EffectType::Silenced) > 0);
  float foeBefore = w.enemy(foe).health;
  float nyxBefore = w.nyx().health;
  w.combat.damageEnemy(ALLY_ID, w.enemy(foe), 40.0f, DamageFlag::Dot);
  assert(foeBefore - w.enemy(foe).health > 39.0f);
  assert(nyxBefore - w.nyx().health > 19.0f);
  // The foe dies: she goes home to the CRT.
  w.enemy(foe).health = 1.0f;
  w.combat.damageEnemy(ALLY_ID, w.enemy(foe), 50.0f);
  w.enemies.update(TICK_MS, w.players, &w.effects);
  w.run(TICK_MS * 2);
  assert(w.form() == Mode::Homing);
}

void test_BecomeTheSwarm_NeedsATarget() {
  World w;
  w.aimAt(300.0f, 0.0f);
  w.tap(SHIFT);
  assert(w.form() == Mode::Body);
  assert(w.nyx().cooldownMs[3] == 0.0f);
}

}  // namespace

int main() {
  Logger::init();
  test_Selectable();
  test_Swarm_RidesAFoe_SilencesAndStacks();
  test_Swarm_RidesAnAlly_FocusesAndSharpens();
  test_Swarm_CleansedFromTheHost_GoesHome();
  test_Swarm_PurgedFromAnAlly_GoesHome();
  test_Swarm_LeashedToNyx();
  test_Swarm_SlowsFoesPassingThroughOncePerPass();
  test_PlagueBringer_RadiatesFromTheSwarmAndContaminates();
  test_PlagueBringer_ContaminateTakesAnAllysDebuffs();
  test_NaniteCloud_NeedsARideMarksFoes();
  test_BecomeTheSwarm_RidesAnAllySharesDamageAndComesHome();
  test_BecomeTheSwarm_OnAFoe_SheShares_TheFoeTakesAll();
  test_BecomeTheSwarm_NeedsATarget();
  std::cout << "Nyx kit passed\n";
}
