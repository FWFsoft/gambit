#include "MatchDirector.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "AbilitySystem.h"
#include "EffectManager.h"
#include "EnemySystem.h"
#include "Infusions.h"
#include "Logger.h"

using Arena::EnemyKind;
using Arena::EnemyTier;
using Arena::Genus;
using Arena::MatchPhase;
namespace Tuning = Arena::Tuning;

namespace {

constexpr float PI = 3.14159265f;
constexpr float END_SCREEN_SECONDS = 8.0f;
constexpr float MIN_SPAWN_DISTANCE = 430.0f;   // from any living player
constexpr float MAX_SPAWN_DISTANCE = 1050.0f;  // from the nearest player
constexpr float TURRET_RANGE = 380.0f;
constexpr float TURRET_DAMAGE = 14.0f;
constexpr float TURRET_INTERVAL_MS = 700.0f;

float dist(float x0, float y0, float x1, float y1) {
  float dx = x1 - x0;
  float dy = y1 - y0;
  return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

MatchDirector::MatchDirector(std::unordered_map<uint32_t, Player>& players,
                             EnemySystem* enemySystem,
                             AbilitySystem* abilitySystem, float shipX,
                             float shipY, float worldWidth, float worldHeight,
                             Hooks hooks)
    : players(players),
      enemies(enemySystem),
      abilities(abilitySystem),
      shipX(shipX),
      shipY(shipY),
      worldWidth(worldWidth),
      worldHeight(worldHeight),
      hooks(std::move(hooks)) {
  enemies->setShip(shipX, shipY, false);
  enemies->setShipDamageCallback([this](float amount) { damageShip(amount); });
  abilities->setCombatEnabled(false);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

float MatchDirector::random01() {
  return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
}

float MatchDirector::randomRange(float lo, float hi) {
  return std::uniform_real_distribution<float>(lo, hi)(rng);
}

bool MatchDirector::insideWorld(float x, float y) const {
  float halfWidth = worldWidth / 2.0f;
  float halfHeight = worldHeight / 2.0f;
  if (halfWidth <= 0.0f || halfHeight <= 0.0f) return true;
  return std::abs(x) / halfWidth + std::abs(y) / halfHeight <= 0.96f;
}

int MatchDirector::participantCount() const {
  int count = 0;
  for (const auto& [id, player] : players) {
    if (isParticipant(player)) count++;
  }
  return count;
}

int MatchDirector::livingParticipants() const {
  int count = 0;
  for (const auto& [id, player] : players) {
    if (isParticipant(player) && player.isAlive()) count++;
  }
  return count;
}

bool MatchDirector::allParticipantsReady() const {
  int count = 0;
  for (const auto& [id, player] : players) {
    if (!isParticipant(player)) continue;
    if (!player.ready) return false;
    count++;
  }
  return count > 0;
}

void MatchDirector::bringPlayersHome(bool reviveAndHeal) {
  int index = 0;
  int count = std::max(1, participantCount());
  for (auto& [id, player] : players) {
    if (!isParticipant(player)) continue;
    float angle = (2.0f * PI * index) / count + PI * 0.5f;
    index++;
    player.x = shipX + std::cos(angle) * 150.0f;
    player.y = shipY + std::sin(angle) * 110.0f + 40.0f;
    player.vx = player.vy = 0.0f;
    // A blip of forced motion tells clients not to predict across the jump.
    player.forcedVx = player.forcedVy = 0.0f;
    player.forcedMs = 100.0f;
    if (reviveAndHeal) {
      player.health = player.maxHealth;
      player.deathTime = 0.0f;
      player.reviveProgress = 0.0f;
      wasDead[id] = false;
    }
  }
}

// ---------------------------------------------------------------------------
// Phase transitions
// ---------------------------------------------------------------------------

void MatchDirector::startRound(int round) {
  currentRound = round;
  currentPhase = MatchPhase::Round;
  phaseTimeLeft = Tuning::ROUND_SECONDS[round - 1];
  phaseElapsed = 0.0f;
  zone = Tuning::ZONE_START_RADIUS[round - 1];
  spawnTimer = 800.0f;
  raidTimer = Tuning::RAID_FIRST_MS;
  zoneTickTimer = 0.0f;
  announcedShrink = false;
  announcedReturn = false;
  killsThisRound = 0;
  infusionDropsThisRound = 0;
  bossId = 0;
  bossSpawned = false;

  enemies->despawnAll();
  abilities->effects().clearAllEnemies();
  abilities->clearTransient();
  if (hooks.clearPickups) hooks.clearPickups();

  bringPlayersHome(true);
  for (auto& [id, player] : players) {
    if (!isParticipant(player)) continue;
    abilities->resetPlayerForRound(player);
    player.ready = false;
    player.carryingScrap = false;
    roundStartScore[id] = player.score;
  }

  enemies->setShip(shipX, shipY, true);
  abilities->setCombatEnabled(true);
  abilities->fxAnnounce(Announcement::RoundStart);
  Logger::info("Match: round " + std::to_string(round) + " started");
}

void MatchDirector::endRound() {
  // Night falls. Anyone who is not back at the ship is lost to the dark.
  for (auto& [id, player] : players) {
    if (!isParticipant(player) || player.isDead()) continue;
    if (dist(player.x, player.y, shipX, shipY) > Tuning::SHIP_SAFE_RADIUS) {
      player.health = 0.0f;
      player.deaths++;
      player.score += Tuning::SCORE_DEATH;
    }
  }

  // "When I win, we win": the round's top scorer earns an extra infusion.
  uint32_t mvp = 0;
  int32_t best = 0;
  for (auto& [id, player] : players) {
    if (!isParticipant(player)) continue;
    int32_t gained = player.score - roundStartScore[id];
    if (gained > best) {
      best = gained;
      mvp = id;
    }
  }
  if (mvp != 0 && participantCount() > 1) {
    int genus = std::uniform_int_distribution<int>(0, Arena::GENUS_COUNT - 1)(rng);
    players[mvp].infusionInventory[genus]++;
  }

  if (currentRound >= Tuning::ROUND_COUNT) {
    startIntermission();  // one last breather before the boss
  } else {
    startIntermission();
  }
}

void MatchDirector::startIntermission() {
  currentPhase = MatchPhase::Intermission;
  phaseTimeLeft = Tuning::INTERMISSION_SECONDS;
  phaseElapsed = 0.0f;

  enemies->despawnAll();
  abilities->effects().clearAllEnemies();
  abilities->clearTransient();
  abilities->setCombatEnabled(false);
  enemies->setShip(shipX, shipY, false);
  if (hooks.clearPickups) hooks.clearPickups();

  bringPlayersHome(true);
  for (auto& [id, player] : players) {
    if (!isParticipant(player)) continue;
    abilities->resetPlayerForRound(player);
    player.ready = false;
    // Scrap in hand when the round ends still counts.
    if (player.carryingScrap) bankScrap(player);
  }
  abilities->fxAnnounce(Announcement::Intermission);
  Logger::info("Match: intermission after round " +
               std::to_string(currentRound));
}

void MatchDirector::startBossFight() {
  currentPhase = MatchPhase::BossFight;
  currentRound = Tuning::ROUND_COUNT + 1;
  phaseTimeLeft = 0.0f;
  phaseElapsed = 0.0f;
  zone = Tuning::BOSS_ZONE_RADIUS;
  spawnTimer = 6000.0f;
  bossSpawned = false;
  bossId = 0;

  enemies->despawnAll();
  abilities->clearTransient();
  bringPlayersHome(true);
  for (auto& [id, player] : players) {
    if (!isParticipant(player)) continue;
    abilities->resetPlayerForRound(player);
    player.ready = false;
  }
  // The ship is beyond saving in the finale: nothing left to raid.
  enemies->setShip(shipX, shipY, false);
  abilities->setCombatEnabled(true);
  abilities->fxAnnounce(Announcement::BossIncoming);
  Logger::info("Match: boss fight started");
}

void MatchDirector::finishMatch(bool victory) {
  currentPhase = victory ? MatchPhase::Victory : MatchPhase::Defeat;
  phaseTimeLeft = END_SCREEN_SECONDS;
  phaseElapsed = 0.0f;
  abilities->setCombatEnabled(false);
  abilities->clearTransient();
  enemies->setShip(shipX, shipY, false);
  if (victory) {
    for (auto& [id, player] : players) {
      if (isParticipant(player)) player.score += Tuning::SCORE_BOSS;
    }
  } else {
    enemies->despawnAll();
  }
  for (auto& [id, player] : players) player.ready = false;
  abilities->fxAnnounce(victory ? Announcement::Victory
                                : Announcement::Defeat);
  Logger::info(std::string("Match: ") + (victory ? "VICTORY" : "DEFEAT"));
}

void MatchDirector::resetMatch() {
  currentPhase = MatchPhase::Lobby;
  currentRound = 0;
  phaseTimeLeft = 0.0f;
  lobbyCountingDown = false;
  shipHp = Tuning::SHIP_MAX_HEALTH;
  scrapBanked = 0;
  bossId = 0;
  bossSpawned = false;
  zone = Tuning::ZONE_START_RADIUS[0];
  enemies->despawnAll();
  abilities->clearTransient();
  abilities->setCombatEnabled(false);
  if (hooks.clearPickups) hooks.clearPickups();

  for (auto& [id, player] : players) {
    player.score = 0;
    player.scoreAccum = 0.0f;
    player.kills = 0;
    player.deaths = 0;
    player.ready = false;
    player.carryingScrap = false;
    player.coreGenus = Genus::None;
    for (int i = 0; i < Arena::TREE_COUNT; i++) {
      player.treeGenus[i] = Genus::None;
      player.treeLevel[i] = 0;
      player.infusionCounter[i] = 0;
    }
    for (uint8_t& count : player.infusionInventory) count = 0;
    if (isParticipant(player)) abilities->applyKitStats(player);
  }
  bringPlayersHome(true);
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void MatchDirector::update(float dtMs) {
  // Fractional score becomes whole points.
  for (auto& [id, player] : players) {
    if (player.scoreAccum >= 1.0f) {
      int whole = static_cast<int>(player.scoreAccum);
      player.score += whole;
      player.scoreAccum -= whole;
    }
  }
  shipAlertCooldown = std::max(0.0f, shipAlertCooldown - dtMs);

  switch (currentPhase) {
    case MatchPhase::Lobby:
      updateLobby(dtMs);
      break;
    case MatchPhase::Round:
      updateRound(dtMs);
      break;
    case MatchPhase::Intermission:
      updateIntermission(dtMs);
      break;
    case MatchPhase::BossFight:
      updateBossFight(dtMs);
      break;
    case MatchPhase::Victory:
    case MatchPhase::Defeat:
      updateEnd(dtMs);
      break;
  }
}

void MatchDirector::updateLobby(float dtMs) {
  // Start once everyone connected has picked a character.
  int total = static_cast<int>(players.size());
  int picked = participantCount();
  bool everyonePicked = total > 0 && picked == total;

  if (!everyonePicked) {
    lobbyCountingDown = false;
    phaseTimeLeft = 0.0f;
    return;
  }
  if (!lobbyCountingDown) {
    lobbyCountingDown = true;
    phaseTimeLeft = Tuning::LOBBY_COUNTDOWN_SECONDS;
  }
  phaseTimeLeft -= dtMs / 1000.0f;
  if (phaseTimeLeft <= 0.0f) {
    lobbyCountingDown = false;
    startRound(1);
  }
}

void MatchDirector::updateRound(float dtMs) {
  float dt = dtMs / 1000.0f;
  phaseTimeLeft -= dt;
  phaseElapsed += dt;

  // The Blue Zone holds, then closes in on the ship.
  float total = Tuning::ROUND_SECONDS[currentRound - 1];
  float startRadius = Tuning::ZONE_START_RADIUS[currentRound - 1];
  float holdUntil = total * Tuning::ZONE_HOLD_FRACTION;
  if (phaseElapsed <= holdUntil) {
    zone = startRadius;
  } else {
    float t = std::min(1.0f, (phaseElapsed - holdUntil) / (total - holdUntil));
    zone = startRadius + (Tuning::ZONE_END_RADIUS - startRadius) * t;
    if (!announcedShrink) {
      announcedShrink = true;
      abilities->fxAnnounce(Announcement::ZoneShrinking);
    }
  }
  if (!announcedReturn && phaseTimeLeft <= 25.0f) {
    announcedReturn = true;
    abilities->fxAnnounce(Announcement::ReturnToShip);
  }

  updateSpawning(dtMs, false);
  updateZoneDamage(dtMs);
  updateTurrets(dtMs);
  updateScrapDelivery();
  updateDeathsAndRevives(dtMs);

  if (shipHp <= 0.0f) {
    finishMatch(false);
    return;
  }
  if (participantCount() > 0 && livingParticipants() == 0) {
    finishMatch(false);
    return;
  }
  if (phaseTimeLeft <= 0.0f) endRound();
}

void MatchDirector::updateIntermission(float dtMs) {
  phaseTimeLeft -= dtMs / 1000.0f;
  if (phaseTimeLeft <= 0.0f || allParticipantsReady()) {
    if (currentRound >= Tuning::ROUND_COUNT) {
      startBossFight();
    } else {
      startRound(currentRound + 1);
    }
  }
}

void MatchDirector::updateBossFight(float dtMs) {
  phaseElapsed += dtMs / 1000.0f;

  if (!bossSpawned && phaseElapsed >= 3.5f) spawnBoss();

  if (bossSpawned) {
    Enemy* boss = enemies->find(bossId);
    if (!boss || boss->state == EnemyState::Dead) {
      finishMatch(true);
      return;
    }
  }

  updateSpawning(dtMs, true);
  updateZoneDamage(dtMs);
  updateDeathsAndRevives(dtMs);

  if (participantCount() > 0 && livingParticipants() == 0) {
    finishMatch(false);
  }
}

void MatchDirector::updateEnd(float dtMs) {
  phaseTimeLeft = std::max(0.0f, phaseTimeLeft - dtMs / 1000.0f);
  // Everyone readies up (after the banner has had a moment) to play again.
  if (phaseTimeLeft <= 0.0f && allParticipantsReady()) resetMatch();
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------

void MatchDirector::updateSpawning(float dtMs, bool bossPhase) {
  int participants = std::max(1, participantCount());
  // Solo has nobody to revive you, so it gets a lighter load.
  float playerScale = participants == 1 ? 0.65f : 0.55f + 0.45f * participants;

  spawnTimer -= dtMs;
  if (spawnTimer <= 0.0f) {
    spawnTimer = Tuning::SPAWN_INTERVAL_MS -
                 400.0f * (std::min(currentRound, Tuning::ROUND_COUNT) - 1);

    int target;
    if (bossPhase) {
      target = static_cast<int>(4 * playerScale) + (bossSpawned ? 1 : 0);
    } else {
      float total = Tuning::ROUND_SECONDS[currentRound - 1];
      float ramp = 1.0f + Tuning::ENEMIES_RAMP * (phaseElapsed / total);
      target = static_cast<int>(
          (Tuning::ENEMIES_BASE + Tuning::ENEMIES_PER_ROUND * currentRound) *
          playerScale * ramp);
    }

    if (enemies->livingCount() < target) {
      // Pick a point inside the zone, away from players but not so far that
      // the pack never finds anyone.
      for (int attempt = 0; attempt < 12; attempt++) {
        float angle = randomRange(0.0f, 2.0f * PI);
        float inner = 320.0f;
        float outer = std::max(inner + 60.0f, zone - 60.0f);
        float radius = std::sqrt(randomRange(inner * inner, outer * outer));
        float x = shipX + std::cos(angle) * radius;
        float y = shipY + std::sin(angle) * radius;
        if (!insideWorld(x, y)) continue;

        float nearest = 1e9f;
        for (const auto& [id, player] : players) {
          if (!isParticipant(player) || player.isDead()) continue;
          nearest = std::min(nearest, dist(x, y, player.x, player.y));
        }
        if (nearest < MIN_SPAWN_DISTANCE) continue;
        if (nearest > MAX_SPAWN_DISTANCE && attempt < 9) continue;

        int size = currentRound <= 1
                       ? std::uniform_int_distribution<int>(2, 4)(rng)
                       : std::uniform_int_distribution<int>(3, 5)(rng);
        spawnPack(x, y, size, false);
        break;
      }
    }
  }

  if (!bossPhase) {
    raidTimer -= dtMs;
    if (raidTimer <= 0.0f) {
      raidTimer = Tuning::RAID_INTERVAL_MS;
      spawnRaid();
    }
  }
}

void MatchDirector::spawnPack(float x, float y, int size, bool targetShip,
                              Genus genusOverride, bool swarmersOnly) {
  Genus genus = genusOverride != Genus::None
                    ? genusOverride
                    : Arena::genusForDirection(x - shipX, y - shipY);
  int round = std::min(currentRound, Tuning::ROUND_COUNT);

  // Later rounds mix in more casters and brutes.
  static const float CASTER_CHANCE[3] = {0.20f, 0.30f, 0.32f};
  static const float BRUTE_CHANCE[3] = {0.03f, 0.10f, 0.17f};
  float healthScale = 1.0f + 0.2f * (round - 1);
  float damageScale = 1.0f + 0.15f * (round - 1);

  VfxTheme theme = VfxTheme::Poison;
  if (genus == Genus::Cerebrian) theme = VfxTheme::Psychic;
  if (genus == Genus::Frigidariant) theme = VfxTheme::Ice;
  if (genus == Genus::Magnarok) theme = VfxTheme::Fire;

  for (int i = 0; i < size; i++) {
    float roll = random01();
    EnemyTier tier = EnemyTier::Swarmer;
    if (roll < BRUTE_CHANCE[round - 1]) {
      tier = EnemyTier::Brute;
    } else if (roll < BRUTE_CHANCE[round - 1] + CASTER_CHANCE[round - 1]) {
      tier = EnemyTier::Caster;
    }
    if (targetShip && tier == EnemyTier::Brute) tier = EnemyTier::Swarmer;
    if (swarmersOnly) tier = EnemyTier::Swarmer;

    EnemySystem::SpawnOptions options;
    options.targetShip = targetShip;
    options.healthScale = healthScale;
    options.damageScale = damageScale;
    options.elite =
        tier != EnemyTier::Swarmer && random01() < 0.04f * currentRound;

    float ox = randomRange(-70.0f, 70.0f);
    float oy = randomRange(-70.0f, 70.0f);
    enemies->spawn(Arena::enemyKindFor(genus, tier), x + ox, y + oy, options);

    GameEventPacket puff;
    puff.kind = GameEventKind::SpawnPuff;
    puff.x = x + ox;
    puff.y = y + oy;
    puff.param2 = static_cast<uint8_t>(theme);
    abilities->emit(puff);
  }
}

void MatchDirector::spawnRaid() {
  // A pack marches on the ship from the edge of the zone.
  for (int attempt = 0; attempt < 12; attempt++) {
    float angle = randomRange(0.0f, 2.0f * PI);
    float radius = std::max(450.0f, zone - 40.0f);
    float x = shipX + std::cos(angle) * radius;
    float y = shipY + std::sin(angle) * radius;
    if (!insideWorld(x, y)) continue;
    int participants = std::max(1, participantCount());
    int size = 2 + currentRound + participants;
    spawnPack(x, y, size, true);
    return;
  }
}

void MatchDirector::spawnBoss() {
  bossSpawned = true;
  float angle = randomRange(0.0f, 2.0f * PI);
  float x = shipX + std::cos(angle) * 520.0f;
  float y = shipY + std::sin(angle) * 380.0f;
  if (!insideWorld(x, y)) {
    x = shipX;
    y = shipY - 380.0f;
  }
  EnemySystem::SpawnOptions options;
  options.healthScale = 1.0f + 0.7f * (std::max(1, participantCount()) - 1);
  bossId = enemies->spawn(EnemyKind::Boss, x, y, options);
  abilities->fxRing(x, y, 260.0f, VfxTheme::Shadow);
  abilities->fxBurst(x, y, 200.0f, VfxTheme::Blood);
  Logger::info("Match: boss spawned");
}

// ---------------------------------------------------------------------------
// Zone, turrets, scrap, deaths
// ---------------------------------------------------------------------------

void MatchDirector::updateZoneDamage(float dtMs) {
  zoneTickTimer -= dtMs;
  if (zoneTickTimer > 0.0f) return;
  zoneTickTimer = 500.0f;

  for (auto& [id, player] : players) {
    if (!isParticipant(player) || player.isDead()) continue;
    float shipDistance = dist(player.x, player.y, shipX, shipY);
    if (shipDistance <= Tuning::SHIP_SAFE_RADIUS &&
        currentPhase == MatchPhase::Round && player.health < player.maxHealth) {
      abilities->healPlayer(
          0, player, player.maxHealth * Tuning::SHIP_HEAL_PER_SECOND * 0.5f);
    }
    if (shipDistance <= zone) continue;
    // Outside the Blue Zone the planet itself is killing you.
    abilities->damagePlayer(
        player, player.maxHealth * Tuning::ZONE_DAMAGE_PER_SECOND * 0.5f, 0,
        DamageFlag::Dot);
  }
}

void MatchDirector::updateTurrets(float dtMs) {
  if (scrapBanked < Tuning::SHIP_TURRET_SCRAP) return;
  turretTimer -= dtMs;
  if (turretTimer > 0.0f) return;
  turretTimer = TURRET_INTERVAL_MS;

  Enemy* target = abilities->nearestEnemy(shipX, shipY, TURRET_RANGE);
  if (!target) return;
  abilities->fxBeam(shipX, shipY - 30.0f, target->x, target->y, 5.0f, 140.0f,
                    VfxTheme::Steel);
  abilities->damageEnemy(0, *target, TURRET_DAMAGE, DamageFlag::Proc);
}

void MatchDirector::bankScrap(Player& player) {
  player.carryingScrap = false;
  scrapBanked++;
  shipHp = std::min(Tuning::SHIP_MAX_HEALTH,
                    shipHp + Tuning::SHIP_REPAIR_PER_SCRAP);
  player.score += Tuning::SCORE_SCRAP;
  abilities->fxAnnounce(scrapBanked == Tuning::SHIP_TURRET_SCRAP
                            ? Announcement::ShipTurretsOnline
                            : Announcement::ScrapDelivered);
  abilities->fxRing(shipX, shipY, 140.0f, VfxTheme::Heal);
}

void MatchDirector::updateScrapDelivery() {
  for (auto& [id, player] : players) {
    if (!player.carryingScrap || player.isDead()) continue;
    if (dist(player.x, player.y, shipX, shipY) <= Tuning::SHIP_RADIUS + 70.0f) {
      bankScrap(player);
    }
  }
}

void MatchDirector::updateDeathsAndRevives(float dtMs) {
  for (auto& [id, player] : players) {
    if (!isParticipant(player)) continue;

    bool dead = player.isDead();
    if (dead && !wasDead[id]) {
      // Just went down.
      wasDead[id] = true;
      player.deaths++;
      player.score += Tuning::SCORE_DEATH;
      player.reviveProgress = 0.0f;
      player.deathTime = 1.0f;
      abilities->effects().clearEntity(id, false);
      if (player.carryingScrap) {
        player.carryingScrap = false;
        if (hooks.spawnPickup) {
          hooks.spawnPickup(Arena::PickupId::Scrap, player.x, player.y);
        }
      }
      abilities->fxAnnounce(Announcement::PlayerDown);
    }
    if (!dead) {
      wasDead[id] = false;
      continue;
    }

    // Revive: a living ally standing over the body brings them back.
    Player* reviver = nullptr;
    for (auto& [otherId, other] : players) {
      if (otherId == id || !isParticipant(other) || other.isDead()) continue;
      if (dist(player.x, player.y, other.x, other.y) <= Tuning::REVIVE_RADIUS) {
        reviver = &other;
        break;
      }
    }
    if (reviver) {
      player.reviveProgress += dtMs / (Tuning::REVIVE_SECONDS * 1000.0f);
      if (player.reviveProgress >= 1.0f) {
        player.health = player.maxHealth * Tuning::REVIVE_HEALTH_FRACTION;
        player.reviveProgress = 0.0f;
        player.deathTime = 0.0f;
        player.intangibleMs = 1500.0f;  // a moment to get clear
        wasDead[id] = false;
        reviver->score += Tuning::SCORE_REVIVE;
        abilities->fxAnnounce(Announcement::PlayerRevived);
        abilities->fxRing(player.x, player.y, 90.0f, VfxTheme::Heal);
      }
    } else {
      player.reviveProgress =
          std::max(0.0f, player.reviveProgress - dtMs / 2000.0f);
    }
  }
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void MatchDirector::onPlayerJoined(Player& player) {
  player.x = shipX + randomRange(-120.0f, 120.0f);
  player.y = shipY + 120.0f + randomRange(-30.0f, 30.0f);
  player.ready = false;
  wasDead[player.id] = false;
}

void MatchDirector::onPlayerLeft(uint32_t playerId) {
  wasDead.erase(playerId);
  roundStartScore.erase(playerId);
}

void MatchDirector::onCharacterSelected(Player& player, uint32_t characterId) {
  if (!Arena::isPlayable(characterId)) {
    Logger::info("Match: character " + std::to_string(characterId) +
                 " has no kit yet, ignoring selection");
    return;
  }
  // Characters are locked once a player is in the fight.
  bool swappable = currentPhase == MatchPhase::Lobby || player.characterId == 0;
  if (!swappable) return;

  bool joiningLate = player.characterId == 0 && currentPhase != MatchPhase::Lobby;
  player.characterId = characterId;
  abilities->applyKitStats(player);
  if (joiningLate) {
    // Late joiners drop in at the ship.
    player.x = shipX + randomRange(-100.0f, 100.0f);
    player.y = shipY + 130.0f;
    player.forcedMs = 100.0f;
    roundStartScore[player.id] = player.score;
  }
  wasDead[player.id] = false;
}

void MatchDirector::onReady(Player& player, bool ready) {
  player.ready = ready;
}

void MatchDirector::onSlotInfusion(Player& player, Genus genus, int slot) {
  // The suit can only be opened up back at the ship.
  if (currentPhase == MatchPhase::Round ||
      currentPhase == MatchPhase::BossFight) {
    if (dist(player.x, player.y, shipX, shipY) > Tuning::SHIP_SAFE_RADIUS) {
      return;
    }
  }
  if (Infusions::slotInfusion(player, genus, slot)) {
    Logger::info("Match: player " + std::to_string(player.id) + " slotted " +
                 Arena::genusName(genus) + " into slot " +
                 std::to_string(slot));
  }
}

void MatchDirector::onEnemyKilled(const Enemy& enemy, uint32_t killerId) {
  const Arena::EnemyKindDef& def = Arena::enemyDef(enemy.type);
  auto it = players.find(killerId);
  if (it != players.end()) {
    it->second.kills++;
    it->second.score += enemy.elite ? def.score * 2 : def.score;
  }
  killsThisRound++;
  if (inCombat()) dropLoot(enemy);
}

void MatchDirector::dropLoot(const Enemy& enemy) {
  if (!hooks.spawnPickup) return;
  const Arena::EnemyKindDef& def = Arena::enemyDef(enemy.type);
  if (def.tier == EnemyTier::Boss) return;

  static const float INFUSION_CHANCE[3] = {0.07f, 0.24f, 0.75f};
  float chance = INFUSION_CHANCE[static_cast<int>(def.tier)];
  if (enemy.elite) chance = 1.0f;
  // Pity: every round hands out its first infusion early so there is always
  // something to slot at the first intermission.
  int participants = std::max(1, participantCount());
  if (infusionDropsThisRound < participants && killsThisRound >= 6 * (infusionDropsThisRound + 1)) {
    chance = 1.0f;
  }

  if (random01() < chance) {
    infusionDropsThisRound++;
    hooks.spawnPickup(
        Arena::PickupId::InfusionBase + static_cast<uint32_t>(def.genus),
        enemy.x, enemy.y);
  }
  if (def.tier == EnemyTier::Brute && random01() < 0.5f) {
    hooks.spawnPickup(Arena::PickupId::Scrap, enemy.x + 24.0f, enemy.y);
  } else if (random01() < 0.05f) {
    hooks.spawnPickup(Arena::PickupId::Medkit, enemy.x - 20.0f, enemy.y);
  }
}

bool MatchDirector::onPickup(Player& player, uint32_t itemId) {
  if (!isParticipant(player) || player.isDead()) return false;

  if (Arena::PickupId::isInfusion(itemId)) {
    int genus = static_cast<int>(Arena::PickupId::infusionGenus(itemId));
    if (player.infusionInventory[genus - 1] >= 99) return false;
    player.infusionInventory[genus - 1]++;
    player.score += 15;
    return true;
  }
  if (itemId == Arena::PickupId::Scrap) {
    if (player.carryingScrap) return false;  // one piece at a time
    player.carryingScrap = true;
    return true;
  }
  if (itemId == Arena::PickupId::Medkit) {
    if (player.health >= player.maxHealth) return false;
    abilities->healPlayer(0, player, player.maxHealth * 0.35f);
    return true;
  }
  return false;
}

void MatchDirector::onObjectiveCompleted(Player& player, bool deliveredScrap) {
  player.score += Tuning::SCORE_OBJECTIVE;
  abilities->fxAnnounce(Announcement::ObjectiveComplete);
  if (deliveredScrap) {
    player.carryingScrap = true;
    bankScrap(player);
  } else if (hooks.spawnPickup) {
    // Objectives always pay out an infusion from the local genus.
    Genus genus = Arena::genusForDirection(player.x - shipX, player.y - shipY);
    hooks.spawnPickup(
        Arena::PickupId::InfusionBase + static_cast<uint32_t>(genus),
        player.x + 30.0f, player.y);
  }
}

void MatchDirector::damageShip(float amount) {
  if (currentPhase != MatchPhase::Round) return;
  shipHp = std::max(0.0f, shipHp - amount * Tuning::SHIP_DAMAGE_SCALE);
  if (shipAlertCooldown <= 0.0f) {
    shipAlertCooldown = 12000.0f;
    abilities->fxAnnounce(Announcement::ShipUnderAttack);
  }
}

void MatchDirector::onDebugCommand(Player& player, DebugCommandKind command,
                                   uint8_t arg) {
  switch (command) {
    case DebugCommandKind::SkipPhase:
      if (currentPhase == MatchPhase::Lobby) {
        if (participantCount() > 0) startRound(1);
      } else if (currentPhase == MatchPhase::Round) {
        phaseTimeLeft = 0.0f;
        bringPlayersHome(false);  // skipping should not strand anyone
      } else if (currentPhase == MatchPhase::Intermission) {
        phaseTimeLeft = 0.0f;
      } else if (currentPhase == MatchPhase::BossFight) {
        finishMatch(true);
      } else {
        resetMatch();
      }
      break;
    case DebugCommandKind::GiveInfusions:
      for (uint8_t& count : player.infusionInventory) count += 2;
      break;
    case DebugCommandKind::SpawnWave: {
      // arg 1-4: a swarm of that genus's smallest alien (sprite testing).
      float angle = randomRange(0.0f, 2.0f * PI);
      Genus genus = arg >= 1 && arg <= Arena::GENUS_COUNT
                        ? static_cast<Genus>(arg)
                        : Genus::None;
      spawnPack(player.x + std::cos(angle) * 300.0f,
                player.y + std::sin(angle) * 300.0f, 6, false, genus,
                genus != Genus::None);
      break;
    }
    case DebugCommandKind::ToggleGodMode:
      player.godMode = !player.godMode;
      break;
    case DebugCommandKind::SpawnBoss:
      if (inCombat() && !bossSpawned) spawnBoss();
      break;
    case DebugCommandKind::KillAll: {
      std::vector<uint32_t> ids;
      for (auto& [id, enemy] : enemies->getEnemies()) {
        if (enemy.state != EnemyState::Dead && enemy.type != EnemyKind::Boss) {
          ids.push_back(id);
        }
      }
      for (uint32_t id : ids) enemies->damageEnemy(id, 1e9f, player.id);
      break;
    }
  }
}

MatchStatePacket MatchDirector::buildState() const {
  MatchStatePacket packet;
  packet.phase = static_cast<uint8_t>(currentPhase);
  packet.round = static_cast<uint8_t>(currentRound);
  packet.roundCount = static_cast<uint8_t>(Tuning::ROUND_COUNT);
  packet.timeRemaining = std::max(0.0f, phaseTimeLeft);
  packet.shipHealth = shipHp;
  packet.shipMaxHealth = Tuning::SHIP_MAX_HEALTH;
  packet.shipX = shipX;
  packet.shipY = shipY;
  packet.zoneRadius = zone;
  packet.scrapBanked = static_cast<uint16_t>(scrapBanked);
  packet.turretsOnline = scrapBanked >= Tuning::SHIP_TURRET_SCRAP ? 1 : 0;
  packet.bossId = bossId;

  for (const auto& [id, player] : players) {
    MatchPlayerState p;
    p.playerId = id;
    p.characterId = static_cast<uint8_t>(player.characterId);
    p.score = player.score;
    p.kills = player.kills;
    p.deaths = player.deaths;
    p.ready = player.ready ? 1 : 0;
    p.coreGenus = static_cast<uint8_t>(player.coreGenus);
    for (int i = 0; i < Arena::TREE_COUNT; i++) {
      p.treeGenus[i] = static_cast<uint8_t>(player.treeGenus[i]);
      p.treeLevel[i] = player.treeLevel[i];
    }
    for (int i = 0; i < Arena::GENUS_COUNT; i++) {
      p.inventory[i] = player.infusionInventory[i];
    }
    p.carryingScrap = player.carryingScrap ? 1 : 0;
    p.reviveProgress = player.reviveProgress;
    packet.players.push_back(p);
  }
  return packet;
}
