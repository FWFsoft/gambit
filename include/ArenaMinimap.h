#pragma once

#include <algorithm>
#include <cmath>

#include "ArenaData.h"
#include "config/ScreenConfig.h"

// One transform for drawing, terrain sampling and cursor aiming. Keep the
// ship as the anchor; zoom out just enough to include the hero and mirrors.
struct ArenaMinimap {
  static constexpr float RADIUS = 86.0f;
  static constexpr float X = Config::Screen::WIDTH - RADIUS - 14.0f;
  static constexpr float Y = Config::Screen::HEIGHT - RADIUS - 14.0f;
  float shipX, shipY, range;

  float scale() const { return RADIUS / range; }
  float mapX(float x) const { return X + (x - shipX) * scale(); }
  float mapY(float y) const { return Y + (y - shipY) * scale(); }
  float worldX(float x) const { return shipX + (x - X) / scale(); }
  float worldY(float y) const { return shipY + (y - Y) / scale(); }
  static bool contains(float x, float y) {
    return (x - X) * (x - X) + (y - Y) * (y - Y) <= RADIUS * RADIUS;
  }
  void include(float x, float y) {
    range = std::max(range, std::hypot(x - shipX, y - shipY) * 1.15f);
  }
};

template <class Match, class Zones>
ArenaMinimap arenaMinimap(const Match& match, float playerX, float playerY,
                          const Zones& zones) {
  ArenaMinimap map{match.shipX, match.shipY,
                   std::max(700.0f, match.zoneRadius * 1.15f)};
  map.include(playerX, playerY);
  for (const auto& zone : zones) {
    if (zone.variant == Arena::KadeMode::MirrorZone) map.include(zone.x, zone.y);
  }
  return map;
}
