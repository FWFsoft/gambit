#pragma once

#include <algorithm>

struct Camera {
  float x, y;
  float worldWidth, worldHeight;
  int screenWidth, screenHeight;

  Camera(int screenW, int screenH)
      : x(0),
        y(0),
        worldWidth(800),
        worldHeight(600),
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
    float isoX = worldX - worldY;
    float isoY = (worldX + worldY) * 0.5f;

    float camIsoX = x - y;
    float camIsoY = (x + y) * 0.5f;

    screenX = static_cast<int>(isoX - camIsoX + screenWidth / 2);
    screenY = static_cast<int>(isoY - camIsoY + screenHeight / 2);
  }

  void screenToWorld(int screenX, int screenY, float& worldX,
                     float& worldY) const {
    float camIsoX = x - y;
    float camIsoY = (x + y) * 0.5f;

    float isoX = screenX + camIsoX - screenWidth / 2;
    float isoY = screenY + camIsoY - screenHeight / 2;

    worldX = isoY + (isoX * 0.5f);
    worldY = isoY - (isoX * 0.5f);
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
