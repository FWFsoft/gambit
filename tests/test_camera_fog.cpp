// Camera modes (locked / unlocked, edge panning) and the fog of war model.

#include <cassert>
#include <cmath>
#include <iostream>

#include "Camera.h"
#include "FogOfWar.h"
#include "config/CameraConfig.h"

namespace {

namespace Tune = Config::CameraView;
constexpr int W = Config::Screen::WIDTH;
constexpr int H = Config::Screen::HEIGHT;

bool near(float a, float b, float epsilon = 0.01f) {
  return std::abs(a - b) < epsilon;
}

Camera makeCamera() {
  Camera camera(W, H);
  camera.setWorldBounds(8000.0f, 4000.0f);
  return camera;
}

// --- Camera -----------------------------------------------------------------

void test_Camera_LockedFollowsAndIgnoresEdges() {
  Camera camera = makeCamera();
  camera.setMode(Camera::Mode::Locked);
  assert(camera.isFollowing());
  camera.follow(120.0f, -40.0f);
  camera.pan(W - 1, H - 1, 1.0f / 60.0f);  // cursor in the corner
  assert(near(camera.x, 120.0f) && near(camera.y, -40.0f));
}

void test_Camera_UnlockedPansFromEdges() {
  Camera camera = makeCamera();
  camera.setMode(Camera::Mode::Unlocked);
  assert(!camera.isFollowing());

  camera.pan(W / 2, H / 2, 1.0f / 60.0f);  // cursor mid-screen: stays put
  assert(near(camera.x, 0.0f) && near(camera.y, 0.0f));

  camera.pan(W - 1, H / 2, 1.0f / 60.0f);  // right edge
  assert(camera.x > 0.0f && near(camera.y, 0.0f));
  float afterRight = camera.x;
  camera.pan(0, 0, 1.0f / 60.0f);  // top-left corner
  assert(camera.x < afterRight && camera.y < 0.0f);
}

void test_Camera_UnlockedStaysOnTheMap() {
  Camera camera = makeCamera();
  camera.setMode(Camera::Mode::Unlocked);
  for (int i = 0; i < 2000; i++) camera.pan(W - 1, H - 1, 1.0f / 60.0f);
  assert(near(camera.x, 4000.0f) && near(camera.y, 2000.0f));
  for (int i = 0; i < 4000; i++) camera.pan(0, 0, 1.0f / 60.0f);
  assert(near(camera.x, -4000.0f) && near(camera.y, -2000.0f));
}

void test_Camera_HitchDoesNotThrowTheView() {
  Camera camera = makeCamera();
  camera.setMode(Camera::Mode::Unlocked);
  camera.pan(W - 1, H / 2, 5.0f);  // a five second hitch
  assert(camera.x <= Tune::EDGE_PAN_SPEED * 0.05f + 0.01f);
}

void test_Camera_RecentreFollowsWhileHeld() {
  Camera camera = makeCamera();
  camera.setMode(Camera::Mode::Unlocked);
  camera.recentreHeld = true;
  assert(camera.isFollowing());
  camera.pan(W - 1, H / 2, 1.0f / 60.0f);
  assert(near(camera.x, 0.0f));
  camera.toggleMode();  // back to locked: the hold is forgotten
  assert(camera.isLocked() && !camera.recentreHeld);
}

void test_Camera_ScreenWorldRoundTrip() {
  Camera camera = makeCamera();
  camera.follow(300.0f, -200.0f);
  float wx = 0.0f, wy = 0.0f;
  camera.screenToWorld(W / 2, H / 2, wx, wy);
  assert(near(wx, 300.0f) && near(wy, -200.0f));
  // The right screen edge is half a screen away, measured in world pixels.
  camera.screenToWorld(W, H / 2, wx, wy);
  assert(near(wx, 300.0f + W * 0.5f / camera.zoom));
  int sx = 0, sy = 0;
  camera.worldToScreen(wx, wy, sx, sy);
  assert(std::abs(sx - W) <= 1 && std::abs(sy - H / 2) <= 1);
}

// --- Fog of war -------------------------------------------------------------

void test_Fog_StartsUnexplored() {
  FogOfWar fog(4000.0f, 2000.0f);
  assert(fog.visibility(0.0f, 0.0f) == 0.0f);
  assert(!fog.reveals(0.0f, 0.0f));
  assert(fog.explored(0.0f, 0.0f) == 0.0f);
  assert(fog.texels().size() ==
         static_cast<size_t>(fog.gridWidth()) * fog.gridHeight() * 4);
  // Every texel is solid fog.
  const int solid = static_cast<int>(Tune::FOG_UNEXPLORED * 255.0f + 0.5f);
  for (size_t i = 3; i < fog.texels().size(); i += 4) {
    assert(fog.texels()[i] == solid);
  }
  // The image covers the whole map.
  assert(fog.originX() <= -2000.0f && fog.originX() + fog.spanX() >= 2000.0f);
  assert(fog.originY() <= -1000.0f && fog.originY() + fog.spanY() >= 1000.0f);
}

void test_Fog_SourceRevealsACircle() {
  FogOfWar fog(4000.0f, 2000.0f);
  fog.update({{100.0f, 50.0f, 400.0f}});
  assert(near(fog.visibility(100.0f, 50.0f), 1.0f));
  assert(fog.reveals(100.0f + 200.0f, 50.0f));
  assert(fog.reveals(100.0f, 50.0f - 200.0f));  // a circle, not an ellipse
  assert(!fog.reveals(100.0f + 401.0f, 50.0f));
  assert(fog.visibility(100.0f + 500.0f, 50.0f) == 0.0f);
  // Something big is seen as soon as its rim enters sight.
  assert(fog.reveals(100.0f + 440.0f, 50.0f, 80.0f));
  assert(!fog.reveals(100.0f + 440.0f, 50.0f));  // its centre alone is not
  assert(!fog.reveals(100.0f + 490.0f, 50.0f, 80.0f));
  // Sight fades towards its edge.
  float inner = fog.visibility(100.0f + 300.0f, 50.0f);
  float outer = fog.visibility(100.0f + 380.0f, 50.0f);
  assert(inner > outer && outer > 0.0f && inner < 1.0f);
}

void test_Fog_SightIsShared() {
  FogOfWar fog(4000.0f, 2000.0f);
  fog.update({{-800.0f, 0.0f, 300.0f}, {800.0f, 0.0f, 300.0f}});
  assert(fog.reveals(-800.0f, 0.0f));
  assert(fog.reveals(800.0f, 0.0f));  // the teammate's sight is yours
  assert(!fog.reveals(0.0f, 0.0f));   // the gap between them is not
}

void test_Fog_RemembersWhereTheCrewHasBeen() {
  FogOfWar fog(4000.0f, 2000.0f);
  fog.update({{0.0f, 0.0f, 400.0f}});
  fog.update({{1500.0f, 0.0f, 400.0f}});      // walked away
  assert(!fog.reveals(0.0f, 0.0f));           // enemies there are hidden again
  assert(fog.explored(0.0f, 0.0f) >= 0.99f);  // but the ground is known
  assert(fog.explored(0.0f, 900.0f) == 0.0f);

  // Explored ground is covered less than unexplored ground, seen ground not
  // at all.
  auto alphaAt = [&](float x, float y) {
    int column = static_cast<int>((x - fog.originX()) / Tune::FOG_CELL);
    int row = static_cast<int>((y - fog.originY()) / Tune::FOG_CELL);
    return fog
        .texels()[(static_cast<size_t>(row) * fog.gridWidth() + column) * 4 +
                  3];
  };
  int seen = alphaAt(1500.0f, 0.0f);
  int explored = alphaAt(0.0f, 0.0f);
  int unexplored = alphaAt(0.0f, 900.0f);
  assert(seen == 0);
  assert(explored > seen && explored < unexplored);

  fog.reset();  // a new match
  assert(fog.explored(0.0f, 0.0f) == 0.0f);
  assert(!fog.reveals(1500.0f, 0.0f));
}

void test_Fog_OutsideTheMapIsNeverExplored() {
  FogOfWar fog(4000.0f, 2000.0f);
  fog.update({{0.0f, 0.0f, 400.0f}});
  assert(fog.explored(99999.0f, 0.0f) == 0.0f);
  assert(fog.explored(0.0f, -99999.0f) == 0.0f);
  assert(!fog.reveals(99999.0f, 99999.0f));
}

}  // namespace

int main() {
  test_Camera_LockedFollowsAndIgnoresEdges();
  test_Camera_UnlockedPansFromEdges();
  test_Camera_UnlockedStaysOnTheMap();
  test_Camera_HitchDoesNotThrowTheView();
  test_Camera_RecentreFollowsWhileHeld();
  test_Camera_ScreenWorldRoundTrip();

  test_Fog_StartsUnexplored();
  test_Fog_SourceRevealsACircle();
  test_Fog_SightIsShared();
  test_Fog_RemembersWhereTheCrewHasBeen();
  test_Fog_OutsideTheMapIsNeverExplored();

  std::cout << "camera and fog of war: all tests passed" << std::endl;
  return 0;
}
