// Going back to the main menu mid-match lets a player pick a new character.
// The client sends characterId 0 ("left for the menu"); the server takes the
// player out of the fight, and the next pick drops them in at the ship.
// Leaving while down is no escape: they come back down where they fell.

#include <cassert>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "AbilitySystem.h"
#include "ArenaData.h"
#include "EffectManager.h"
#include "EnemySystem.h"
#include "Logger.h"
#include "MatchDirector.h"

namespace {

constexpr float TICK_MS = 1000.0f / 60.0f;
constexpr uint32_t HERO = 1;
constexpr uint32_t ALLY = 2;

struct World {
  std::unordered_map<uint32_t, Player> players;
  std::vector<EnemySpawn> noSpawns;
  EnemySystem enemies{noSpawns};
  EffectManager effects;
  AbilitySystem combat{players,  &enemies, &effects,
                       nullptr,  8000.0f,  4000.0f,
                       [](const GameEventPacket&) {}};
  MatchDirector director{players, &enemies, &combat, 0.0f,
                         0.0f,    8000.0f,  4000.0f, MatchDirector::Hooks{}};

  World() {
    enemies.setCombat(&combat);
    for (uint32_t id : {HERO, ALLY}) {
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

  Player& hero() { return players[HERO]; }
  void run(float ms) {
    for (float t = 0.0f; t < ms; t += TICK_MS) {
      combat.update(TICK_MS);
      director.update(TICK_MS);
    }
  }
};

void pickIsLockedInTheFight() {
  World w;
  w.director.onCharacterSelected(w.hero(), Arena::CharacterId::Nyx);
  assert(w.hero().characterId == Arena::CharacterId::Wade);
  std::cout << "  pick is locked during a round\n";
}

void mainMenuFreesThePick() {
  World w;
  w.director.onCharacterSelected(w.hero(), 0);
  assert(w.hero().characterId == 0);
  w.director.onCharacterSelected(w.hero(), Arena::CharacterId::Nyx);
  assert(w.hero().characterId == Arena::CharacterId::Nyx);
  assert(w.hero().isAlive());
  const Arena::KitInfo* nyx = Arena::kitInfo(Arena::CharacterId::Nyx);
  assert(w.hero().maxHealth == nyx->maxHealth);
  // No free cooldown refresh: the new kit starts on cooldown.
  for (int slot = 0; slot < Arena::KIT_SLOT_COUNT; slot++) {
    assert(w.hero().cooldownMs[slot] == nyx->abilities[slot].cooldownMs);
  }
  std::cout << "  main menu frees the pick mid-round\n";
}

void leavingWhileDownComesBackDown() {
  World w;
  w.hero().x = 2500.0f;
  w.hero().y = 1800.0f;
  w.hero().health = 0.0f;
  w.run(100.0f);
  assert(w.hero().isDead());
  int deaths = w.hero().deaths;

  w.director.onCharacterSelected(w.hero(), 0);
  w.director.onCharacterSelected(w.hero(), Arena::CharacterId::Nyx);
  assert(w.hero().characterId == Arena::CharacterId::Nyx);
  assert(w.hero().isDead());
  assert(w.hero().x == 2500.0f && w.hero().y == 1800.0f);
  w.run(100.0f);
  assert(w.hero().deaths == deaths);  // not counted twice
  std::cout << "  leaving while down comes back down\n";
}

}  // namespace

int main() {
  Logger::init();
  std::cout << "Character swap\n";
  pickIsLockedInTheFight();
  mainMenuFreesThePick();
  leavingWhileDownComesBackDown();
  std::cout << "All character-swap tests passed\n";
  return 0;
}
