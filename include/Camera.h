#pragma once

#include <algorithm>
#include <cassert>

#include "config/CameraConfig.h"
#include "config/ScreenConfig.h"

// The view onto the world: a centre (world coordinates), a zoom, and one of
// two modes.
//
//   Locked:   centred on the hero every frame.
//   Unlocked: stays where it is. The cursor pans it from the screen edges,
//             and holding "recentre" (Space) follows the hero again.
//
// The world is centred on the origin: x in [-worldWidth/2, worldWidth/2].
struct Camera {
  enum class Mode { Locked, Unlocked };

  float x, y;
  float worldWidth, worldHeight;
  int screenWidth, screenHeight;
  float zoom = Config::Screen::WORLD_ZOOM;  // world magnification
  Mode mode = Config::CameraView::START_LOCKED ? Mode::Locked : Mode::Unlocked;
  bool recentreHeld = false;  // Space, while unlocked

  Camera(int screenW, int screenH)
      : x(0),
        y(0),
        worldWidth(Config::Screen::WIDTH),
        worldHeight(Config::Screen::HEIGHT),
        screenWidth(screenW),
        screenHeight(screenH) {}

  bool isLocked() const { return mode == Mode::Locked; }
  // True when the camera is centred on the hero this frame.
  bool isFollowing() const { return isLocked() || recentreHeld; }
  void setMode(Mode newMode) {
    mode = newMode;
    recentreHeld = false;
  }
  void toggleMode() { setMode(isLocked() ? Mode::Unlocked : Mode::Locked); }

  void follow(float targetX, float targetY) {
    x = targetX;
    y = targetY;
  }

  // Edge panning for the unlocked camera, once per update tick. Does
  // nothing while the camera follows the hero.
  void pan(int cursorX, int cursorY, float deltaSeconds) {
    assert(zoom > 0.0f);
    assert(deltaSeconds >= 0.0f);
    if (isFollowing()) return;

    const float margin = Config::CameraView::EDGE_PAN_MARGIN;
    // -1..1 per axis: how hard the cursor pushes on that edge.
    auto push = [margin](int cursor, int size) {
      if (cursor < margin) return -std::min(1.0f, (margin - cursor) / margin);
      if (cursor > size - margin) {
        return std::min(1.0f, (cursor - (size - margin)) / margin);
      }
      return 0.0f;
    };
    // Never jump further than a few frames' worth after a hitch.
    const float step =
        Config::CameraView::EDGE_PAN_SPEED * std::min(deltaSeconds, 0.05f);
    x += push(cursorX, screenWidth) * step;
    y += push(cursorY, screenHeight) * step;
    clampToWorldBounds();
  }

  void setWorldBounds(float width, float height) {
    worldWidth = width;
    worldHeight = height;
  }

  void worldToScreen(float worldX, float worldY, int& screenX,
                     int& screenY) const {
    // gridToWorld already converts to isometric coordinates,
    // so just apply camera offset like TileRenderer shader does
    screenX = static_cast<int>((worldX - x) * zoom + screenWidth / 2);
    screenY = static_cast<int>((worldY - y) * zoom + screenHeight / 2);
  }

  void screenToWorld(int screenX, int screenY, float& worldX,
                     float& worldY) const {
    // Reverse of worldToScreen - simple offset since coordinates are already
    // isometric
    worldX = (screenX - screenWidth / 2) / zoom + x;
    worldY = (screenY - screenHeight / 2) / zoom + y;
  }

 private:
  // A free camera may look at any part of the map, but not leave it.
  void clampToWorldBounds() {
    x = std::clamp(x, -worldWidth / 2.0f, worldWidth / 2.0f);
    y = std::clamp(y, -worldHeight / 2.0f, worldHeight / 2.0f);
  }
};
