#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>

#include "ArenaData.h"
#include "Enemy.h"
#include "NetworkProtocol.h"
#include "Player.h"

class AbilitySystem;
class EnemySystem;

// Runs a match: lobby -> rounds (deploy, fight, shrinking Blue Zone, get back
// to the ship) -> intermission (slot infusions) -> boss -> victory / defeat.
// It decides *when* and *what* spawns, owns the ship and the scoreboard, and
// hands out loot. How things fight is AbilitySystem / EnemySystem's job.
class MatchDirector {
 public:
  struct Hooks {
    // Drop a pickup in the world (Arena::PickupId).
    std::function<void(uint32_t itemId, float x, float y)> spawnPickup;
    // Remove every pickup lying on the ground.
    std::function<void()> clearPickups;
  };

  MatchDirector(std::unordered_map<uint32_t, Player>& players,
                EnemySystem* enemySystem, AbilitySystem* abilitySystem,
                float shipX, float shipY, float worldWidth, float worldHeight,
                Hooks hooks);

  void update(float dtMs);

  // ---- events from the rest of the server --------------------------------
  void onPlayerJoined(Player& player);
  void onPlayerLeft(uint32_t playerId);
  void onCharacterSelected(Player& player, uint32_t characterId);
  void onReady(Player& player, bool ready);
  void onSlotInfusion(Player& player, Arena::Genus genus, int slot);
  void onEnemyKilled(const Enemy& enemy, uint32_t killerId);
  // Returns true if the pickup was consumed.
  bool onPickup(Player& player, uint32_t itemId);
  void onObjectiveCompleted(Player& player, bool deliveredScrap);
  void damageShip(float amount);
  void onDebugCommand(Player& player, DebugCommandKind command, uint8_t arg);

  // ---- state -------------------------------------------------------------
  Arena::MatchPhase phase() const { return currentPhase; }
  int round() const { return currentRound; }
  float shipHealth() const { return shipHp; }
  float zoneRadius() const { return zone; }
  MatchStatePacket buildState() const;
  // True while enemies and abilities are live.
  bool inCombat() const {
    return currentPhase == Arena::MatchPhase::Round ||
           currentPhase == Arena::MatchPhase::BossFight;
  }

 private:
  std::unordered_map<uint32_t, Player>& players;
  EnemySystem* enemies;
  AbilitySystem* abilities;
  float shipX, shipY;
  float worldWidth, worldHeight;
  Hooks hooks;

  Arena::MatchPhase currentPhase = Arena::MatchPhase::Lobby;
  int currentRound = 0;
  float phaseTimeLeft = 0.0f;  // seconds
  float phaseElapsed = 0.0f;   // seconds
  float shipHp = Arena::Tuning::SHIP_MAX_HEALTH;
  float zone = Arena::Tuning::ZONE_START_RADIUS[0];
  int scrapBanked = 0;
  uint32_t bossId = 0;
  bool bossSpawned = false;
  bool lobbyCountingDown = false;

  // Cadence timers (milliseconds)
  float spawnTimer = 0.0f;
  float raidTimer = 0.0f;
  float zoneTickTimer = 0.0f;
  float turretTimer = 0.0f;
  float shipAlertCooldown = 0.0f;
  bool announcedShrink = false;
  bool announcedReturn = false;
  int killsThisRound = 0;
  int infusionDropsThisRound = 0;
  std::unordered_map<uint32_t, int32_t> roundStartScore;
  std::unordered_map<uint32_t, bool> wasDead;

  std::mt19937 rng{4242};

  // Phase transitions
  void startRound(int round);
  void endRound();
  void startIntermission();
  void startBossFight();
  void finishMatch(bool victory);
  void resetMatch();

  // Per-phase updates
  void updateLobby(float dtMs);
  void updateRound(float dtMs);
  void updateIntermission(float dtMs);
  void updateBossFight(float dtMs);
  void updateEnd(float dtMs);

  // Shared helpers
  void updateSpawning(float dtMs, bool bossPhase);
  void updateZoneDamage(float dtMs);
  void updateTurrets(float dtMs);
  void updateDeathsAndRevives(float dtMs);
  void updateScrapDelivery();
  // genusOverride / swarmersOnly are for playtest cheats.
  void spawnPack(float x, float y, int size, bool targetShip,
                 Arena::Genus genusOverride = Arena::Genus::None,
                 bool swarmersOnly = false);
  void spawnRaid();
  void spawnBoss();
  void dropLoot(const Enemy& enemy);
  void bringPlayersHome(bool reviveAndHeal);
  void bankScrap(Player& player);

  int participantCount() const;
  int livingParticipants() const;
  bool allParticipantsReady() const;
  bool isParticipant(const Player& player) const {
    return player.characterId != 0;
  }
  float random01();
  float randomRange(float lo, float hi);
  bool insideWorld(float x, float y) const;
};
