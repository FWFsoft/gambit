#pragma once

#include <algorithm>

#include "config/ScreenConfig.h"

struct Camera {
  float x, y;
  float worldWidth, worldHeight;
  int screenWidth, screenHeight;

  Camera(int screenW, int screenH)
      : x(0),
        y(0),
        worldWidth(Config::Screen::WIDTH),
        worldHeight(Config::Screen::HEIGHT),
        screenWidth(screenW),
        screenHeight(screenH) {}

  void follow(float targetX, float targetY) {
    x = targetX;
    y = targetY;
  }

  void setWorldBounds(float width, float height) {
    worldWidth = width;
    worldHeight = height;
  }

  void worldToScreen(float worldX, float worldY, int& screenX,
                     int& screenY) const {
    // gridToWorld already converts to isometric coordinates,
    // so just apply camera offset like TileRenderer shader does
    screenX = static_cast<int>(worldX - x + screenWidth / 2);
    screenY = static_cast<int>(worldY - y + screenHeight / 2);
  }

  void screenToWorld(int screenX, int screenY, float& worldX,
                     float& worldY) const {
    // Reverse of worldToScreen - simple offset since coordinates are already
    // isometric
    worldX = screenX + x - screenWidth / 2;
    worldY = screenY + y - screenHeight / 2;
  }

 private:
  void clampToWorldBounds() {
    float halfW = screenWidth / 2.0f;
    float halfH = screenHeight / 2.0f;

    if (worldWidth <= screenWidth) {
      x = worldWidth / 2.0f;
    } else {
      x = std::clamp(x, halfW, worldWidth - halfW);
    }

    if (worldHeight <= screenHeight) {
      y = worldHeight / 2.0f;
    } else {
      y = std::clamp(y, halfH, worldHeight - halfH);
    }
  }
};
