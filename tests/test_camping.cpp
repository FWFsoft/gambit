// Night falls where the crew stands: no Blue Zone in a round, nobody is
// teleported home or killed for being out. The ship is the safe camp (full
// heal, scrap banked, suit and ship upgrades); a camp in the wild heals
// part-way (more with crewmates), keeps scrap in hand, carries the downed in,
// and draws an ambush at dawn. Nobody walks at night.

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
#include "Infusions.h"
#include "MatchDirector.h"

namespace {

constexpr float TICK_MS = 1000.0f / 60.0f;
constexpr uint32_t A = 1, B = 2, C = 3;
namespace T = Arena::Tuning;

struct World {
  std::unordered_map<uint32_t, Player> players;
  std::vector<EnemySpawn> noSpawns;
  EnemySystem enemies{noSpawns};
  EffectManager effects;
  AbilitySystem combat;
  MatchDirector director;

  World()
      : combat(players, &enemies, &effects, nullptr, 20000.0f, 20000.0f,
               [](const GameEventPacket&) {}),
        director(players, &enemies, &combat, 0.0f, 0.0f, 20000.0f, 20000.0f,
                 MatchDirector::Hooks()) {
    enemies.setCombat(&combat);
    for (uint32_t id : {A, B, C}) {
      Player p;
      p.id = id;
      players[id] = p;
      director.onPlayerJoined(players[id]);
      director.onCharacterSelected(players[id], Arena::CharacterId::Wade);
      director.onReady(players[id], true);
    }
    run(Arena::Intro::LENGTH * 1000.0f + 500.0f);
    assert(director.phase() == Arena::MatchPhase::Round);
  }
  Player& p(uint32_t id) { return players[id]; }
  void place(uint32_t id, float x, float y) {
    p(id).x = x;
    p(id).y = y;
  }
  void run(float ms) {
    for (float t = 0.0f; t < ms; t += TICK_MS) {
      combat.update(TICK_MS);
      director.update(TICK_MS);
    }
  }
  void nightfall() {
    director.onDebugCommand(p(A), DebugCommandKind::SkipPhase, 0);
    run(100.0f);
    assert(director.phase() == Arena::MatchPhase::Intermission);
  }
  int livingEnemies() {
    int n = 0;
    for (const auto& [id, e] : enemies.getEnemies()) {
      if (e.state != EnemyState::Dead) n++;
    }
    return n;
  }
};

void noZoneDamageFarOut() {
  World w;
  w.place(A, 6000.0f, 6000.0f);  // far past the old zone
  float before = w.p(A).health;
  w.run(T::ROUND_SECONDS[0] * 1000.0f * 0.9f);
  assert(w.director.phase() == Arena::MatchPhase::Round);
  assert(w.p(A).health == before);
  std::cout << "  round: no Blue Zone, the planet is open\n";
}

void nightFallsWhereYouStand() {
  World w;
  w.place(A, 0.0f, 150.0f);       // at the ship
  w.place(B, 5000.0f, 5000.0f);   // alone in the wild
  w.place(C, 9000.0f, 1000.0f);   // alone elsewhere
  w.p(A).health = 10.0f;
  w.p(B).health = 10.0f;
  w.p(B).scrapCarried = 5;
  w.nightfall();

  // Nobody moved, nobody died.
  assert(w.p(B).x == 5000.0f && w.p(B).y == 5000.0f);
  assert(!w.p(C).isDead());
  // The ship heals fully; the wild heals part-way and keeps the scrap.
  assert(w.p(A).health == w.p(A).maxHealth);
  float expected = 10.0f + w.p(B).maxHealth * T::CAMP_HEAL_FRACTION;
  assert(std::abs(w.p(B).health - expected) < 0.5f);
  assert(w.p(B).scrapCarried == 5);
  assert(w.director.scrapInBank() == 0);

  // Nobody walks at night.
  w.run(500.0f);
  assert(w.p(B).speedMult == 0.0f);
  std::cout << "  night: camp where you stand; the ship heals fully\n";
}

void campingTogetherRestsBetter() {
  World w;
  w.place(A, 5000.0f, 5000.0f);
  w.place(B, 5200.0f, 5000.0f);  // in A's camp
  w.place(C, 9000.0f, 9000.0f);
  w.p(A).health = 1.0f;
  w.p(C).health = 1.0f;
  w.nightfall();
  assert(w.p(A).health > w.p(C).health);
  std::cout << "  night: a shared camp heals more than a lonely one\n";
}

void downedAreCarriedInOrWakeAtShip() {
  World w;
  w.place(A, 5000.0f, 5000.0f);
  w.place(B, 5100.0f, 5000.0f);   // down beside A
  w.place(C, 12000.0f, 12000.0f); // down and alone
  w.p(B).health = 0.0f;
  w.p(C).health = 0.0f;
  w.p(C).scrapCarried = 5;
  w.nightfall();
  assert(!w.p(B).isDead() && w.p(B).x == 5100.0f);
  assert(w.p(B).health < w.p(B).maxHealth);
  assert(!w.p(C).isDead());
  assert(std::hypot(w.p(C).x, w.p(C).y) < w.director.safeRadius());
  assert(w.p(C).scrapCarried == 0);
  std::cout << "  night: the downed are carried into camp, or wake at the ship\n";
}

void suitAndUpgradesOnlyAtTheShip() {
  World w;
  w.place(A, 5000.0f, 5000.0f);
  w.place(B, 0.0f, 150.0f);
  w.director.onDebugCommand(w.p(A), DebugCommandKind::RoofTurrets, 0);  // scrap
  w.nightfall();
  assert(!w.director.onShipUpgrade(w.p(A)));
  assert(w.director.onShipUpgrade(w.p(B)));
  std::cout << "  night: the ship is built by whoever camped beside it\n";
}

void fieldStationOpensTheSuitInTheWild() {
  // Issue #45: 8 carried scrap sets one up; until dawn everyone within
  // 500 px of it can install and combine, as at the ship.
  namespace T = Arena::Tuning;
  using Arena::Genus;
  using Arena::InfusionPart;
  World w;
  w.place(A, 5000.0f, 5000.0f);
  w.place(B, 5000.0f + T::FIELD_STATION_RADIUS, 5000.0f);  // inclusive edge
  w.place(C, 5000.0f + T::FIELD_STATION_RADIUS + 1.0f, 5000.0f);  // outside
  // Not by day.
  w.director.addScrap(w.p(A), T::FIELD_STATION_SCRAP);
  assert(!w.director.onBuildFieldStation(w.p(A)));
  w.nightfall();
  for (uint32_t id : {A, B, C}) {
    assert(Infusions::addToInventory(w.p(id), Genus::Venoxarid,
                                      InfusionPart::Talon));
  }
  // Locked in the wild until a station stands.
  w.director.onSlotInfusion(w.p(B), Genus::Venoxarid, InfusionPart::Talon, 1,
                            Arena::CORE_NODE);
  assert(w.p(B).coreGenus == Genus::None);
  // One short: refused, nothing spent.
  w.director.spendScrap(w.p(A), 1);
  assert(!w.director.onBuildFieldStation(w.p(A)));
  assert(w.p(A).scrapCarried == T::FIELD_STATION_SCRAP - 1);
  w.director.addScrap(w.p(A), 1);
  assert(w.director.onBuildFieldStation(w.p(A)));
  assert(w.p(A).scrapCarried == 0);
  assert(w.director.buildState().stations.size() == 1);
  // Paid once: a second request builds nothing more.
  w.director.addScrap(w.p(A), T::FIELD_STATION_SCRAP);
  assert(!w.director.onBuildFieldStation(w.p(A)));
  assert(w.p(A).scrapCarried == T::FIELD_STATION_SCRAP);
  // A crewmate inside it has no reason to build another.
  w.director.addScrap(w.p(B), T::FIELD_STATION_SCRAP);
  assert(!w.director.onBuildFieldStation(w.p(B)));
  // Shared: the builder and B (on the edge) can install; C cannot.
  assert(w.director.atFieldStation(w.p(A)) && w.director.atFieldStation(w.p(B)));
  assert(!w.director.atFieldStation(w.p(C)));
  w.director.onSlotInfusion(w.p(B), Genus::Venoxarid, InfusionPart::Talon, 1,
                            Arena::CORE_NODE);
  assert(w.p(B).coreGenus == Genus::Venoxarid);
  w.director.onSlotInfusion(w.p(C), Genus::Venoxarid, InfusionPart::Talon, 1,
                            Arena::CORE_NODE);
  assert(w.p(C).coreGenus == Genus::None);
  // No ship upgrades from the field.
  w.director.onDebugCommand(w.p(A), DebugCommandKind::RoofTurrets, 0);
  assert(!w.director.onShipUpgrade(w.p(A)));
  // Gone at dawn: the next night needs another.
  w.director.onDebugCommand(w.p(A), DebugCommandKind::SkipPhase, 0);
  w.run(100.0f);
  assert(w.director.phase() == Arena::MatchPhase::Round);
  assert(w.director.buildState().stations.empty());
  assert(!w.director.atFieldStation(w.p(A)));
  std::cout << "  night: a field station opens the suit for the camp\n";
}

void dawnAmbushesWildCampsOnly() {
  World w;
  w.place(A, 0.0f, 150.0f);
  w.place(B, 5000.0f, 5000.0f);
  w.place(C, 5150.0f, 5000.0f);  // same camp as B: one ambush
  w.nightfall();
  w.director.onDebugCommand(w.p(A), DebugCommandKind::SkipPhase, 0);
  w.run(100.0f);
  assert(w.director.phase() == Arena::MatchPhase::Round);
  assert(w.director.round() == 2);
  // Woke where they camped.
  assert(w.p(B).x == 5000.0f && w.p(B).y == 5000.0f);
  int near = 0, nearShip = 0;
  for (const auto& [id, e] : w.enemies.getEnemies()) {
    if (e.state == EnemyState::Dead) continue;
    if (std::hypot(e.x - 5075.0f, e.y - 5000.0f) < 800.0f) near++;
    if (std::hypot(e.x, e.y) < 800.0f) nearShip++;
  }
  assert(near > 0);
  assert(nearShip == 0);
  assert(w.p(B).speedMult > 0.0f);
  std::cout << "  dawn: an ambush at the wild camp, none at the ship\n";
}

void readinessSkipsWaitButAfkCannotStall() {
  World w;
  w.nightfall();
  for (uint32_t id : {A, B, C}) assert(!w.p(id).ready);
  // The old 40 s cap must no longer interrupt reading.
  w.run(41000.0f);
  assert(w.director.phase() == Arena::MatchPhase::Intermission);
  w.director.onReady(w.p(A), true);
  w.director.onReady(w.p(B), true);
  w.run(TICK_MS);
  assert(w.director.phase() == Arena::MatchPhase::Intermission);
  w.director.onReady(w.p(B), false);  // players may change their mind
  w.director.onReady(w.p(C), true);
  w.run(TICK_MS);
  assert(w.director.phase() == Arena::MatchPhase::Intermission);
  w.director.onReady(w.p(B), true);
  w.director.update(TICK_MS);  // no countdown after the final ready
  assert(w.director.phase() == Arena::MatchPhase::Round);
  assert(w.director.round() == 2);

  World afk;
  afk.nightfall();
  afk.director.onReady(afk.p(A), true);
  afk.director.onReady(afk.p(B), true);
  afk.run((T::INTERMISSION_SECONDS - 1.0f) * 1000.0f);
  assert(afk.director.phase() == Arena::MatchPhase::Intermission);
  afk.run(1100.0f);
  assert(afk.director.phase() == Arena::MatchPhase::Round);
  assert(afk.director.round() == 2);
  std::cout << "  night: longer reading cap, all-ready skips, AFK cannot stall\n";
}

}  // namespace

int main() {
  Logger::init();
  std::cout << "Camping\n";
  noZoneDamageFarOut();
  nightFallsWhereYouStand();
  campingTogetherRestsBetter();
  downedAreCarriedInOrWakeAtShip();
  suitAndUpgradesOnlyAtTheShip();
  dawnAmbushesWildCampsOnly();
  fieldStationOpensTheSuitInTheWild();
  readinessSkipsWaitButAfkCannotStall();
  std::cout << "All camping tests passed\n";
  return 0;
}
