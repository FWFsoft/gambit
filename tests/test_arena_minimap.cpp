#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#include "ArenaMinimap.h"

namespace {
struct Match {
  float shipX, shipY, zoneRadius;
};
struct Zone {
  int variant;
  float x, y;
};
bool near(float a, float b) { return std::abs(a - b) < 0.02f; }
}  // namespace

int main() {
  Match match{250.0f, -300.0f, 900.0f};
  std::vector<Zone> zones;
  auto map = arenaMinimap(match, match.shipX, match.shipY, zones);
  assert(near(map.range, 1035.0f));  // existing zone framing unchanged
  assert(near(map.mapX(match.shipX), ArenaMinimap::X));
  assert(near(map.mapY(match.shipY), ArenaMinimap::Y));
  assert(ArenaMinimap::contains(ArenaMinimap::X, ArenaMinimap::Y));
  assert(!ArenaMinimap::contains(ArenaMinimap::X + 87, ArenaMinimap::Y));
  // A bounding-box corner must not capture world aiming outside the circle.
  assert(!ArenaMinimap::contains(ArenaMinimap::X + 85, ArenaMinimap::Y + 85));
  match.zoneRadius = 0;
  assert(near(arenaMinimap(match, 250, -300, zones).range, 700));

  // Travel far in every direction, including beyond the former fixed circle.
  for (float dx : {-12000.0f, 0.0f, 12000.0f}) {
    for (float dy : {-9000.0f, 0.0f, 9000.0f}) {
      float x = match.shipX + dx, y = match.shipY + dy;
      map = arenaMinimap(match, x, y, zones);
      assert(ArenaMinimap::contains(map.mapX(x), map.mapY(y)));
      assert(std::hypot(map.mapX(x) - ArenaMinimap::X,
                        map.mapY(y) - ArenaMinimap::Y) < 82);
      assert(near(map.worldX(map.mapX(x)), x));
      assert(near(map.worldY(map.mapY(y)), y));
    }
  }
  // Only mirrors affect zoom; remote targets remain at their real positions.
  zones = {{0, 999999, 999999}, {Arena::KadeMode::MirrorZone, -20000, 18000}};
  map = arenaMinimap(match, 250, -300, zones);
  assert(map.range < 40000);
  const Zone& mirror = zones.back();
  assert(ArenaMinimap::contains(map.mapX(mirror.x), map.mapY(mirror.y)));
  assert(near(map.worldX(map.mapX(mirror.x)), mirror.x));
  assert(near(map.worldY(map.mapY(mirror.y)), mirror.y));
  zones.clear();
  assert(near(arenaMinimap(match, 250, -300, zones).range, 700));
  std::cout << "arena minimap: all tests passed\n";
}
