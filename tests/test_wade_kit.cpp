// Wade's kit, on the real combat systems: punch combinations, the planted
// uppercut channel, Slip's haymaker counter and Roll's Resonance discharge.

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

namespace Form = Arena::WadeForm;
constexpr float TICK_MS = 1000.0f / 60.0f;
constexpr uint8_t LMB = 1 << 0, RMB = 1 << 1, Q = 1 << 2, SHIFT = 1 << 3;
constexpr uint32_t WADE_ID = 1;

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

  World() {
    enemies.setCombat(&combat);
    Player wade;
    wade.id = WADE_ID;
    wade.characterId = Arena::CharacterId::Wade;
    wade.x = 0.0f;
    wade.y = 0.0f;
    players[WADE_ID] = wade;
    combat.applyKitStats(players[WADE_ID]);
  }

  Player &wade() { return players[WADE_ID]; }

  // Hold `buttons` and aim to the right for `ms` of game time. Only the
  // combat system ticks: enemies stand still, so tests decide who hits whom.
  void run(float ms, uint8_t buttons) {
    for (float t = 0.0f; t < ms; t += TICK_MS) {
      Player &w = wade();
      combat.setInput(w, buttons, w.x + 100.0f, w.y);
      combat.update(TICK_MS);
    }
  }

  // The form of every cast of `slot` so far.
  std::vector<uint8_t> casts(int slot) const {
    std::vector<uint8_t> out;
    for (const GameEventPacket &p : events) {
      if (p.kind == GameEventKind::AbilityCast && p.sourceId == WADE_ID &&
          p.param == slot) {
        out.push_back(static_cast<uint8_t>(p.extraId));
      }
    }
    return out;
  }

  Enemy &spawnEnemy(float x, float y) {
    uint32_t id = enemies.spawn(Arena::EnemyKind::Toxlith, x, y);
    Enemy &enemy = enemies.getEnemies().at(id);
    enemy.maxHealth = enemy.health = 1000.0f;
    return enemy;
  }
};

bool rearHand(uint8_t punch) {
  return punch == Form::Cross || punch == Form::RearHook ||
         punch == Form::CrossBody;
}

void test_SimplePunches_ThrowValidCombinations() {
  World world;
  world.run(20000.0f, LMB);
  std::vector<uint8_t> punches = world.casts(0);
  assert(punches.size() >= 30);

  bool seen[Form::PunchCount] = {};
  for (size_t i = 0; i < punches.size(); i++) {
    assert(punches[i] < Form::PunchCount);
    seen[punches[i]] = true;
    // Inside a three-punch combination the rear hand never throws twice
    // running: it has to come back before it can go again.
    if (i % 3 != 0)
      assert(!(rearHand(punches[i]) && rearHand(punches[i - 1])));
  }
  // Over 30+ punches he has thrown the whole vocabulary: 1, 2, 3, 4 and
  // both body shots.
  for (bool s : seen)
    assert(s);
}

void test_SimplePunches_ThirdPunchGrantsHasteAndResetsRoll() {
  World world;
  Player &wade = world.wade();
  wade.cooldownMs[3] = 5000.0f;
  while (world.casts(0).size() < 2)
    world.run(TICK_MS, LMB);
  assert(!world.effects.getPlayerEffects(WADE_ID).hasEffect(EffectType::Haste));
  assert(wade.cooldownMs[3] > 0.0f);
  while (world.casts(0).size() < 3)
    world.run(TICK_MS, LMB);
  assert(world.effects.getPlayerEffects(WADE_ID).getStacks(EffectType::Haste) ==
         1);
  assert(wade.cooldownMs[3] == 0.0f);
}

void test_Uppercuts_AlternateHandsAndPlantFeet() {
  World world;
  world.run(1500.0f, RMB);
  std::vector<uint8_t> hands = world.casts(1);
  assert(hands.size() >= 5);
  for (size_t i = 0; i < hands.size(); i++) {
    assert(hands[i] == (i % 2 == 0 ? Form::LeadUppercut : Form::RearUppercut));
  }
  // Planted: no walking while the channel runs.
  assert(world.wade().speedMult == 0.0f);

  world.run(100.0f, 0);
  assert(world.wade().speedMult > 0.0f);
  // A new hold opens with the lead hand again.
  world.wade().cooldownMs[1] = 0.0f;
  size_t before = world.casts(1).size();
  world.run(100.0f, RMB);
  assert(world.casts(1).size() > before);
  assert(world.casts(1)[before] == Form::LeadUppercut);
}

void test_Uppercuts_SpeedUpWithHaste() {
  World slow, fast;
  fast.combat.applyToPlayer(fast.wade(), EffectType::Haste, 5, WADE_ID,
                            60000.0f);
  slow.run(2000.0f, RMB);
  fast.run(2000.0f, RMB);
  assert(fast.casts(1).size() > slow.casts(1).size() + 3);
}

void test_Slip_CountersWithDelayedHaymakerTowardAttacker() {
  World world;
  Player &wade = world.wade();
  // The attacker stands behind the aim direction (aim is +x): the haymaker
  // must go at them, not at the cursor.
  Enemy &attacker = world.spawnEnemy(-150.0f, 0.0f);
  Enemy &bystander = world.spawnEnemy(150.0f, 0.0f);
  uint32_t attackerId = attacker.id, bystanderId = bystander.id;

  world.run(TICK_MS, Q);
  assert(world.casts(2).size() == 1 && world.casts(2)[0] == Form::SlipChamber);
  assert(wade.intangibleMs > 0.0f);
  assert(wade.cooldownMs[2] > 0.0f);
  world.run(100.0f, 0);

  float healthBefore = wade.health;
  float dealt = world.combat.damagePlayer(wade, 40.0f, attackerId);
  assert(dealt == 0.0f && wade.health == healthBefore); // slipped
  assert(world.casts(2).size() == 2 && world.casts(2)[1] == Form::Haymaker);
  assert(wade.cooldownMs[2] == 0.0f); // Slip is refreshed

  // The haymaker is loading: nothing has landed yet, and he cannot be hit
  // out of it.
  Enemy *a = world.combat.findEnemy(attackerId);
  assert(a && a->health == 1000.0f);
  assert(world.combat.damagePlayer(wade, 40.0f, attackerId) == 0.0f);
  assert(world.casts(2).size() == 2); // one hit, one counter

  world.run(300.0f, 0);
  a = world.combat.findEnemy(attackerId);
  assert(a && a->health <= 1000.0f - 70.0f); // massive damage, that way
  Enemy *b = world.combat.findEnemy(bystanderId);
  assert(b && b->health == 1000.0f);

  // Afterwards he is an ordinary target again.
  world.run(300.0f, 0);
  assert(world.combat.damagePlayer(wade, 40.0f, attackerId) > 0.0f);
}

void test_Slip_NoCounterWhenNothingHits() {
  World world;
  Player &wade = world.wade();
  Enemy &enemy = world.spawnEnemy(100.0f, 0.0f);
  uint32_t enemyId = enemy.id;
  world.run(TICK_MS, Q);
  world.run(600.0f, 0); // the window passes
  assert(world.casts(2).size() == 1);
  assert(wade.cooldownMs[2] > 0.0f);
  assert(world.combat.damagePlayer(wade, 40.0f, enemyId) > 0.0f);
  assert(world.casts(2).size() == 1);
  assert(world.combat.findEnemy(enemyId)->health == 1000.0f);
}

void test_Roll_CleansesAndDischargesResonance() {
  World plain, charged;
  for (World *world : {&plain, &charged}) {
    Player &wade = world->wade();
    world->combat.applyToPlayer(wade, EffectType::Slow, 2, 0);
    world->combat.applyToPlayer(wade, EffectType::Snared, 1, 0);
    assert(
        world->effects.getPlayerEffects(WADE_ID).hasEffect(EffectType::Slow));
  }
  charged.combat.applyToPlayer(charged.wade(), EffectType::Resonance, 1,
                               WADE_ID, 60000.0f);

  plain.run(600.0f, SHIFT);
  charged.run(600.0f, SHIFT);

  for (World *world : {&plain, &charged}) {
    const ActiveEffects &active = world->effects.getPlayerEffects(WADE_ID);
    assert(!active.hasEffect(EffectType::Slow));
    assert(!active.hasEffect(EffectType::Snared));
  }
  assert(plain.casts(3).size() == 1 && plain.casts(3)[0] == Form::Roll);
  assert(charged.casts(3).size() == 1 &&
         charged.casts(3)[0] == Form::ResonanceRoll);
  // Resonance is spent, and bought a much longer roll toward the cursor.
  assert(!charged.effects.getPlayerEffects(WADE_ID).hasEffect(
      EffectType::Resonance));
  assert(plain.wade().x > 150.0f && plain.wade().x < 230.0f);
  assert(charged.wade().x > plain.wade().x * 1.8f);
}

} // namespace

int main() {
  Logger::init();
  test_SimplePunches_ThrowValidCombinations();
  test_SimplePunches_ThirdPunchGrantsHasteAndResetsRoll();
  test_Uppercuts_AlternateHandsAndPlantFeet();
  test_Uppercuts_SpeedUpWithHaste();
  test_Slip_CountersWithDelayedHaymakerTowardAttacker();
  test_Slip_NoCounterWhenNothingHits();
  test_Roll_CleansesAndDischargesResonance();
  std::cout << "All Wade kit tests passed" << std::endl;
  return 0;
}
