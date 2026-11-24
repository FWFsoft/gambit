// Kade's kit on the real combat systems: Refraction Wall is held, pinning a
// mirror and turning its glass with the cursor, and sets mirrors down
// (three at most, the oldest replaced); anything striking a mirror leaves
// straight out of its glass and the barrier between them pushes
// and Exposes what crosses it; Full Release runs its beam for 3s, bends
// through the mirrors and Stuns, then leaves him spent and rooted; Mirror
// Step leaves a mirror behind and comes out of the one nearest the cursor.

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
constexpr uint8_t LMB = 1 << 0, Q = 1 << 2, SHIFT = 1 << 3;
constexpr uint32_t KADE_ID = 1;

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
  float aimX = 100.0f, aimY = 0.0f;

  World() {
    enemies.setCombat(&combat);
    Player kade;
    kade.id = KADE_ID;
    kade.characterId = Arena::CharacterId::Kade;
    players[KADE_ID] = kade;
    combat.applyKitStats(players[KADE_ID]);
  }

  Player& kade() { return players[KADE_ID]; }

  uint32_t foe(float x, float y) {
    uint32_t id = enemies.spawn(Arena::EnemyKind::VenoxaridGrunt, x, y);
    Enemy& e = enemies.getEnemies().at(id);
    e.health = e.maxHealth = 5000.0f;
    return id;
  }
  Enemy& enemy(uint32_t id) { return enemies.getEnemies().at(id); }

  void run(float ms, uint8_t buttons) {
    for (float t = 0.0f; t < ms; t += TICK_MS) {
      combat.setInput(kade(), buttons, aimX, aimY);
      combat.update(TICK_MS);
      enemies.update(TICK_MS, players, &effects);
    }
  }
  void tap(uint8_t button) {
    run(TICK_MS, button);
    run(TICK_MS, 0);
  }
  // Set a mirror down at (x, y), its glass facing on (away from Kade),
  // ready for the next cast.
  void mirror(float x, float y) { mirror(x, y, x, y); }
  // Pin at (x, y), turn the glass toward (faceX, faceY), release. Kade
  // stands just west of the pin for it and is put back afterwards (the
  // slap dashes him there).
  void mirror(float x, float y, float faceX, float faceY) {
    const float homeX = kade().x, homeY = kade().y;
    kade().x = x - 120.0f;
    kade().y = y;
    aimX = x;
    aimY = y;
    run(100.0f, LMB);
    aimX = faceX;
    aimY = faceY;
    run(100.0f, LMB);
    run(300.0f, 0);
    kade().cooldownMs[0] = 0.0f;
    kade().kitCounter[3] = 0;  // charges back, for setting up a scene
    kade().x = homeX;
    kade().y = homeY;
    kade().forcedMs = 0.0f;
  }
  int mirrorCount() {
    int n = 0;
    for (int i = 0; i < Arena::KadeMode::MAX_MIRRORS; i++) {
      uint32_t id = static_cast<uint32_t>(kade().kitCounter[i]);
      if (id && combat.findZone(id)) n++;
    }
    return n;
  }
};

void test_Wall_AtMostThreeMirrors_OldestReplaced() {
  World w;
  w.mirror(200.0f, 0.0f);
  uint32_t first = static_cast<uint32_t>(w.kade().kitCounter[0]);
  w.mirror(0.0f, 200.0f);
  w.mirror(-200.0f, 0.0f);
  assert(w.mirrorCount() == 3);
  w.mirror(0.0f, -200.0f);
  assert(w.mirrorCount() == 3);
  assert(!w.combat.findZone(first));
  const AbilitySystem::Zone* newest =
      w.combat.findZone(static_cast<uint32_t>(w.kade().kitCounter[2]));
  assert(newest && newest->y < -150.0f);
}

void test_Barrier_PushesAndExposesWhatCrosses() {
  World w;
  w.mirror(300.0f, -150.0f);
  w.mirror(300.0f, 150.0f);
  uint32_t a = w.foe(300.0f, 4.0f);  // on the line between them
  w.run(200.0f, 0);
  assert(std::abs(w.enemy(a).x - 300.0f) > 20.0f);  // pushed off it
  assert(w.effects.getEnemyEffects(a).getStacks(EffectType::Expose) == 1);
  assert(w.enemy(a).health == 5000.0f);  // Playmaker: no base damage
}

void test_FullRelease_RunsThenSpent_StunsAlongTheBeam() {
  World w;
  uint32_t a = w.foe(300.0f, 0.0f);
  w.aimX = 400.0f;
  w.aimY = 0.0f;
  w.tap(Q);
  assert(w.kade().kitMode == Arena::KadeMode::Releasing);
  w.run(300.0f, 0);
  assert(w.effects.getEnemyEffects(a).getStacks(EffectType::Stunned) >= 1);
  // Nothing else can be cast while the beam runs.
  w.tap(LMB);
  assert(w.mirrorCount() == 0);
  w.run(2800.0f, 0);
  assert(w.kade().kitMode == Arena::KadeMode::Spent);
  assert(w.kade().kitAnchored);
  w.run(1300.0f, 0);
  assert(w.kade().kitMode == Arena::KadeMode::Ready);
  assert(!w.kade().kitAnchored);
}

void test_Wall_HeldPinsTheMirrorAndTheCursorTurnsIt() {
  World w;
  w.mirror(300.0f, 0.0f, 250.0f, 120.0f);  // pinned, then turned south-west
  const AbilitySystem::Zone* m =
      w.combat.findZone(static_cast<uint32_t>(w.kade().kitCounter[0]));
  assert(m && std::abs(m->x - 300.0f) < 1.0f && std::abs(m->y) < 1.0f);
  // Snapped to one of 8 ways: south-west on the 45s.
  assert(std::abs(m->dirX + 0.7071f) < 0.01f && std::abs(m->dirY - 0.7071f) < 0.01f);
}

void test_Wall_DashesToArmsLengthAndSlapsItUp() {
  World w;
  w.aimX = 200.0f;
  w.aimY = 0.0f;
  w.run(100.0f, LMB);
  w.run(100.0f, 0);
  assert(w.mirrorCount() == 0);  // not yet: the slap lands after the dash
  w.run(300.0f, 0);
  assert(w.mirrorCount() == 1);
  assert(std::abs(w.kade().x - (200.0f - 46.0f)) < 6.0f);
  // Out of reach, the pin stops at 240.
  World far;
  far.aimX = 600.0f;
  far.run(100.0f, LMB);
  far.run(400.0f, 0);
  const AbilitySystem::Zone* m =
      far.combat.findZone(static_cast<uint32_t>(far.kade().kitCounter[0]));
  assert(m && std::abs(m->x - 240.0f) < 1.0f);
}

void test_Wall_ThreeQuickCharges_ThenWaitsForOne() {
  World w;
  float xs[4] = {120.0f, 0.0f, -120.0f, 0.0f};
  float ys[4] = {0.0f, 120.0f, 0.0f, -120.0f};
  for (int i = 0; i < 3; i++) {
    w.aimX = w.kade().x + xs[i];
    w.aimY = w.kade().y + ys[i];
    w.tap(LMB);
    w.run(450.0f, 0);  // the slap lands; the short lockout passes
  }
  assert(w.mirrorCount() == 3);
  assert(Arena::KadeMode::charges(w.kade().resource) == 0);
  uint32_t oldest = static_cast<uint32_t>(w.kade().kitCounter[0]);
  // No charge: the fourth press does nothing.
  w.aimX = w.kade().x + xs[3];
  w.aimY = w.kade().y + ys[3];
  w.tap(LMB);
  w.run(450.0f, 0);
  assert(w.combat.findZone(oldest));
  // One charge comes back after 5s (counted from the first drop), and the
  // next drop replaces the oldest mirror.
  w.run(4000.0f, 0);
  assert(Arena::KadeMode::charges(w.kade().resource) == 1);
  w.tap(LMB);
  w.run(450.0f, 0);
  assert(!w.combat.findZone(oldest));
  assert(w.mirrorCount() == 3);
}

// The human's test: two mirrors, the Q strikes one, which sends it into the
// other, which turns it again.
void test_FullRelease_BouncesMirrorToMirror() {
  World w;
  w.mirror(300.0f, 0.0f, 300.0f, 100.0f);     // A: glass to the south
  w.mirror(300.0f, 250.0f, 400.0f, 250.0f);   // B, below A: glass to the east
  uint32_t afterB = w.foe(550.0f, 250.0f);    // east of B: the second turn
  uint32_t betweenAB = w.foe(300.0f, 130.0f); // on the first turn's leg
  uint32_t pastA = w.foe(560.0f, 0.0f);       // straight on past A: shadowed
  uint32_t pastB = w.foe(300.0f, 450.0f);     // straight on past B: shadowed
  w.aimX = 600.0f;
  w.aimY = 0.0f;
  w.tap(Q);
  w.run(400.0f, 0);
  auto stunned = [&](uint32_t id) {
    return w.effects.getEnemyEffects(id).getStacks(EffectType::Stunned) >= 1;
  };
  assert(stunned(betweenAB));
  assert(stunned(afterB));
  assert(!stunned(pastA));
  assert(!stunned(pastB));
  // The beam's pulses drew three legs: Kade to A, A to B, B onward east.
  bool legAB = false, legOut = false;
  for (const GameEventPacket& e : w.events) {
    if (e.kind != GameEventKind::Beam) continue;
    if (std::abs(e.x - 300.0f) < 1.0f && std::abs(e.y) < 1.0f &&
        std::abs(e.x2 - 300.0f) < 1.0f && std::abs(e.y2 - 250.0f) < 1.0f) {
      legAB = true;
    }
    if (std::abs(e.x - 300.0f) < 1.0f && std::abs(e.y - 250.0f) < 1.0f &&
        e.x2 > 600.0f && std::abs(e.y2 - 250.0f) < 1.0f) {
      legOut = true;
    }
  }
  assert(legAB && legOut);
}

void test_FullRelease_LeavesEachMirrorOutOfItsGlass() {
  World w;
  w.mirror(300.0f, 0.0f, 300.0f, 100.0f);  // in the path, glass to the south
  uint32_t below = w.foe(300.0f, 300.0f);   // where the glass sends it
  uint32_t beyond = w.foe(560.0f, 0.0f);    // straight on: shadowed
  w.aimX = 600.0f;
  w.aimY = 0.0f;
  w.tap(Q);
  w.run(400.0f, 0);
  assert(w.effects.getEnemyEffects(below).getStacks(EffectType::Stunned) >= 1);
  assert(w.effects.getEnemyEffects(beyond).getStacks(EffectType::Stunned) == 0);
}

void test_Mirror_TurnsAPlayersShotOutOfItsGlass() {
  World w;
  w.mirror(300.0f, 0.0f, 300.0f, -100.0f);  // glass to the north
  AbilitySystem::Projectile shot;
  shot.ownerId = KADE_ID;
  shot.x = 100.0f;
  shot.vx = 600.0f;
  shot.lifeMs = 2000.0f;
  uint32_t id = w.combat.spawnProjectile(shot);
  w.events.clear();
  w.run(500.0f, 0);
  bool turned = false;
  for (const GameEventPacket& e : w.events) {
    if (e.kind == GameEventKind::ProjectileSpawn && e.extraId == id) {
      turned = std::abs(e.x2) < 1.0f && e.y2 < -500.0f;
    }
  }
  assert(turned);
}

void test_MirrorStep_NeedsAMirror() {
  World w;
  w.aimX = 300.0f;
  w.tap(SHIFT);
  assert(std::abs(w.kade().x) < 1.0f);
  assert(w.kade().cooldownMs[3] <= 0.0f);  // refused, not spent
  assert(w.kade().actionLockMs <= 0.0f);
  assert(w.mirrorCount() == 0);
  int denied = 0;
  for (const GameEventPacket& e : w.events) {
    assert(e.kind != GameEventKind::AbilityCast);
    if (e.kind == GameEventKind::CannotCast) {
      assert(e.sourceId == KADE_ID && e.param == 3);
      assert(std::abs(e.x) < 1.0f && std::abs(e.y) < 1.0f);
      denied++;
    }
  }
  assert(denied == 1);
}

void test_MirrorStep_EndedMirrorDoesNotAllowCast() {
  World w;
  w.mirror(300.0f, 0.0f);
  uint32_t stale = static_cast<uint32_t>(w.kade().kitCounter[0]);
  w.combat.endZone(stale);
  w.events.clear();
  w.tap(SHIFT);
  assert(std::abs(w.kade().x) < 1.0f);
  assert(w.kade().cooldownMs[3] <= 0.0f);
  assert(w.mirrorCount() == 0);
  bool denied = false;
  for (const GameEventPacket& e : w.events) {
    assert(e.kind != GameEventKind::AbilityCast);
    denied |= e.kind == GameEventKind::CannotCast;
  }
  assert(denied);
}

void test_MirrorStep_LeavesAMirrorAndComesOutOfTheTarget() {
  World w;
  w.mirror(300.0f, 0.0f);
  w.mirror(0.0f, 300.0f);
  w.aimX = 320.0f;  // nearest the first mirror
  w.aimY = 10.0f;
  w.tap(SHIFT);
  w.run(100.0f, 0);
  assert(w.kade().x > 300.0f);  // just through it
  assert(std::abs(w.kade().y) < 5.0f);
  assert(w.mirrorCount() == 3);
  const AbilitySystem::Zone* left =
      w.combat.findZone(static_cast<uint32_t>(w.kade().kitCounter[2]));
  assert(left && std::abs(left->x) < 1.0f && std::abs(left->y) < 1.0f);
}

void test_MirrorStep_AtTheLimit_KeepsTheTarget() {
  for (int targetIndex = 0; targetIndex < Arena::KadeMode::MAX_MIRRORS;
       targetIndex++) {
    World w;
    w.mirror(300.0f, 0.0f);  // oldest
    w.mirror(0.0f, 300.0f);
    w.mirror(-300.0f, 0.0f);
    uint32_t ids[3];
    for (int i = 0; i < 3; i++) {
      ids[i] = static_cast<uint32_t>(w.kade().kitCounter[i]);
    }
    uint32_t target = ids[targetIndex];
    const AbilitySystem::Zone* destination = w.combat.findZone(target);
    assert(destination);
    w.aimX = destination->x;
    w.aimY = destination->y;
    w.tap(SHIFT);
    w.run(100.0f, 0);
    assert(w.combat.findZone(target));
    assert(std::hypot(w.kade().x - w.aimX, w.kade().y - w.aimY) < 60.0f);
    assert(w.kade().cooldownMs[3] > 0.0f);
    assert(w.mirrorCount() == 3);
    const int evicted = targetIndex == 0 ? 1 : 0;
    for (int i = 0; i < 3; i++) {
      assert((w.combat.findZone(ids[i]) != nullptr) == (i != evicted));
    }
    assert(static_cast<uint32_t>(w.kade().kitCounter[0]) ==
           ids[evicted == 0 ? 1 : 0]);
    assert(static_cast<uint32_t>(w.kade().kitCounter[1]) == ids[2]);
    const AbilitySystem::Zone* left =
        w.combat.findZone(static_cast<uint32_t>(w.kade().kitCounter[2]));
    assert(left && std::abs(left->x) < 1.0f && std::abs(left->y) < 1.0f);
    for (const GameEventPacket& e : w.events) {
      assert(e.kind != GameEventKind::CannotCast);
    }
  }
}

}  // namespace

int main() {
  Logger::init();
  test_Wall_AtMostThreeMirrors_OldestReplaced();
  test_Barrier_PushesAndExposesWhatCrosses();
  test_FullRelease_RunsThenSpent_StunsAlongTheBeam();
  test_Wall_HeldPinsTheMirrorAndTheCursorTurnsIt();
  test_Wall_DashesToArmsLengthAndSlapsItUp();
  test_Wall_ThreeQuickCharges_ThenWaitsForOne();
  test_FullRelease_BouncesMirrorToMirror();
  test_FullRelease_LeavesEachMirrorOutOfItsGlass();
  test_Mirror_TurnsAPlayersShotOutOfItsGlass();
  test_MirrorStep_NeedsAMirror();
  test_MirrorStep_EndedMirrorDoesNotAllowCast();
  test_MirrorStep_LeavesAMirrorAndComesOutOfTheTarget();
  test_MirrorStep_AtTheLimit_KeepsTheTarget();
  std::cout << "Kade kit passed\n";
}
