#pragma once

#include <SDL2/SDL.h>

#include "Camera.h"
#include "ClientPrediction.h"
#include "EventBus.h"
#include "RemotePlayerInterpolation.h"
#include "TileRenderer.h"
#include "TiledMap.h"
#include "Window.h"

class RenderSystem {
 public:
  RenderSystem(Window* window, ClientPrediction* clientPrediction,
               RemotePlayerInterpolation* remoteInterpolation, Camera* camera,
               TiledMap* tiledMap, TileRenderer* tileRenderer);

  ~RenderSystem() = default;

 private:
  Window* window;
  ClientPrediction* clientPrediction;
  RemotePlayerInterpolation* remoteInterpolation;
  Camera* camera;
  TiledMap* tiledMap;
  TileRenderer* tileRenderer;

  void onRender(const RenderEvent& e);
  void drawPlayer(const Player& player);

  static constexpr int PLAYER_SIZE = 32;
};
