// Points of interest on the real server systems: the scrap haul (salvage,
// carry slowed, bank by walking into the ship's ring), probes delivered on
// arrival, camps that count for their objective however they are cleared,
// and a gate guardian that waits.

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
#include "MatchDirector.h"
#include "ObjectiveSystem.h"

namespace {

constexpr float TICK_MS = 1000.0f / 60.0f;
constexpr uint32_t HERO = 1;

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
  std::vector<uint32_t> pickups;  // item ids the director dropped
  MatchDirector director{players, &enemies, &combat, 0.0f,
                         0.0f,    8000.0f,  4000.0f, hooks()};

  MatchDirector::Hooks hooks() {
    MatchDirector::Hooks h;
    h.spawnPickup = [this](uint32_t id, float, float) {
      pickups.push_back(id);
    };
    return h;
  }

  World() {
    enemies.setCombat(&combat);
    Player hero;
    hero.id = HERO;
    players[HERO] = hero;
    director.onPlayerJoined(players[HERO]);
    director.onCharacterSelected(players[HERO], Arena::CharacterId::Wade);
    director.onReady(players[HERO], true);
    run(Arena::Intro::LENGTH * 1000.0f + 500.0f);
    assert(director.phase() == Arena::MatchPhase::Round);
  }

  Player& hero() { return players[HERO]; }
  bool slowed() {
    return effects.getPlayerEffects(HERO).hasEffect(EffectType::Slow);
  }
  void run(float ms) {
    for (float t = 0.0f; t < ms; t += TICK_MS) {
      combat.update(TICK_MS);
      director.update(TICK_MS);
    }
  }
};

void scrapIsCarriedSlowedAndBankedInTheShipRing() {
  World w;
  w.director.damageShip(600.0f);
  float before = w.director.shipHealth();

  w.hero().x = 900.0f;
  w.hero().y = 0.0f;
  w.director.onObjectiveCompleted(w.hero(), true, 900.0f, 0.0f);
  assert(w.hero().scrapCarried == Arena::Tuning::SCRAP_BUNDLE);
  assert(w.slowed());

  // Out in the field nothing is banked.
  w.run(500.0f);
  assert(w.hero().scrapCarried == Arena::Tuning::SCRAP_BUNDLE);
  assert(w.director.shipHealth() == before);

  // Anywhere inside the ring counts: the hull itself is solid.
  w.hero().x = Arena::Tuning::SHIP_DROP_RADIUS - 10.0f;
  w.events.clear();
  w.run(100.0f);
  assert(w.hero().scrapCarried == 0);
  assert(!w.slowed());
  assert(w.director.shipHealth() > before);

  // The payoff is visible: a repair number at the ship and the scrap lobbed
  // onto the hull.
  bool number = false, lob = false;
  for (const GameEventPacket& p : w.events) {
    number = number || (p.kind == GameEventKind::Heal && p.value > 0.0f);
    lob = lob || p.kind == GameEventKind::Lob;
  }
  assert(number && lob);
  std::cout << "  scrap haul: carried, slowed, banked in the ring\n";
}

Objective objective(ObjectiveType type, float x, float y, float radius) {
  Objective o;
  o.id = 1;
  o.type = type;
  o.x = x;
  o.y = y;
  o.radius = radius;
  o.interactionTime = 0.1f;
  return o;
}

void smallScrapAddsUpAndOnlyAFullLoadIsHeavy() {
  namespace T = Arena::Tuning;
  World w;
  w.hero().x = 900.0f;
  w.hero().y = 0.0f;
  // Pocket change from kills: light, and it adds up.
  assert(w.director.onPickup(w.hero(), Arena::PickupId::scrap(3)));
  assert(w.hero().scrapCarried == 3 && !w.slowed());
  assert(w.director.onPickup(w.hero(), Arena::PickupId::scrap(4)));
  assert(w.hero().scrapCarried == 7 && !w.slowed());
  // A full load is heavy...
  w.director.addScrap(w.hero(), T::SCRAP_HEAVY_LOAD - 7);
  assert(w.hero().scrapCarried == T::SCRAP_HEAVY_LOAD && w.slowed());
  // ...until some of it is spent in the field.
  w.director.spendScrap(w.hero(), 1);
  assert(!w.slowed());
  // Banking it all repairs the ship by the amount.
  w.director.damageShip(1000.0f);
  float before = w.director.shipHealth();
  int carried = w.hero().scrapCarried;
  w.hero().x = 0.0f;
  w.hero().y = 150.0f;
  w.run(100.0f);
  assert(w.hero().scrapCarried == 0);
  assert(w.director.scrapInBank() == carried);
  assert(std::abs(w.director.shipHealth() - before -
                  T::SHIP_REPAIR_PER_SCRAP * carried) < 0.5f);
  std::cout << "  scrap: small amounts add up; a full load is heavy\n";
}

void downedPlayersDropTheirWholePile() {
  World w;
  w.hero().x = 900.0f;
  w.hero().y = 0.0f;
  w.director.addScrap(w.hero(), 6);
  w.pickups.clear();
  w.hero().health = 0.0f;
  w.run(50.0f);
  assert(w.hero().scrapCarried == 0);
  assert(w.pickups.size() == 1 && w.pickups[0] == Arena::PickupId::scrap(6));
  std::cout << "  scrap: going down drops the whole pile as one pickup\n";
}

void elitesAlwaysDropScrap() {
  World w;
  Enemy elite = Enemy();
  elite.type = static_cast<Arena::EnemyKind>(0);
  elite.elite = true;
  elite.x = 900.0f;
  elite.y = 0.0f;
  w.pickups.clear();
  w.director.onEnemyKilled(elite, HERO);
  bool scrap = false;
  for (uint32_t id : w.pickups) {
    if (Arena::PickupId::isScrap(id)) {
      assert(Arena::PickupId::scrapAmount(id) ==
             Arena::Tuning::SCRAP_DROP_ELITE);
      scrap = true;
    }
  }
  assert(scrap);
  std::cout << "  scrap: an elite always drops its pile\n";
}

void turretRepairIsPaidInCarriedScrap() {
  namespace T = Arena::Tuning;
  World w;
  w.hero().x = 900.0f;
  w.hero().y = 0.0f;
  assert(!w.director.canAffordTurretRepair(w.hero()));
  w.director.addScrap(w.hero(), T::TURRET_REPAIR_SCRAP + 2);
  assert(w.director.canAffordTurretRepair(w.hero()));
  w.director.payForTurretRepair(w.hero());
  assert(w.hero().scrapCarried == 2);
  assert(w.director.onTurretRepaired(w.hero()));
  std::cout << "  turret: the repair is paid from carried scrap\n";
}

void probeIsDeliveredByWalkingIn() {
  ObjectiveSystem system;
  Objective probe =
      objective(ObjectiveType::RecoverProbe, 1500.0f, 0.0f, 70.0f);
  probe.hasDepositPoint = true;
  probe.depositRadius = Arena::Tuning::SHIP_DROP_RADIUS;
  system.initialize({probe});
  int completions = 0;
  system.setCompletionCallback([&](uint32_t, uint32_t) { completions++; });

  assert(system.tryInteract(HERO, 1500.0f, 0.0f));
  system.update(200.0f);
  assert(system.getObjective(1)->state == ObjectiveState::ReadyToDeposit);

  system.deliverOnArrival(HERO, 600.0f, 0.0f);  // not there yet
  assert(completions == 0);
  system.deliverOnArrival(2, 50.0f, 0.0f);  // someone else is not carrying it
  assert(completions == 0);
  system.deliverOnArrival(HERO, 150.0f, 0.0f);  // inside the ring
  assert(completions == 1);
  std::cout << "  probe: delivered on arrival, by its carrier only\n";
}

void campKillsCountBeforeAnyoneInteracts() {
  ObjectiveSystem system;
  Objective outpost =
      objective(ObjectiveType::CaptureOutpost, 1000.0f, 1000.0f, 140.0f);
  outpost.enemiesRequired = 3;
  system.initialize({outpost});
  int completions = 0;
  uint32_t credited = 0;
  system.setCompletionCallback([&](uint32_t, uint32_t playerId) {
    completions++;
    credited = playerId;
  });

  for (int i = 0; i < 3; i++) {
    system.onEnemyDeath(1000.0f + 40.0f * i, 1000.0f, HERO);
  }
  assert(system.getObjective(1)->state == ObjectiveState::Completed);
  // Nobody pressed interact: the player who landed the last kill is paid.
  assert(completions == 1);
  assert(credited == HERO);
  std::cout << "  camp: cleared by fighting completes and pays the killer\n";
}

void guardianDeathOpensGate() {
  ObjectiveSystem system;
  Objective gate = objective(ObjectiveType::LittleJohn, 0.0f, 0.0f, 110.0f);
  gate.guardianEnemyId = 7;
  system.initialize({gate});
  int completions = 0;
  system.setCompletionCallback([&](uint32_t, uint32_t) { completions++; });

  system.onGuardianDeath(8, HERO);  // some other enemy
  assert(system.getObjective(1)->state != ObjectiveState::Completed);
  system.onGuardianDeath(7, HERO);
  assert(system.getObjective(1)->state == ObjectiveState::Completed);
  assert(completions == 1);
  system.onGuardianDeath(7, HERO);  // already open: nothing twice
  assert(completions == 1);
  std::cout << "  gate: killing the guardian opens it\n";
}

void guardianWaitsUntilWoken() {
  World w;
  Player& hero = w.hero();
  hero.x = 0.0f;
  hero.y = 0.0f;
  uint32_t id = w.enemies.spawn(Arena::EnemyKind::VenoxaridGrunt, 120.0f, 0.0f);
  Enemy& guard = w.enemies.getEnemies().at(id);
  guard.passive = true;
  guard.campGuard = true;
  for (int i = 0; i < 60; i++) w.enemies.update(TICK_MS, w.players, &w.effects);
  assert(std::abs(w.enemies.getEnemies().at(id).x - 120.0f) < 25.0f);
  assert(w.enemies.getEnemies().at(id).targetPlayerId == 0);

  w.enemies.getEnemies().at(id).passive = false;
  for (int i = 0; i < 60; i++) w.enemies.update(TICK_MS, w.players, &w.effects);
  assert(w.enemies.getEnemies().at(id).targetPlayerId == HERO);
  std::cout << "  guardian: waits while passive, hunts once woken\n";
}

}  // namespace

int main() {
  Logger::init();
  std::cout << "Points of interest\n";
  scrapIsCarriedSlowedAndBankedInTheShipRing();
  smallScrapAddsUpAndOnlyAFullLoadIsHeavy();
  downedPlayersDropTheirWholePile();
  elitesAlwaysDropScrap();
  turretRepairIsPaidInCarriedScrap();
  probeIsDeliveredByWalkingIn();
  campKillsCountBeforeAnyoneInteracts();
  guardianDeathOpensGate();
  guardianWaitsUntilWoken();
  std::cout << "All points-of-interest tests passed\n";
  return 0;
}
