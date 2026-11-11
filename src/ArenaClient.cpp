#include "ArenaClient.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Effect.h"
#include "Logger.h"

namespace {

constexpr float STALE_ENEMY_MS = 400.0f;    // not in recent updates: despawned
constexpr float ENEMY_DEATH_ANIM_MS = 700.0f;
constexpr float STALE_PLAYER_MS = 1500.0f;
constexpr float SMOOTHING_RATE = 18.0f;     // per second, toward server pos

// 0xAABBGGRR
constexpr uint32_t COLOR_DAMAGE = 0xFFFFFFFF;
constexpr uint32_t COLOR_CRIT = 0xFF40D8FF;
constexpr uint32_t COLOR_DOT = 0xFF78D878;
constexpr uint32_t COLOR_PLAYER_HURT = 0xFF5050FF;
constexpr uint32_t COLOR_HEAL = 0xFF90FF90;
constexpr uint32_t COLOR_BUFF = 0xFFFFE080;
constexpr uint32_t COLOR_DEBUFF = 0xFFFF90D0;

float lifetimeFor(const GameEventPacket& p) {
  switch (p.kind) {
    case GameEventKind::Slash:
      return 260.0f;
    case GameEventKind::Cone:
      return 280.0f;
    case GameEventKind::Burst:
      return 320.0f;
    case GameEventKind::Ring:
      return 420.0f;
    case GameEventKind::Beam:
      return std::max(80.0f, p.value2);
    case GameEventKind::Lob:
      return std::max(60.0f, p.value2);
    case GameEventKind::TelegraphCircle:
    case GameEventKind::TelegraphCone:
      return std::max(60.0f, p.value2);
    case GameEventKind::DashTrail:
      return 300.0f;
    case GameEventKind::Tether:
      return std::max(80.0f, p.value2);
    case GameEventKind::SpawnPuff:
      return 450.0f;
    default:
      return 300.0f;
  }
}

}  // namespace

ArenaClient::ArenaClient() {
  EventBus::instance().subscribe<NetworkPacketReceivedEvent>(
      [this](const NetworkPacketReceivedEvent& e) { onPacket(e); });
  EventBus::instance().subscribe<UpdateEvent>(
      [this](const UpdateEvent& e) { onUpdate(e); });
}

const MatchPlayerState* ArenaClient::matchPlayer(uint32_t playerId) const {
  for (const auto& p : matchState.players) {
    if (p.playerId == playerId) return &p;
  }
  return nullptr;
}

void ArenaClient::onPacket(const NetworkPacketReceivedEvent& e) {
  if (e.size == 0) return;
  // In embedded mode the server shares this EventBus; packets travelling
  // client -> server carry a non-zero client id and are not for us.
  if (e.clientId != 0) return;

  PacketType type = static_cast<PacketType>(e.data[0]);
  switch (type) {
    case PacketType::StateUpdate:
      onStateUpdate(deserializeStateUpdate(e.data, e.size));
      break;
    case PacketType::EnemyStateUpdate:
      onEnemyUpdate(deserializeEnemyStateUpdate(e.data, e.size));
      break;
    case PacketType::EnemyDied: {
      EnemyDiedPacket packet = deserializeEnemyDied(e.data, e.size);
      auto it = enemyViews.find(packet.enemyId);
      if (it != enemyViews.end() && !it->second.dying) {
        it->second.dying = true;
        it->second.deathMs = 0.0f;
      }
      break;
    }
    case PacketType::PlayerLeft: {
      PlayerLeftPacket packet = deserializePlayerLeft(e.data, e.size);
      playerViews.erase(packet.playerId);
      break;
    }
    case PacketType::GameEvent:
      eventCount++;
      onGameEvent(deserializeGameEvent(e.data, e.size));
      break;
    case PacketType::MatchState: {
      matchState = deserializeMatchState(e.data, e.size);
      matchReceived = true;
      if (matchState.phase != lastPhase) {
        // Leaving a fight clears whatever was still on screen.
        Arena::MatchPhase phase = static_cast<Arena::MatchPhase>(matchState.phase);
        if (phase != Arena::MatchPhase::Round &&
            phase != Arena::MatchPhase::BossFight) {
          shots.clear();
          groundZones.clear();
          fx.clear();
        }
        lastPhase = matchState.phase;
      }
      break;
    }
    default:
      break;
  }
}

void ArenaClient::onStateUpdate(const StateUpdatePacket& packet) {
  for (const PlayerState& state : packet.players) {
    PlayerView& view = playerViews[state.playerId];
    bool teleported = false;
    if (view.hasPosition) {
      float dx = state.x - view.x;
      float dy = state.y - view.y;
      // Blinks and round teleports should not smear across the map.
      teleported = dx * dx + dy * dy > 220.0f * 220.0f;
    }
    view.state = state;
    if (!view.hasPosition || teleported) {
      view.x = state.x;
      view.y = state.y;
      view.hasPosition = true;
    }
    view.lastSeenMs = timeMs;
  }
}

void ArenaClient::onEnemyUpdate(const EnemyStateUpdatePacket& packet) {
  for (const NetworkEnemyState& state : packet.enemies) {
    EnemyView& view = enemyViews[state.id];
    bool wasDead = view.hasPosition &&
                   view.state.state == static_cast<uint8_t>(3);  // Dead
    view.state = state;
    if (!view.hasPosition) {
      view.x = state.x;
      view.y = state.y;
      view.hasPosition = true;
    }
    if (state.state == 3 && !wasDead && !view.dying) {
      view.dying = true;
      view.deathMs = 0.0f;
    }
    if (std::abs(state.vx) > 5.0f) view.faceLeft = state.vx < 0.0f;
    view.lastSeenMs = timeMs;
  }
}

void ArenaClient::pushBanner(const std::string& title,
                             const std::string& subtitle, uint32_t color,
                             float lifeMs) {
  // Only the newest couple of banners matter.
  if (bannerQueue.size() >= 2) bannerQueue.erase(bannerQueue.begin());
  Banner banner;
  banner.title = title;
  banner.subtitle = subtitle;
  banner.color = color;
  banner.lifeMs = lifeMs;
  bannerQueue.push_back(banner);
}

void ArenaClient::onGameEvent(const GameEventPacket& p) {
  VfxTheme theme = static_cast<VfxTheme>(p.param2);

  switch (p.kind) {
    case GameEventKind::AbilityCast: {
      auto it = playerViews.find(p.sourceId);
      if (it == playerViews.end()) break;
      PlayerView& view = it->second;
      view.castMs = view.castTotalMs = std::max(80.0f, p.value);
      view.castSlot = p.param;
      view.castCount++;
      view.castAlt = view.state.resource >= 200;
      float dx = p.x2 - p.x;
      float dy = p.y2 - p.y;
      float d = std::sqrt(dx * dx + dy * dy);
      if (d > 0.001f) {
        view.castDirX = dx / d;
        view.castDirY = dy / d;
      }
      break;
    }

    case GameEventKind::EnemyAttack: {
      auto it = enemyViews.find(p.sourceId);
      if (it == enemyViews.end()) break;
      EnemyView& view = it->second;
      view.attackMs = view.attackTotalMs = std::max(60.0f, p.value);
      view.aimX = p.x2;
      view.aimY = p.y2;
      if (std::abs(p.x2 - p.x) > 2.0f) view.faceLeft = p.x2 < p.x;
      break;
    }

    case GameEventKind::ProjectileSpawn: {
      Projectile shot;
      shot.id = p.extraId;
      shot.x = p.x;
      shot.y = p.y;
      shot.vx = p.x2;
      shot.vy = p.y2;
      shot.radius = p.value;
      shot.lifeMs = p.value2;
      shot.theme = theme;
      shot.sprite = p.param;
      shot.fromEnemy = p.sourceIsEnemy;
      shots.push_back(shot);
      break;
    }

    case GameEventKind::ProjectileEnd: {
      for (size_t i = 0; i < shots.size(); i++) {
        if (shots[i].id != p.extraId) continue;
        // A small pop where it ended.
        Fx pop;
        pop.kind = GameEventKind::Burst;
        pop.x = p.x;
        pop.y = p.y;
        pop.x2 = pop.y2 = 0;
        pop.value = shots[i].radius * 1.8f;
        pop.value2 = 0;
        pop.param = 1;  // lifted to projectile height
        pop.theme = shots[i].theme;
        pop.lifeMs = 180.0f;
        fx.push_back(pop);
        shots.erase(shots.begin() + i);
        break;
      }
      break;
    }

    case GameEventKind::ZoneSpawn: {
      Zone zone;
      zone.id = p.extraId;
      zone.x = p.x;
      zone.y = p.y;
      zone.radius = p.value;
      zone.lifeMs = p.value2;
      zone.theme = theme;
      zone.variant = p.param;
      zone.fromEnemy = p.sourceIsEnemy;
      groundZones.push_back(zone);
      break;
    }

    case GameEventKind::ZoneEnd: {
      groundZones.erase(
          std::remove_if(groundZones.begin(), groundZones.end(),
                         [&](const Zone& z) { return z.id == p.extraId; }),
          groundZones.end());
      break;
    }

    case GameEventKind::Damage: {
      bool onPlayer = (p.param & 2) != 0;
      bool crit = (p.param & 1) != 0;
      bool dot = (p.param & 4) != 0;
      if (p.sourceIsEnemy) {
        auto it = enemyViews.find(p.sourceId);
        if (it != enemyViews.end()) {
          it->second.hitFlashMs = 120.0f;
          it->second.hitMs = 0.0f;
        }
      } else {
        auto it = playerViews.find(p.sourceId);
        if (it != playerViews.end()) {
          it->second.hitFlashMs = 160.0f;
          if (!dot) it->second.hitMs = 0.0f;
        }
      }
      if (p.value < 0.5f) break;
      if (!dot) {
        // Impact flash on whatever got hit: every hit should feel like one.
        Fx spark;
        spark.kind = GameEventKind::HitSpark;
        spark.x = p.x;
        spark.y = p.y;
        spark.x2 = static_cast<float>((eventCount * 53) % 628) / 100.0f;
        spark.y2 = 0;
        spark.value = crit ? 30.0f : (p.value >= 30.0f ? 24.0f : 17.0f);
        spark.value2 = 0;
        spark.param = onPlayer ? 1 : 0;
        spark.theme = onPlayer ? VfxTheme::Blood
                               : (crit ? VfxTheme::Holy : VfxTheme::Neutral);
        spark.lifeMs = 170.0f;
        fx.push_back(spark);
      }
      FloatText text;
      // Scatter numbers a little so stacked hits stay readable.
      float jitter = static_cast<float>((eventCount * 37) % 41) - 20.0f;
      text.x = p.x + jitter;
      text.y = p.y - (onPlayer ? 70.0f : 46.0f);
      text.driftX = jitter * 0.6f;
      char buffer[24];
      std::snprintf(buffer, sizeof(buffer), crit ? "%.0f!" : "%.0f", p.value);
      text.text = buffer;
      if (onPlayer) {
        text.color = COLOR_PLAYER_HURT;
        text.size = 17.0f;
      } else if (crit) {
        text.color = COLOR_CRIT;
        text.size = 24.0f;
        text.lifeMs = 1100.0f;
      } else if (dot) {
        text.color = COLOR_DOT;
        text.size = 13.0f;
        text.lifeMs = 700.0f;
      } else {
        text.color = COLOR_DAMAGE;
        text.size = p.value >= 40.0f ? 20.0f : 16.0f;
      }
      texts.push_back(text);
      break;
    }

    case GameEventKind::Heal: {
      FloatText text;
      text.x = p.x;
      text.y = p.y - 78.0f;
      char buffer[24];
      std::snprintf(buffer, sizeof(buffer), "+%.0f", p.value);
      text.text = buffer;
      text.color = COLOR_HEAL;
      text.size = 16.0f;
      texts.push_back(text);
      break;
    }

    case GameEventKind::EffectText: {
      EffectType type = static_cast<EffectType>(p.param);
      FloatText text;
      text.x = p.x;
      text.y = p.y;
      text.text = EffectRegistry::getName(type);
      text.color = EffectRegistry::getCategory(type) == EffectCategory::Buff
                       ? COLOR_BUFF
                       : COLOR_DEBUFF;
      text.size = 15.0f;
      text.lifeMs = 1000.0f;
      texts.push_back(text);
      break;
    }

    case GameEventKind::ShieldBubble: {
      auto it = playerViews.find(p.sourceId);
      if (it != playerViews.end()) {
        it->second.shieldBubbleMs = std::min(p.value2, 60000.0f);
        it->second.shieldTheme = p.param2;
      }
      break;
    }

    case GameEventKind::Announcement: {
      switch (static_cast<Announcement>(p.param)) {
        case Announcement::RoundStart:
          pushBanner("ROUND START", "Scavenge, fight, and get back before dark",
                     0xFFFFE0A0);
          break;
        case Announcement::ZoneShrinking:
          pushBanner("THE BLUE ZONE IS CLOSING",
                     "Stay inside the shield or the planet takes you",
                     0xFFFFB060);
          break;
        case Announcement::ReturnToShip:
          pushBanner("RETURN TO THE SHIP", "Night is falling", 0xFF60C0FF);
          break;
        case Announcement::ShipUnderAttack:
          pushBanner("SHIP UNDER ATTACK", "", 0xFF5060FF, 2200.0f);
          break;
        case Announcement::ShipTurretsOnline:
          pushBanner("SHIP TURRETS ONLINE", "The ship can defend itself now",
                     0xFF90FF90);
          break;
        case Announcement::Intermission:
          pushBanner("SAFE FOR THE NIGHT", "Slot your infusions", 0xFFFFE0A0);
          break;
        case Announcement::BossIncoming:
          pushBanner("SOMETHING IS COMING", "The ship is lost. Kill it.",
                     0xFF6060FF, 4500.0f);
          break;
        case Announcement::Victory:
          pushBanner("VICTORY", "", 0xFF90FF90, 6000.0f);
          break;
        case Announcement::Defeat:
          pushBanner("DEFEAT", "", 0xFF5060FF, 6000.0f);
          break;
        case Announcement::PlayerDown:
          pushBanner("PLAYER DOWN", "Stand over them to revive", 0xFF5060FF,
                     2200.0f);
          break;
        case Announcement::PlayerRevived:
          pushBanner("REVIVED", "", 0xFF90FF90, 1600.0f);
          break;
        case Announcement::ScrapDelivered:
          pushBanner("SCRAP DELIVERED", "Ship repaired", 0xFFFFE0A0, 2000.0f);
          break;
        case Announcement::ObjectiveComplete:
          pushBanner("OBJECTIVE COMPLETE", "", 0xFF90FF90, 2000.0f);
          break;
      }
      break;
    }

    default: {
      // Everything else is a simple timed visual.
      Fx effect;
      effect.kind = p.kind;
      effect.x = p.x;
      effect.y = p.y;
      effect.x2 = p.x2;
      effect.y2 = p.y2;
      effect.value = p.value;
      effect.value2 = p.value2;
      effect.param = p.param;
      effect.theme = theme;
      effect.lifeMs = lifetimeFor(p);
      fx.push_back(effect);
      break;
    }
  }
}

void ArenaClient::onUpdate(const UpdateEvent& e) {
  float dtMs = e.deltaTime;
  float dt = dtMs / 1000.0f;
  timeMs += dtMs;
  float blend = std::min(1.0f, dt * SMOOTHING_RATE);

  for (auto it = playerViews.begin(); it != playerViews.end();) {
    PlayerView& view = it->second;
    if (timeMs - view.lastSeenMs > STALE_PLAYER_MS) {
      it = playerViews.erase(it);
      continue;
    }
    view.x += (view.state.x - view.x) * blend;
    view.y += (view.state.y - view.y) * blend;
    view.castMs = std::max(0.0f, view.castMs - dtMs);
    view.hitMs += dtMs;
    if (view.state.health <= 0.0f) {
      view.deadMs += dtMs;
    } else {
      view.deadMs = 0.0f;
    }
    view.hitFlashMs = std::max(0.0f, view.hitFlashMs - dtMs);
    view.shieldBubbleMs = std::max(0.0f, view.shieldBubbleMs - dtMs);
    if (view.state.shield <= 0.0f && view.shieldTheme == 0) {
      view.shieldBubbleMs = 0.0f;  // ordinary shields pop when spent
    }
    view.animTime += dt;
    ++it;
  }

  for (auto it = enemyViews.begin(); it != enemyViews.end();) {
    EnemyView& view = it->second;
    bool stale = timeMs - view.lastSeenMs > STALE_ENEMY_MS;
    if (view.dying) {
      view.deathMs += dtMs;
      if (view.deathMs > ENEMY_DEATH_ANIM_MS && stale) {
        it = enemyViews.erase(it);
        continue;
      }
      // Safety net: never keep a corpse around for long.
      if (view.deathMs > 2500.0f) {
        it = enemyViews.erase(it);
        continue;
      }
    } else if (stale) {
      it = enemyViews.erase(it);
      continue;
    }
    view.x += (view.state.x - view.x) * blend;
    view.y += (view.state.y - view.y) * blend;
    if (view.attackMs > 0.0f) {
      view.attackMs -= dtMs;
      if (view.attackMs <= 0.0f) {
        view.attackMs = 0.0f;
        view.strikeMs = 160.0f;
      }
    }
    view.strikeMs = std::max(0.0f, view.strikeMs - dtMs);
    view.hitFlashMs = std::max(0.0f, view.hitFlashMs - dtMs);
    view.hitMs += dtMs;
    view.animTime += dt;
    ++it;
  }

  for (auto& effect : fx) effect.ageMs += dtMs;
  fx.erase(std::remove_if(fx.begin(), fx.end(),
                          [](const Fx& f) { return f.ageMs >= f.lifeMs; }),
           fx.end());

  for (auto& shot : shots) {
    shot.ageMs += dtMs;
    shot.x += shot.vx * dt;
    shot.y += shot.vy * dt;
  }
  // The server normally ends projectiles; the grace period covers a lost
  // packet so nothing flies forever.
  shots.erase(std::remove_if(shots.begin(), shots.end(),
                             [](const Projectile& s) {
                               return s.ageMs >= s.lifeMs + 250.0f;
                             }),
              shots.end());

  for (auto& zone : groundZones) zone.ageMs += dtMs;
  groundZones.erase(std::remove_if(groundZones.begin(), groundZones.end(),
                                   [](const Zone& z) {
                                     return z.ageMs >= z.lifeMs + 250.0f;
                                   }),
                    groundZones.end());

  for (auto& text : texts) text.ageMs += dtMs;
  texts.erase(std::remove_if(texts.begin(), texts.end(),
                             [](const FloatText& t) {
                               return t.ageMs >= t.lifeMs;
                             }),
              texts.end());
  if (texts.size() > 160) texts.erase(texts.begin(), texts.end() - 160);

  for (auto& banner : bannerQueue) banner.ageMs += dtMs;
  bannerQueue.erase(std::remove_if(bannerQueue.begin(), bannerQueue.end(),
                                   [](const Banner& b) {
                                     return b.ageMs >= b.lifeMs;
                                   }),
                    bannerQueue.end());
}
