#pragma once

#include <SDL2/SDL.h>

#include <memory>

#include "Camera.h"
#include "ClientPrediction.h"
#include "CollisionDebugRenderer.h"
#include "EnemyInterpolation.h"
#include "EventBus.h"
#include "MusicZoneDebugRenderer.h"
#include "ObjectiveDebugRenderer.h"
#include "RemotePlayerInterpolation.h"
#include "SpriteRenderer.h"
#include "TileRenderer.h"
#include "TiledMap.h"
#include "Window.h"
#include "config/PlayerConfig.h"

class PresentationLayer;

class RenderSystem {
 public:
  RenderSystem(Window* window, ClientPrediction* clientPrediction,
               RemotePlayerInterpolation* remoteInterpolation,
               EnemyInterpolation* enemyInterpolation, Camera* camera,
               TiledMap* tiledMap,
               CollisionDebugRenderer* collisionDebugRenderer,
               MusicZoneDebugRenderer* musicZoneDebugRenderer,
               ObjectiveDebugRenderer* objectiveDebugRenderer);

  ~RenderSystem();

  // The look (dark world, light pools, bloom, grade). ArenaView and
  // UISystem need it to mark what glows and to resolve the scene.
  PresentationLayer* getPresentation() { return presentation.get(); }

  // Access to sprite renderer for other systems
  SpriteRenderer* getSpriteRenderer() { return spriteRenderer.get(); }

  // The arena view draws characters, enemies and effects itself; with this
  // off, RenderSystem only draws the tile layer and debug overlays.
  void setLegacyEntityRendering(bool enabled) { legacyEntities = enabled; }

  // Test mode: capture screenshot
  void captureScreenshot(const std::string& path);

 private:
  Window* window;
  ClientPrediction* clientPrediction;
  RemotePlayerInterpolation* remoteInterpolation;
  EnemyInterpolation* enemyInterpolation;
  Camera* camera;
  TiledMap* tiledMap;
  CollisionDebugRenderer* collisionDebugRenderer;
  MusicZoneDebugRenderer* musicZoneDebugRenderer;
  ObjectiveDebugRenderer* objectiveDebugRenderer;
  std::unique_ptr<SpriteRenderer> spriteRenderer;
  std::unique_ptr<Texture> whitePixelTexture;
  std::unique_ptr<TileRenderer> tileRenderer;
  std::unique_ptr<PresentationLayer> presentation;
  Texture* playerTexture;  // Managed by TextureManager, not owned
  Texture* shipTexture;    // Managed by TextureManager, not owned
  bool legacyEntities = true;

  void onRender(const RenderEvent& e);
  void drawPlayer(const Player& player);
  void drawEnemy(const Enemy& enemy);
  void drawWorldItem(const WorldItem& worldItem);
  void drawObjective(const ClientObjective& objective);
  void drawShip(float worldX, float worldY);
  void drawHealthBar(int x, int y, float health, float maxHealth);
};
