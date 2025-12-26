#pragma once

#include <SDL2/SDL.h>

#include <memory>

#include "Camera.h"
#include "ClientPrediction.h"
#include "CollisionDebugRenderer.h"
#include "EventBus.h"
#include "MusicZoneDebugRenderer.h"
#include "RemotePlayerInterpolation.h"
#include "SpriteRenderer.h"
#include "TileRenderer.h"
#include "TiledMap.h"
#include "Window.h"

class RenderSystem {
 public:
  RenderSystem(Window* window, ClientPrediction* clientPrediction,
               RemotePlayerInterpolation* remoteInterpolation, Camera* camera,
               TiledMap* tiledMap,
               CollisionDebugRenderer* collisionDebugRenderer,
               MusicZoneDebugRenderer* musicZoneDebugRenderer);

  ~RenderSystem() = default;

 private:
  Window* window;
  ClientPrediction* clientPrediction;
  RemotePlayerInterpolation* remoteInterpolation;
  Camera* camera;
  TiledMap* tiledMap;
  CollisionDebugRenderer* collisionDebugRenderer;
  MusicZoneDebugRenderer* musicZoneDebugRenderer;
  std::unique_ptr<SpriteRenderer> spriteRenderer;
  std::unique_ptr<Texture> whitePixelTexture;
  std::unique_ptr<TileRenderer> tileRenderer;
  Texture* playerTexture;  // Managed by TextureManager, not owned

  void onRender(const RenderEvent& e);
  void drawPlayer(const Player& player);

  static constexpr int PLAYER_SIZE = 32;
};
