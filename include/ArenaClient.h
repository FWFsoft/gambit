#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ArenaData.h"
#include "EventBus.h"
#include "NetworkProtocol.h"

// Client-side picture of the match, built purely from server packets.
// No rendering and no GL here: ArenaView draws what this class tracks, and
// a headless client can read it just as well.
//
// The server resolves all combat and reports what happened as GameEvents;
// this class turns those into short-lived visual objects (slashes, bursts,
// projectiles, floating numbers) and keeps smoothed positions for players
// and enemies.
class ArenaClient {
 public:
  ArenaClient();

  struct PlayerView {
    PlayerState state;      // latest from the server
    float x = 0, y = 0;     // smoothed render position
    bool hasPosition = false;
    float castMs = 0;       // time left in the cast animation
    float castTotalMs = 0;
    float castDirX = 1, castDirY = 0;
    int castSlot = 0;
    uint32_t castCount = 0;  // increments on every cast (alternating anims)
    float deadMs = 0;        // time spent down (death animation)
    float hitMs = 1e9f;      // time since the last hit taken (hit animation)
    float hitFlashMs = 0;
    float animTime = 0;     // seconds, drives bob / walk cycle
    float shieldBubbleMs = 0;
    uint8_t shieldTheme = 0;
    float lastSeenMs = 0;
  };

  struct EnemyView {
    NetworkEnemyState state;
    float x = 0, y = 0;
    bool hasPosition = false;
    bool faceLeft = false;
    float attackMs = 0;  // windup animation time left
    float attackTotalMs = 0;
    float strikeMs = 0;  // short lunge after the windup ends
    float aimX = 0, aimY = 0;
    float hitFlashMs = 0;
    float hitMs = 1e9f;  // time since the last hit taken
    bool dying = false;
    float deathMs = 0;   // time since death started
    float animTime = 0;
    float lastSeenMs = 0;
  };

  // One-shot effects (slash, cone, burst, ring, beam, trail, tether, puff,
  // telegraphs, lobs). Meaning of the fields follows GameEventPacket.
  struct Fx {
    GameEventKind kind;
    float x, y, x2, y2;
    float value, value2;
    uint8_t param;
    VfxTheme theme;
    float ageMs = 0;
    float lifeMs = 300;
  };

  struct Projectile {
    uint32_t id;
    float x, y, vx, vy;
    float radius;
    float ageMs = 0, lifeMs = 1000;
    VfxTheme theme;
    uint8_t sprite;
    bool fromEnemy;
  };

  struct Zone {
    uint32_t id;
    float x, y, radius;
    float ageMs = 0, lifeMs = 1000;
    VfxTheme theme;
    uint8_t variant;
    bool fromEnemy;
  };

  struct FloatText {
    float x, y;
    std::string text;
    uint32_t color;  // 0xAABBGGRR (ImGui order)
    float size = 16.0f;
    float ageMs = 0, lifeMs = 900;
    float driftX = 0;
  };

  struct Banner {
    std::string title;
    std::string subtitle;
    uint32_t color = 0xFFFFFFFF;
    float ageMs = 0, lifeMs = 3200;
  };

  bool hasMatch() const { return matchReceived; }
  const MatchStatePacket& match() const { return matchState; }
  Arena::MatchPhase phase() const {
    return static_cast<Arena::MatchPhase>(matchState.phase);
  }
  const MatchPlayerState* matchPlayer(uint32_t playerId) const;

  const std::unordered_map<uint32_t, PlayerView>& players() const {
    return playerViews;
  }
  const std::unordered_map<uint32_t, EnemyView>& enemies() const {
    return enemyViews;
  }
  const std::vector<Fx>& effects() const { return fx; }
  const std::vector<Projectile>& projectiles() const { return shots; }
  const std::vector<Zone>& zones() const { return groundZones; }
  const std::vector<FloatText>& floatTexts() const { return texts; }
  const std::vector<Banner>& banners() const { return bannerQueue; }
  float nowMs() const { return timeMs; }

  // Stats for the HUD / debugging
  uint32_t eventsReceived() const { return eventCount; }

 private:
  bool matchReceived = false;
  MatchStatePacket matchState;
  std::unordered_map<uint32_t, PlayerView> playerViews;
  std::unordered_map<uint32_t, EnemyView> enemyViews;
  std::vector<Fx> fx;
  std::vector<Projectile> shots;
  std::vector<Zone> groundZones;
  std::vector<FloatText> texts;
  std::vector<Banner> bannerQueue;
  float timeMs = 0.0f;
  uint32_t eventCount = 0;
  uint8_t lastPhase = 255;

  void onPacket(const NetworkPacketReceivedEvent& e);
  void onUpdate(const UpdateEvent& e);
  void onStateUpdate(const StateUpdatePacket& packet);
  void onEnemyUpdate(const EnemyStateUpdatePacket& packet);
  void onGameEvent(const GameEventPacket& packet);
  void pushBanner(const std::string& title, const std::string& subtitle,
                  uint32_t color, float lifeMs = 3200.0f);
};
