#pragma once

#include <SDL2/SDL.h>

#include <memory>

#include "Camera.h"
#include "ClientPrediction.h"
#include "CollisionDebugRenderer.h"
#include "EnemyInterpolation.h"
#include "EventBus.h"
#include "MusicZoneDebugRenderer.h"
#include "RemotePlayerInterpolation.h"
#include "SpriteRenderer.h"
#include "TileRenderer.h"
#include "TiledMap.h"
#include "Window.h"
#include "config/PlayerConfig.h"

class RenderSystem {
 public:
  RenderSystem(Window* window, ClientPrediction* clientPrediction,
               RemotePlayerInterpolation* remoteInterpolation,
               EnemyInterpolation* enemyInterpolation, Camera* camera,
               TiledMap* tiledMap,
               CollisionDebugRenderer* collisionDebugRenderer,
               MusicZoneDebugRenderer* musicZoneDebugRenderer);

  ~RenderSystem() = default;

 private:
  Window* window;
  ClientPrediction* clientPrediction;
  RemotePlayerInterpolation* remoteInterpolation;
  EnemyInterpolation* enemyInterpolation;
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
  void drawEnemy(const Enemy& enemy);
  void drawHealthBar(int x, int y, float health, float maxHealth);
};
