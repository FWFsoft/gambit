// Nolan's kit, on the real combat systems: the sweep's knock-up and hop, the
// Lotus knockback, the Expose passive, and the two moves that need an
// airborne foe (Black-out Kick and Anime Shit), including Wade's Resonance.

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
constexpr uint32_t NOLAN_ID = 1;
constexpr uint32_t WADE_ID = 2;

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
    Player nolan;
    nolan.id = NOLAN_ID;
    nolan.characterId = Arena::CharacterId::Nolan;
    players[NOLAN_ID] = nolan;
    combat.applyKitStats(players[NOLAN_ID]);
  }

  Player &nolan() { return players[NOLAN_ID]; }

  void addWade() {
    Player wade;
    wade.id = WADE_ID;
    wade.characterId = Arena::CharacterId::Wade;
    wade.x = -300.0f;
    players[WADE_ID] = wade;
    combat.applyKitStats(players[WADE_ID]);
  }

  // A sturdy foe that will not die to the test's hits.
  uint32_t foe(float x, float y, Arena::EnemyKind kind = Arena::EnemyKind::Toxlith) {
    uint32_t id = enemies.spawn(kind, x, y);
    Enemy &e = enemies.getEnemies().at(id);
    e.health = e.maxHealth = 5000.0f;
    return id;
  }
  Enemy &enemy(uint32_t id) { return enemies.getEnemies().at(id); }
  int stacks(uint32_t enemyId, EffectType type) {
    return effects.getEnemyEffects(enemyId).getStacks(type);
  }

  // Hold `buttons` aimed at (aimX, aimY) for `ms`. With `moveEnemies`,
  // enemies run too (knockbacks travel, knock-ups count down).
  void run(float ms, uint8_t buttons, bool moveEnemies = false) {
    for (float t = 0.0f; t < ms; t += TICK_MS) {
      combat.setInput(nolan(), buttons, aimX, aimY);
      combat.update(TICK_MS);
      if (moveEnemies) enemies.update(TICK_MS, players, &effects);
    }
  }
  void tap(uint8_t button) {
    run(TICK_MS, button);
    run(TICK_MS, 0);
  }
};

void test_Selectable() {
  assert(Arena::isPlayable(Arena::CharacterId::Nolan));
  assert(makeKit(Arena::CharacterId::Nolan));
  const Arena::KitInfo *kit = Arena::kitInfo(Arena::CharacterId::Nolan);
  assert(kit->abilities[1].mode == Arena::AbilityMode::Charge);
}

void test_Sweep_KnocksUpExposesAndHopsBack() {
  World w;
  uint32_t a = w.foe(80.0f, 0.0f);
  uint32_t behind = w.foe(-80.0f, 0.0f);
  w.tap(LMB);
  // The kick lands on frame 2 of the clip, not on the press.
  assert(w.enemy(a).health == 5000.0f);
  w.run(100.0f, 0);
  assert(w.enemy(a).health < 5000.0f);
  assert(w.enemy(a).airborne());
  assert(w.stacks(a, EffectType::Expose) == 1);
  // Only in front.
  assert(!w.enemy(behind).airborne());
  assert(w.enemy(behind).health == 5000.0f);
  // Then he hops back, away from the cursor.
  w.run(400.0f, 0);
  assert(w.nolan().x < -40.0f);
  assert(w.nolan().cooldownMs[0] > 0.0f);
}

void test_Airborne_CannotActAndLands() {
  World w;
  uint32_t a = w.foe(80.0f, 0.0f);
  w.tap(LMB);
  w.run(100.0f, 0);
  float air = w.enemy(a).airMs;
  assert(air > 500.0f);
  float x = w.enemy(a).x;
  w.run(200.0f, 0, true);
  assert(w.enemy(a).airborne());
  // It hangs where it was kicked: no chasing while airborne.
  assert(std::abs(w.enemy(a).x - x) < 0.01f);
  w.run(air, 0, true);
  assert(!w.enemy(a).airborne());
}

void test_Passive_ExposeLengthensKnockUp() {
  World plain, exposed;
  uint32_t a = plain.foe(80.0f, 0.0f);
  uint32_t b = exposed.foe(80.0f, 0.0f);
  exposed.combat.applyToEnemy(exposed.enemy(b), EffectType::Expose, 3, 99);
  plain.tap(LMB);
  exposed.tap(LMB);
  plain.run(100.0f, 0);
  exposed.run(100.0f, 0);
  // Three stacks read before the hit: +105%.
  float ratio = exposed.enemy(b).airMs / plain.enemy(a).airMs;
  assert(ratio > 1.95f && ratio < 2.15f);
}

void test_Lotus_ChargedKnockbackAllAround() {
  World w;
  uint32_t front = w.foe(90.0f, 0.0f);
  uint32_t back = w.foe(-90.0f, 0.0f);
  uint32_t far = w.foe(400.0f, 0.0f);
  // Held: nothing happens until release.
  w.run(1000.0f, RMB);
  assert(w.enemy(front).knockMs <= 0.0f);
  w.run(200.0f, 0, true);
  assert(w.stacks(front, EffectType::Expose) == 1);
  assert(w.stacks(back, EffectType::Expose) == 1);
  assert(w.stacks(far, EffectType::Expose) == 0);
  // Read just after the knockback ends, before they run back in.
  w.run(70.0f, 0, true);
  // Full charge throws them far, both ways round.
  assert(w.enemy(front).x > 300.0f);
  assert(w.enemy(back).x < -300.0f);
  assert(w.nolan().cooldownMs[1] > 0.0f);

  // A quick tap barely moves them.
  World t;
  uint32_t near = t.foe(90.0f, 0.0f);
  t.tap(RMB);
  t.run(270.0f, 0, true);
  assert(t.enemy(near).x < 200.0f && t.enemy(near).x > 160.0f);
}

void test_Blackout_NeedsAnAirborneFoe() {
  World w;
  w.foe(80.0f, 0.0f);
  w.tap(Q);
  w.run(200.0f, 0);
  // Refused: the cooldown is not spent and nothing was cast.
  assert(w.nolan().cooldownMs[2] <= 0.0f);
  for (const GameEventPacket &e : w.events) {
    assert(!(e.kind == GameEventKind::AbilityCast && e.param == 2));
  }
}

void test_Blackout_SlamsStunsAndResonatesWade() {
  World w;
  w.addWade();
  uint32_t target = w.foe(80.0f, 0.0f);
  uint32_t bystander = w.foe(150.0f, 60.0f);
  uint32_t distant = w.foe(80.0f, 600.0f);
  w.combat.knockUpEnemy(w.enemy(target), 1000.0f);
  w.tap(Q);
  w.run(100.0f, 0);
  assert(w.nolan().cooldownMs[2] > 0.0f);
  assert(!w.enemy(target).airborne());
  assert(w.enemy(target).health < 5000.0f);
  assert(w.stacks(target, EffectType::Stunned) > 0);
  assert(w.stacks(bystander, EffectType::Stunned) > 0);
  assert(w.stacks(distant, EffectType::Stunned) == 0);
  assert(w.effects.getPlayerEffects(WADE_ID).getStacks(EffectType::Resonance) == 1);
  // And Nolan gets none.
  assert(w.effects.getPlayerEffects(NOLAN_ID).getStacks(EffectType::Resonance) == 0);
}

void test_AnimeShit_TeleportsToTheLandingAndExposes() {
  World w;
  uint32_t target = w.foe(300.0f, 0.0f);
  // Refused with nothing in the air.
  w.aimX = 300.0f;
  w.tap(SHIFT);
  assert(w.nolan().cooldownMs[3] <= 0.0f);
  assert(w.nolan().x == 0.0f);

  // In the air and flying away: he arrives where it will come down.
  w.combat.knockUpEnemy(w.enemy(target), 1000.0f);
  w.enemies.pushEnemy(w.enemy(target), 1.0f, 0.0f, 120.0f, 400.0f);
  w.tap(SHIFT);
  assert(w.nolan().cooldownMs[3] > 0.0f);
  float landing = 420.0f - Arena::Tuning::PLAYER_RADIUS - w.enemy(target).radius;
  assert(std::abs(w.nolan().x - landing) < 2.0f);
  assert(w.stacks(target, EffectType::Expose) == 1);
}

void test_TheCombo_SweepBlinkKick() {
  // LMB throws it up, Shift follows, Q drives it down: the loop the kit is for.
  World w;
  uint32_t target = w.foe(80.0f, 0.0f);
  w.tap(LMB);
  w.run(100.0f, 0, true);
  assert(w.enemy(target).airborne());
  w.aimX = w.enemy(target).x;
  w.tap(SHIFT);
  assert(w.stacks(target, EffectType::Expose) == 2);
  w.run(200.0f, 0, true);  // the blink's action lock
  assert(w.enemy(target).airborne());
  w.tap(Q);
  w.run(100.0f, 0, true);
  assert(!w.enemy(target).airborne());
  assert(w.stacks(target, EffectType::Stunned) > 0);
}

}  // namespace

int main() {
  Logger::init();
  test_Selectable();
  test_Sweep_KnocksUpExposesAndHopsBack();
  test_Airborne_CannotActAndLands();
  test_Passive_ExposeLengthensKnockUp();
  test_Lotus_ChargedKnockbackAllAround();
  test_Blackout_NeedsAnAirborneFoe();
  test_Blackout_SlamsStunsAndResonatesWade();
  test_AnimeShit_TeleportsToTheLandingAndExposes();
  test_TheCombo_SweepBlinkKick();
  std::cout << "Nolan kit passed\n";
}
