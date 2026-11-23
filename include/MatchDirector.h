#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>

#include "TerrainGrid.h"

#include "ArenaData.h"
#include "Enemy.h"
#include "NetworkProtocol.h"
#include "Player.h"

class AbilitySystem;
class EnemySystem;

// Runs a match: lobby -> rounds (roam the planet, fight, bank scrap) ->
// nights (intermission: the crew camps where it stands; the ship is the safe
// camp) -> boss -> victory / defeat.
// It decides *when* and *what* spawns, owns the ship and the scoreboard, and
// hands out loot. How things fight is AbilitySystem / EnemySystem's job.
class MatchDirector {
 public:
  struct Hooks {
    // Drop a pickup in the world (Arena::PickupId).
    std::function<void(uint32_t itemId, float x, float y)> spawnPickup;
    // Remove every pickup lying on the ground.
    std::function<void()> clearPickups;
    // A new match: camps back at full strength, objectives undone.
    std::function<void()> resetWorld;
    // The ship reached a tier (Arena::Tuning::SHIP_TIER_*): its hull
    // footprint changes.
    std::function<void(int tier)> setShipTier;
  };

  MatchDirector(std::unordered_map<uint32_t, Player>& players,
                EnemySystem* enemySystem, AbilitySystem* abilitySystem,
                float shipX, float shipY, float worldWidth, float worldHeight,
                Hooks hooks);

  void update(float dtMs);

  // The map's regions: each is dealt a genus per match (Arena::regionGenus).
  void setTerrain(const TerrainGrid* terrain);
  // A bigger map has a bigger storm: the zone's radii and the "far"
  // objective distance are the Tuning values times this (map property
  // "zone_scale"; 1 on the prototype arena).
  void setZoneScale(float scale);
  // The ship lands somewhere else (a new match on a map with several
  // landing sites): the zone, the raids and the crew follow it.
  void moveShip(float x, float y);
  // The genus that holds this spot of the planet this match.
  Arena::Genus genusAt(float x, float y) const;

  // ---- events from the rest of the server --------------------------------
  void onPlayerJoined(Player& player);
  void onPlayerLeft(uint32_t playerId);
  // characterId 0 means the player went back to the main menu: they leave
  // the fight and may pick any character again.
  void onCharacterSelected(Player& player, uint32_t characterId);
  void onReady(Player& player, bool ready);
  // node: an Arena::suitNode index, or Arena::CORE_NODE.
  void onSlotInfusion(Player& player, Arena::Genus genus,
                      Arena::InfusionPart part, int node);
  void onEnemyKilled(const Enemy& enemy, uint32_t killerId);
  // Returns true if the pickup was consumed.
  bool onPickup(Player& player, uint32_t itemId);
  // salvagedScrap: the player picks up a scrapyard's bundle
  // (Tuning::SCRAP_BUNDLE), carried until banked at the ship or spent.
  void onObjectiveCompleted(Player& player, bool salvagedScrap,
                            float objectiveX, float objectiveY);
  // A Derelict Turret objective was repaired: the next roof turret stands
  // up on the ship. Returns false if every roof turret is already up. The
  // repair's scrap is paid by the caller (payForTurretRepair) before this.
  bool onTurretRepaired(Player& player);
  // A derelict turret repair costs Tuning::TURRET_REPAIR_SCRAP of the
  // repairer's carried scrap: checked when the repair starts, paid when it
  // finishes (walking away costs nothing).
  bool canAffordTurretRepair(const Player& player) const {
    return player.scrapCarried >= Arena::Tuning::TURRET_REPAIR_SCRAP;
  }
  void payForTurretRepair(Player& player);
  // Carried scrap: picked up, then banked at the ship or spent in the field.
  // Carrying Tuning::SCRAP_HEAVY_LOAD or more slows the player.
  void addScrap(Player& player, int amount);
  void spendScrap(Player& player, int amount);
  // Someone started hammering on a derelict turret: a pack comes for the
  // noise, from the side away from the ship.
  void onRepairNoise(float x, float y);
  // Spend banked scrap on the ship's next tier. Intermission only; returns
  // false if it is not intermission, the ship is at its top tier, or the
  // crew has not banked enough.
  bool onShipUpgrade(Player& player);
  void damageShip(float amount);
  void onDebugCommand(Player& player, DebugCommandKind command, uint8_t arg);

  // ---- state -------------------------------------------------------------
  Arena::MatchPhase phase() const { return currentPhase; }
  int round() const { return currentRound; }
  float shipHealth() const { return shipHp; }
  int shipTier() const { return tier; }
  int scrapInBank() const { return scrapBanked; }
  float shipMaxHealth() const {
    return Arena::Tuning::SHIP_TIER_MAX_HEALTH[tier];
  }
  float safeRadius() const {
    return Arena::Tuning::SHIP_TIER_SAFE_RADIUS[tier];
  }
  float zoneRadius() const { return zone; }
  MatchStatePacket buildState() const;
  // True while enemies and abilities are live.
  bool inCombat() const {
    return currentPhase == Arena::MatchPhase::Round ||
           currentPhase == Arena::MatchPhase::BossFight;
  }

 private:
  std::unordered_map<uint32_t, Player>& players;
  const TerrainGrid* terrain = nullptr;
  uint8_t regionGenera[Arena::REGION_SLOTS] = {};
  float zoneScale = 1.0f;
  // Its own stream, seeded fresh per server: the layout differs between
  // runs, while everything rng drives stays reproducible.
  std::mt19937 regionRng{std::random_device{}()};
  void dealRegions();
  EnemySystem* enemies;
  AbilitySystem* abilities;
  float shipX, shipY;
  float worldWidth, worldHeight;
  Hooks hooks;

  Arena::MatchPhase currentPhase = Arena::MatchPhase::Lobby;
  int currentRound = 0;
  float phaseTimeLeft = 0.0f;  // seconds
  float phaseElapsed = 0.0f;   // seconds
  float shipHp = Arena::Tuning::SHIP_START_HEALTH;
  float zone = Arena::Tuning::ZONE_START_RADIUS[0];
  int scrapBanked = 0;  // delivered and not yet spent on a tier
  int tier = 0;         // the ship's upgrade tier
  uint32_t bossId = 0;
  bool bossSpawned = false;

  // Cadence timers (milliseconds)
  float spawnTimer = 0.0f;
  float raidTimer = 0.0f;
  float zoneTickTimer = 0.0f;
  // Roof turrets, stood up by repairing Derelict Turret objectives.
  struct RoofTurret {
    bool online = false;
    float aim = 0.0f;  // radians, clockwise from straight down
    float cooldownMs = 0.0f;
    float recoilMs = 0.0f;
    uint32_t targetId = 0;
  };
  RoofTurret roofTurrets[Arena::Tuning::ROOF_TURRET_COUNT];
  float shipAlertCooldown = 0.0f;
  bool announcedNightfall = false;
  int killsThisRound = 0;
  int infusionDropsThisRound = 0;
  std::unordered_map<uint32_t, int32_t> roundStartScore;
  std::unordered_map<uint32_t, bool> wasDead;
  // Players who left for the main menu during a round. Picking again that
  // round brings them back with abilities on cooldown, and still down if
  // they left down, so the menu is no escape or free refresh.
  struct MenuLeave {
    int round;
    bool down;
  };
  std::unordered_map<uint32_t, MenuLeave> leftForMenu;

  void leaveToMenu(Player& player);

  std::mt19937 rng{4242};

  // Phase transitions
  void startIntro();
  void startRound(int round);
  void endRound();
  void startIntermission();
  void startBossFight();
  void finishMatch(bool victory);
  void resetMatch();

  // Per-phase updates
  void updateLobby(float dtMs);
  void updateIntro(float dtMs);
  void updateRound(float dtMs);
  void updateIntermission(float dtMs);
  void updateBossFight(float dtMs);
  void updateEnd(float dtMs);

  // Shared helpers
  void updateSpawning(float dtMs, bool bossPhase);
  void updateZoneDamage(float dtMs);
  void updateRoofTurrets(float dtMs);
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
  // Night falls where the crew stands: heal by camp, revive or send home
  // the downed, bank scrap only at the ship.
  void makeCamp();
  // Dawn: an ambush near every camp out of the ship's reach.
  void dawnAmbush();
  bool atShip(const Player& player) const;
  void bankScrap(Player& player);
  // Applies or lifts the heavy-load slow after the carried amount changed.
  void updateScrapLoad(Player& player, bool wasHeavy);
  // Drops everything the player carries on the ground where they stand.
  void dropCarriedScrap(Player& player);

  int participantCount() const;
  int livingParticipants() const;
  bool allParticipantsReady() const;
  void runEffectLab(Player& player, EffectLabAction action);
  bool isParticipant(const Player& player) const {
    return player.characterId != 0;
  }
  float random01();
  float randomRange(float lo, float hi);
  Arena::Genus randomGenus();
  Arena::InfusionPart randomPart();
  bool insideWorld(float x, float y) const;
};
