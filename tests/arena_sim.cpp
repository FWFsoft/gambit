// Headless match simulator.
//
// Runs the real server (ServerGameState + AbilitySystem + EnemySystem +
// MatchDirector) with bot players driving the same ClientInput packets a
// client would send, at many times real time. No window, no sockets.
//
// It exists to answer "does a whole match hang together?" quickly: do rounds
// advance, does anything crash or stall, roughly how do the characters
// compare. It is not a balance oracle: the bots are simple.
//
// Usage:
//   ArenaSim [--bots Suds,Gravon,Isaac,Zephyr] [--map assets/maps/arena.tmx]
//            [--god] [--minutes 20] [--seed N] [--quiet] [--expect victory]
//            [--matches N]  (play N matches in a row on one server; maps with
//                            several crash sites land somewhere new each time)
//
// Exit code is non-zero if the match does not finish in time, or if
// --expect is given and the result differs.

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "AbilitySystem.h"
#include "ArenaData.h"
#include "CollisionSystem.h"
#include "EnemySystem.h"
#include "EventBus.h"
#include "InMemoryChannel.h"
#include "ItemRegistry.h"
#include "Logger.h"
#include "MatchDirector.h"
#include "NetworkProtocol.h"
#include "NetworkServer.h"
#include "ServerGameState.h"
#include "TiledMap.h"
#include "WorldConfig.h"
#include "config/TimingConfig.h"
#include "transport/InMemoryServerTransport.h"

namespace {

struct Bot {
  uint32_t id = 0;
  uint32_t characterId = 0;
  std::string name;
  uint32_t inputSequence = 1;
  bool sentCharacter = false;
  float chargeHeldMs = 0.0f;
  float wanderAngle = 0.0f;
  float wanderTimerMs = 0.0f;
  float slotTimerMs = 0.0f;
  bool gaveSuit = false;  // --suits: handed out once the match is on
  // stats
  int abilityCasts[Arena::SLOT_COUNT] = {0, 0, 0, 0, 0};
};

uint32_t characterIdByName(const std::string& name) {
  static const std::map<std::string, uint32_t> ids = {
      {"Suds", Arena::CharacterId::Suds},
      {"Gravon", Arena::CharacterId::Gravon},
      {"Isaac", Arena::CharacterId::Isaac},
      {"Zephyr", Arena::CharacterId::Zephyr},
      {"Mina", Arena::CharacterId::Mina},
      {"Volgore", Arena::CharacterId::Volgore},
      {"Wade", Arena::CharacterId::Wade},
      {"Nolan", Arena::CharacterId::Nolan},
      {"Nyx", Arena::CharacterId::Nyx},
      {"Presidente", Arena::CharacterId::Presidente},
  };
  auto it = ids.find(name);
  return it == ids.end() ? 0 : it->second;
}

void sendPacket(uint32_t clientId, const std::vector<uint8_t>& data) {
  EventBus::instance().publish(
      NetworkPacketReceivedEvent{clientId, data.data(), data.size()});
}

float dist(float x0, float y0, float x1, float y1) {
  return std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
}

// How close a character wants to be to what it is hitting.
float preferredRange(uint32_t characterId) {
  switch (characterId) {
    case Arena::CharacterId::Gravon:
      return 85.0f;
    case Arena::CharacterId::Wade:
      return 60.0f;
    case Arena::CharacterId::Nolan:
      return 70.0f;
    case Arena::CharacterId::Presidente:
      return 70.0f;
    case Arena::CharacterId::Volgore:
      return 75.0f;
    case Arena::CharacterId::Isaac:
      return 170.0f;
    case Arena::CharacterId::Suds:
      return 260.0f;
    case Arena::CharacterId::Nyx:
      return 240.0f;
    case Arena::CharacterId::Zephyr:
      return 250.0f;
    case Arena::CharacterId::Mina:
      return 330.0f;
    default:
      return 150.0f;
  }
}

class Simulation {
 public:
  Simulation(const std::string& mapPath, unsigned seed) : rng(seed) {
    channel = createInMemoryChannel();
    server = std::make_unique<NetworkServer>(
        std::make_unique<InMemoryServerTransport>(channel));
    server->initialize("sim", 0);

    bool loaded = map.load(mapPath);
    if (!loaded) {
      std::fprintf(stderr, "failed to load map %s\n", mapPath.c_str());
      std::exit(2);
    }
    collision = std::make_unique<CollisionSystem>(map.getCollisionShapes(),
                                                    &map.terrain());
    WorldConfig world(map.getWorldWidth(), map.getWorldHeight(),
                      collision.get(), &map);
    state = std::make_unique<ServerGameState>(server.get(), world);
  }

  void addBot(const std::string& name) {
    Bot bot;
    bot.id = static_cast<uint32_t>(bots.size() + 1);
    bot.name = name;
    bot.characterId = characterIdByName(name);
    bot.wanderAngle = std::uniform_real_distribution<float>(0, 6.28f)(rng);
    bots.push_back(bot);
    EventBus::instance().publish(ClientConnectedEvent{bot.id});
  }

  void setGodMode() { godMode = true; }
  // --suits: every bot starts with one of every infusion at every grade,
  // so the graded effects run in real fights.
  void setFullSuits() { fullSuits = true; }

  void step() {
    for (Bot& bot : bots) driveBot(bot);
    EventBus::instance().publish(
        UpdateEvent{Config::Timing::TARGET_DELTA_MS, tick++});
    drainOutbound();
  }

  ServerGameState& game() { return *state; }
  std::vector<Bot>& allBots() { return bots; }
  uint64_t ticks() const { return tick; }
  uint64_t bytesSent() const { return outboundBytes; }
  uint64_t packetsSent() const { return outboundPackets; }
  const std::map<int, uint64_t>& bytesByType() const { return typeBytes; }

 private:
  std::shared_ptr<InMemoryChannel> channel;
  std::unique_ptr<NetworkServer> server;
  TiledMap map;
  std::unique_ptr<CollisionSystem> collision;
  std::unique_ptr<ServerGameState> state;
  std::vector<Bot> bots;
  std::mt19937 rng;
  uint64_t tick = 0;
  bool godMode = false;
  bool fullSuits = false;
  uint64_t outboundBytes = 0;
  uint64_t outboundPackets = 0;
  std::map<int, uint64_t> typeBytes;

  void drainOutbound() {
    // Nothing reads the "client" side; tally it for bandwidth numbers.
    while (channel->hasServerToClient()) {
      auto message = channel->popServerToClient();
      if (message.empty()) break;
      outboundBytes += message.size();
      outboundPackets++;
      typeBytes[message[0]] += message.size();
    }
  }

  // Isaac's Lifeline needs an anchor: a rock in reach on the way to the
  // fight, else a hurt ally in reach.
  void lifelineAim(const Player& me, const Enemy& target,
                   ClientInputPacket& input) {
    const float reach = Arena::Tuning::LIFELINE_RANGE - 10.0f;
    float best = 1e9f;
    for (const CollisionShape& shape : collision->getShapes()) {
      if (!CollisionSystem::isTerrain(shape)) continue;
      float fx, fy;
      CollisionSystem::closestPoint(shape.aabb, me.x, me.y, fx, fy);
      if (dist(me.x, me.y, fx, fy) > reach) continue;
      float score = dist(fx, fy, target.x, target.y);
      if (score < best) {
        best = score;
        input.aimX = fx;
        input.aimY = fy;
      }
    }
    if (best < 1e9f) return;
    for (const auto& [id, ally] : state->getPlayers()) {
      if (id == me.id || !ally.isAlive()) continue;
      if (dist(me.x, me.y, ally.x, ally.y) > reach) continue;
      input.aimX = ally.x;
      input.aimY = ally.y;
      return;
    }
  }

  void driveBot(Bot& bot) {
    auto& players = state->getPlayers();
    auto it = players.find(bot.id);
    if (it == players.end()) return;
    Player& me = it->second;
    MatchDirector& director = *state->getMatchDirector();
    Arena::MatchPhase phase = director.phase();
    const float dtMs = Config::Timing::TARGET_DELTA_MS;

    if (!bot.sentCharacter) {
      CharacterSelectedPacket pick;
      pick.characterId = bot.characterId;
      sendPacket(bot.id, serialize(pick));
      bot.sentCharacter = true;
      if (godMode) {
        DebugCommandPacket god;
        god.command = static_cast<uint8_t>(DebugCommandKind::ToggleGodMode);
        sendPacket(bot.id, serialize(god));
      }
    }

    // A new match empties suits, so hand them out once it is under way.
    if (fullSuits && !bot.gaveSuit && phase == Arena::MatchPhase::Round) {
      DebugCommandPacket give;
      give.command = static_cast<uint8_t>(DebugCommandKind::GiveInfusions);
      sendPacket(bot.id, serialize(give));
      bot.gaveSuit = true;
    }

    // Between fights: slot whatever was picked up, then ready up.
    if (phase != Arena::MatchPhase::Round &&
        phase != Arena::MatchPhase::BossFight) {
      bot.slotTimerMs -= dtMs;
      if (bot.slotTimerMs <= 0.0f) {
        bot.slotTimerMs = 300.0f;
        bool slotted = false;
        // Core first, then a random node in any tree that takes the item.
        // When nothing fits, combine a pair to reach deeper tiers.
        const Arena::SuitSlots suit = me.suitSlots();
        const int maxTier = Arena::suitTierCap(director.round());
        // Each bot favours its own genus first, so all four get played.
        const int favourite = static_cast<int>(bot.id % Arena::GENUS_COUNT);
        for (int k = 0; k < Arena::GENUS_COUNT && !slotted; k++) {
          int g = (k + favourite) % Arena::GENUS_COUNT;
          Arena::Genus genus = static_cast<Arena::Genus>(g + 1);
          for (int p = 0; p < Arena::PART_COUNT && !slotted; p++) {
            for (int grade = 1; grade <= Arena::GRADE_COUNT && !slotted;
                 grade++) {
              if (me.infusionInventory[g][p][grade - 1] == 0) continue;
              std::vector<int> fits;
              if (me.coreGenus == Arena::Genus::None) {
                fits.push_back(Arena::CORE_NODE);
              } else {
                for (int n = 0; n < Arena::suitNodeCount(); n++) {
                  if (Arena::slotBlock(suit, n, genus, grade, maxTier) ==
                      Arena::SlotBlock::None) {
                    fits.push_back(n);
                  }
                }
              }
              if (fits.empty()) continue;
              int pick = std::uniform_int_distribution<int>(
                  0, static_cast<int>(fits.size()) - 1)(rng);
              InfusionSlotPacket slot;
              slot.genus = static_cast<uint8_t>(g + 1);
              slot.part = static_cast<uint8_t>(p);
              slot.grade = static_cast<uint8_t>(grade);
              slot.node = static_cast<uint8_t>(fits[pick]);
              sendPacket(bot.id, serialize(slot));
              slotted = true;
            }
          }
        }
        for (int g = 0; g < Arena::GENUS_COUNT && !slotted; g++) {
          Arena::Genus genus = static_cast<Arena::Genus>(g + 1);
          for (int grade = 1; grade < Arena::GRADE_COUNT && !slotted; grade++) {
            if (!Arena::canCombine(me.ascendedGenus, genus, grade)) continue;
            int parts[2], found = 0;
            for (int p = 0; p < Arena::PART_COUNT && found < 2; p++) {
              for (int k = 0;
                   k < me.infusionInventory[g][p][grade - 1] && found < 2;
                   k++) {
                parts[found++] = p;
              }
            }
            if (found < 2) continue;
            InfusionCombinePacket combine;
            combine.genus = static_cast<uint8_t>(g + 1);
            combine.grade = static_cast<uint8_t>(grade);
            combine.targetPart = static_cast<uint8_t>(parts[0]);
            combine.otherPart = static_cast<uint8_t>(parts[1]);
            sendPacket(bot.id, serialize(combine));
            slotted = true;
          }
        }
        if (!slotted && !me.ready) {
          PlayerReadyPacket ready;
          sendPacket(bot.id, serialize(ready));
        }
      }
    }

    ClientInputPacket input;
    input.inputSequence = bot.inputSequence++;
    input.moveLeft = input.moveRight = input.moveUp = input.moveDown = false;
    input.aimX = me.x + 50.0f;
    input.aimY = me.y;

    bool fighting = (phase == Arena::MatchPhase::Round ||
                     phase == Arena::MatchPhase::BossFight) &&
                    me.isAlive() && me.characterId != 0;
    if (fighting) {
      MatchStatePacket match = director.buildState();
      float shipX = match.shipX, shipY = match.shipY;
      float shipDist = dist(me.x, me.y, shipX, shipY);

      // Nearest living enemy
      const Enemy* target = nullptr;
      float targetDist = 1e9f;
      for (const auto& [id, enemy] : state->getEnemySystem()->getEnemies()) {
        if (enemy.state == EnemyState::Dead) continue;
        float d = dist(me.x, me.y, enemy.x, enemy.y) - enemy.radius;
        if (d < targetDist) {
          targetDist = d;
          target = &enemy;
        }
      }

      // Somebody has to answer when raiders reach the ship: the bot with
      // the lowest id that is still standing peels off to defend it.
      const Enemy* raider = nullptr;
      for (const auto& [id, enemy] : state->getEnemySystem()->getEnemies()) {
        if (enemy.state == EnemyState::Dead || !enemy.targetShip) continue;
        if (dist(enemy.x, enemy.y, shipX, shipY) < 260.0f) raider = &enemy;
      }
      if (raider) {
        uint32_t defender = 0;
        for (const auto& [id, other] : players) {
          if (other.characterId != 0 && other.isAlive() &&
              (defender == 0 || id < defender)) {
            defender = id;
          }
        }
        if (defender == me.id) {
          target = raider;
          targetDist = dist(me.x, me.y, raider->x, raider->y) - raider->radius;
        }
      }

      float range = preferredRange(me.characterId);
      float goalX = me.x, goalY = me.y;
      bool hasGoal = false;

      // A downed ally nearby is worth a detour.
      const Player* downed = nullptr;
      for (const auto& [id, other] : players) {
        if (id != me.id && other.characterId != 0 && other.isDead() &&
            dist(me.x, me.y, other.x, other.y) < 500.0f) {
          downed = &other;
        }
      }

      bool mustReturn = phase == Arena::MatchPhase::Round &&
                        (match.timeRemaining < 12.0f + shipDist / 200.0f ||
                         shipDist > match.zoneRadius - 110.0f ||
                         me.scrapCarried >= Arena::Tuning::SCRAP_HEAVY_LOAD);
      if (phase == Arena::MatchPhase::BossFight &&
          shipDist > match.zoneRadius - 110.0f) {
        mustReturn = true;
      }

      // Hurt: fall back to the ship to patch up.
      bool retreating = phase == Arena::MatchPhase::Round &&
                        me.health < me.maxHealth * 0.35f && shipDist > 200.0f;
      if (retreating) mustReturn = true;

      if (mustReturn) {
        goalX = shipX;
        goalY = shipY + 60.0f;
        hasGoal = shipDist > 120.0f;
      } else if (downed) {
        goalX = downed->x;
        goalY = downed->y;
        hasGoal = dist(me.x, me.y, goalX, goalY) > 30.0f;
      } else if (target && (targetDist < 700.0f || target == raider)) {
        if (targetDist > range) {
          goalX = target->x;
          goalY = target->y;
          hasGoal = true;
        } else if (targetDist < range * 0.5f && range > 120.0f) {
          // Ranged characters back off.
          goalX = me.x - (target->x - me.x);
          goalY = me.y - (target->y - me.y);
          hasGoal = true;
        }
      } else {
        // Wander the zone looking for a fight.
        bot.wanderTimerMs -= dtMs;
        if (bot.wanderTimerMs <= 0.0f) {
          bot.wanderTimerMs = 4000.0f;
          bot.wanderAngle =
              std::uniform_real_distribution<float>(0, 6.28f)(rng);
        }
        float roam = std::min(700.0f, match.zoneRadius * 0.55f);
        goalX = shipX + std::cos(bot.wanderAngle) * roam;
        goalY = shipY + std::sin(bot.wanderAngle) * roam;
        hasGoal = dist(me.x, me.y, goalX, goalY) > 40.0f;
      }

      if (hasGoal) {
        float dx = goalX - me.x;
        float dy = goalY - me.y;
        if (dx > 12.0f) input.moveRight = true;
        if (dx < -12.0f) input.moveLeft = true;
        if (dy > 12.0f) input.moveDown = true;
        if (dy < -12.0f) input.moveUp = true;
      }

      if (target) {
        input.aimX = target->x;
        input.aimY = target->y;
        bool inRange = targetDist <= range + 60.0f;
        bool close = targetDist <= range + 160.0f;

        if (inRange) input.buttons |= InputButtons::Primary;

        // Hold the charged ability for a beat, then release.
        const Arena::KitInfo* kit = Arena::kitInfo(me.characterId);
        bool secondaryReady = me.cooldownMs[1] <= 0.0f;
        bool holding = me.chargeSlot == 1;
        if (kit && (holding || (secondaryReady && close))) {
          float holdFor = kit->abilities[1].mode == Arena::AbilityMode::Channel
                              ? 1500.0f
                              : kit->abilities[1].maxChargeMs * 0.7f;
          if (me.characterId == Arena::CharacterId::Isaac) {
            lifelineAim(me, *target, input);
          }
          if (bot.chargeHeldMs < holdFor) {
            input.buttons |= InputButtons::Secondary;
            input.buttons &= ~InputButtons::Primary;
            bot.chargeHeldMs += dtMs;
          } else {
            bot.chargeHeldMs = 0.0f;  // release this tick
          }
        } else {
          bot.chargeHeldMs = 0.0f;
        }

        if (close && me.cooldownMs[2] <= 0.0f && tick % 7 == 0) {
          input.buttons |= InputButtons::Special;
        }
        if (me.cooldownMs[3] <= 0.0f && tick % 11 == 0 &&
            (close || me.health < me.maxHealth * 0.4f)) {
          input.buttons |= InputButtons::Mobility;
        }
        if (me.coreGenus != Arena::Genus::None && me.cooldownMs[4] <= 0.0f &&
            targetDist < 180.0f && tick % 13 == 0) {
          input.buttons |= InputButtons::Core;
        }
      }
    }

    for (int slot = 0; slot < Arena::SLOT_COUNT; slot++) {
      uint8_t bit = static_cast<uint8_t>(1u << slot);
      if ((input.buttons & bit) && !(me.buttons & bit)) bot.abilityCasts[slot]++;
    }
    sendPacket(bot.id, serialize(input));
  }
};

const char* phaseName(Arena::MatchPhase phase) {
  switch (phase) {
    case Arena::MatchPhase::Lobby:
      return "Lobby";
    case Arena::MatchPhase::Intro:
      return "Intro";
    case Arena::MatchPhase::Round:
      return "Round";
    case Arena::MatchPhase::Intermission:
      return "Intermission";
    case Arena::MatchPhase::BossFight:
      return "BossFight";
    case Arena::MatchPhase::Victory:
      return "Victory";
    case Arena::MatchPhase::Defeat:
      return "Defeat";
  }
  return "?";
}

}  // namespace

int main(int argc, char** argv) {
  std::string botList = "Suds,Gravon,Isaac,Zephyr";
  std::string mapPath = "assets/maps/arena.tmx";
  std::string expect;
  bool god = false;
  bool suits = false;
  bool quiet = false;
  int matches = 1;
  float minutes = 20.0f;
  unsigned seed = 7;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    auto next = [&]() -> std::string {
      return i + 1 < argc ? argv[++i] : "";
    };
    if (arg == "--bots") botList = next();
    else if (arg == "--map") mapPath = next();
    else if (arg == "--god") god = true;
    else if (arg == "--suits")
      suits = true;
    else if (arg == "--quiet") quiet = true;
    else if (arg == "--matches")
      matches = std::max(1, std::stoi(next()));
    else if (arg == "--minutes") minutes = std::stof(next());
    else if (arg == "--seed") seed = static_cast<unsigned>(std::stoul(next()));
    else if (arg == "--expect") expect = next();
  }

  Logger::init();
  spdlog::set_level(spdlog::level::warn);
  ItemRegistry::instance().loadFromCSV("assets/items.csv");

  Simulation sim(mapPath, seed);
  if (god) sim.setGodMode();
  if (suits) sim.setFullSuits();
  std::stringstream names(botList);
  std::string name;
  while (std::getline(names, name, ',')) {
    if (characterIdByName(name) == 0) {
      std::fprintf(stderr, "unknown or unplayable character: %s\n",
                   name.c_str());
      return 2;
    }
    sim.addBot(name);
  }

  MatchDirector& director = *sim.game().getMatchDirector();
  Arena::MatchPhase lastPhase = director.phase();
  int lastRound = 0;
  const uint64_t maxTicks = static_cast<uint64_t>(minutes * 60.0f * 60.0f);
  int peakEnemies = 0;

  // Maps with several crash sites: where this match landed, and that only
  // that site's objectives were live (all of them near its ship).
  auto reportLanding = [&](int match) {
    if (sim.game().getLandingSite() < 0) return;
    ObjectiveSystem* objectives = sim.game().getObjectiveSystem();
    float farthest = 0.0f;
    int count = 0;
    int shared = 0;
    for (const Objective& o : objectives->getObjectives()) {
      farthest = std::max(farthest, std::hypot(o.x - sim.game().getShipX(),
                                               o.y - sim.game().getShipY()));
      shared += __builtin_popcount(o.sites) > 1;
      count++;
    }
    std::printf(
        "match %d: landing site %d at (%.0f, %.0f): %d objectives in play "
        "(%d shared with another site), farthest %.0f px from the ship\n",
        match, sim.game().getLandingSite(), sim.game().getShipX(),
        sim.game().getShipY(), count, shared, farthest);
  };
  std::vector<int> landedOn;
  bool countedEnd = false;
  bool repeatedSite = false;
  // Issue #51: the ship never moves within a match, and the finale gathers
  // the crew where it actually landed, and the Matriarch arrives there.
  float matchShipX = 0.0f, matchShipY = 0.0f;
  bool haveMatchShip = false;
  bool checkedBossArrival = false;
  bool finaleMoved = false;

  while (sim.ticks() < maxTicks) {
    sim.step();
    Arena::MatchPhase phase = director.phase();
    peakEnemies =
        std::max(peakEnemies, sim.game().getEnemySystem()->livingCount());

    {
      const MatchStatePacket st = director.buildState();
      if (phase == Arena::MatchPhase::Lobby ||
          phase == Arena::MatchPhase::Victory ||
          phase == Arena::MatchPhase::Defeat) {
        haveMatchShip = false;  // the next match may land somewhere new
        checkedBossArrival = false;
      } else if (!haveMatchShip) {
        matchShipX = st.shipX;
        matchShipY = st.shipY;
        haveMatchShip = true;
      } else if (st.shipX != matchShipX || st.shipY != matchShipY) {
        std::printf("ERROR: the ship moved mid-match (%s): (%.0f, %.0f) -> "
                    "(%.0f, %.0f)\n",
                    phaseName(phase), matchShipX, matchShipY, st.shipX,
                    st.shipY);
        finaleMoved = true;
        matchShipX = st.shipX;
        matchShipY = st.shipY;
      }
      if (phase == Arena::MatchPhase::BossFight && lastPhase != phase) {
        for (const auto& [id, player] : sim.game().getPlayers()) {
          if (player.characterId == 0) continue;
          if (std::hypot(player.x - st.shipX, player.y - st.shipY) > 400.0f) {
            std::printf("ERROR: p%u starts the finale %.0f px from the ship\n",
                        id, std::hypot(player.x - st.shipX,
                                       player.y - st.shipY));
            finaleMoved = true;
          }
        }
      }
      if (phase == Arena::MatchPhase::BossFight && !checkedBossArrival &&
          st.bossId != 0) {
        checkedBossArrival = true;
        Enemy* boss = sim.game().getEnemySystem()->find(st.bossId);
        if (boss && std::hypot(boss->x - st.shipX, boss->y - st.shipY) >
                        Arena::Tuning::BOSS_ZONE_RADIUS) {
          std::printf("ERROR: the Matriarch arrives outside the ship's shield\n");
          finaleMoved = true;
        }
      }
    }

    if (phase != lastPhase || director.round() != lastRound) {
      if (!quiet) {
        std::printf("[%6.1fs] %-12s round %d  ship %.0f  enemies %d\n",
                    sim.ticks() / 60.0f, phaseName(phase), director.round(),
                    director.shipHealth(),
                    sim.game().getEnemySystem()->livingCount());
        for (const auto& [id, player] : sim.game().getPlayers()) {
          int nodes = 0, deepest = 0, unslotted = 0;
          for (int n = 0; n < Arena::suitNodeCount(); n++) {
            if (player.nodeGenus[n] == Arena::Genus::None) continue;
            nodes++;
            deepest = std::max<int>(deepest, Arena::suitNode(n).tier);
          }
          for (const auto& genusRow : player.infusionInventory) {
            for (const auto& partRow : genusRow) {
              for (uint8_t count : partRow) unslotted += count;
            }
          }
          std::printf(
              "           p%u %-8s hp %3.0f/%3.0f score %5d kills %3d deaths "
              "%d core %s +%d nodes (deepest tier %d, %d in pack)%s%s\n",
              id, sim.allBots()[id - 1].name.c_str(), player.health,
              player.maxHealth, player.score, player.kills, player.deaths,
              Arena::genusName(player.coreGenus), nodes, deepest, unslotted,
              player.ascendedGenus != Arena::Genus::None ? " ascended " : "",
              player.ascendedGenus != Arena::Genus::None
                  ? Arena::genusName(player.ascendedGenus)
                  : "");
        }
      }
      lastPhase = phase;
      lastRound = director.round();
    }
    if (phase == Arena::MatchPhase::Victory ||
        phase == Arena::MatchPhase::Defeat) {
      if (!countedEnd) {
        countedEnd = true;
        int site = sim.game().getLandingSite();
        if (!landedOn.empty() && site >= 0 && site == landedOn.back()) {
          repeatedSite = true;
        }
        landedOn.push_back(site);
        reportLanding(static_cast<int>(landedOn.size()));
        if (static_cast<int>(landedOn.size()) >= matches) break;
      }
    } else {
      countedEnd = false;
    }
  }

  Arena::MatchPhase result = director.phase();
  float seconds = sim.ticks() / 60.0f;
  std::printf("\nresult: %s after %.0fs of game time (round %d)\n",
              phaseName(result), seconds, director.round());
  std::printf("peak enemies alive: %d\n", peakEnemies);
  if (repeatedSite) {
    std::printf("ERROR: two matches in a row landed on the same site\n");
    return 1;
  }
  if (finaleMoved) return 1;
  if (static_cast<int>(landedOn.size()) < matches) {
    std::printf("ERROR: only %zu of %d matches finished\n", landedOn.size(),
                matches);
    return 1;
  }
  std::printf("server -> all clients: %.1f KB/s, %.0f packets/s\n",
              sim.bytesSent() / 1024.0 / seconds, sim.packetsSent() / seconds);
  for (const Bot& bot : sim.allBots()) {
    const Player& player = sim.game().getPlayers().at(bot.id);
    std::printf(
        "  %-8s score %5d  kills %3d  deaths %d  casts LMB %d RMB %d Q %d "
        "SHIFT %d E %d\n",
        bot.name.c_str(), player.score, player.kills, player.deaths,
        bot.abilityCasts[0], bot.abilityCasts[1], bot.abilityCasts[2],
        bot.abilityCasts[3], bot.abilityCasts[4]);
  }

  bool finished = result == Arena::MatchPhase::Victory ||
                  result == Arena::MatchPhase::Defeat;
  if (!finished) {
    std::fprintf(stderr, "match did not finish within %.0f minutes\n", minutes);
    return 1;
  }
  if (expect == "victory" && result != Arena::MatchPhase::Victory) return 1;
  if (expect == "defeat" && result != Arena::MatchPhase::Defeat) return 1;
  return 0;
}
