// WASM Client Entry Point
// Browser-compatible version of the game client
// Uses stub transport for MVP (no networking yet)

#include <memory>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "AnimationAssetLoader.h"
#include "AnimationSystem.h"
#include "Camera.h"
#include "CharacterRegistry.h"
#include "CharacterSelectionState.h"
#include "ClientPrediction.h"
#include "CollisionDebugRenderer.h"
#include "CollisionSystem.h"
#include "CombatSystem.h"
#include "DamageNumberSystem.h"
#include "EffectTracker.h"
#include "EnemyInterpolation.h"
#include "EventBus.h"
#include "GameLoop.h"
#include "GameStateManager.h"
#include "InputSystem.h"
#include "ItemRegistry.h"
#include "Logger.h"
#include "MapSelectionState.h"
#include "MusicSystem.h"
#include "MusicZoneDebugRenderer.h"
#include "NetworkClient.h"
#include "NetworkProtocol.h"
#include "Objective.h"
#include "ObjectiveDebugRenderer.h"
#include "RemotePlayerInterpolation.h"
#include "RenderSystem.h"
#include "TileRenderer.h"
#include "TiledMap.h"
#include "UISystem.h"
#include "Window.h"
#include "WorldConfig.h"
#include "config/ScreenConfig.h"
#include "config/TimingConfig.h"
#include "GameSession.h"

int main(int argc, char* argv[]) {
  Logger::init();
  Logger::info("=== WASM Client Starting ===");

  // Load item definitions
  if (!ItemRegistry::instance().loadFromCSV("assets/items.csv")) {
    Logger::error("Failed to load items.csv - inventory will be empty");
  }

  // Create embedded game session (client + server in same process)
  auto gameSession = GameSession::create();
  if (!gameSession) {
    Logger::error("Failed to create embedded game session");
    return EXIT_FAILURE;
  }

  NetworkClient& client = *gameSession->getClient();

  // Start in title screen (same as native client)
  GameStateManager::instance().transitionTo(GameState::TitleScreen);

  Window window("Gambit WASM", Config::Screen::WIDTH, Config::Screen::HEIGHT);
  window.initImGui();
  GameLoop gameLoop;

  // Map is already loaded by GameSession, but we need a reference for rendering
  TiledMap map;
  std::string mapPath = MapSelectionState::instance().getSelectedMapPath();
  if (!map.load(mapPath)) {
    Logger::error("Failed to load map: " + mapPath);
    return EXIT_FAILURE;
  }

  CollisionSystem collisionSystem(map.getCollisionShapes());
  Logger::info("Collision system initialized");

  Camera camera(Config::Screen::WIDTH, Config::Screen::HEIGHT);
  camera.setWorldBounds(map.getWorldWidth(), map.getWorldHeight());

  CollisionDebugRenderer collisionDebugRenderer(&camera, &collisionSystem);
  MusicZoneDebugRenderer musicZoneDebugRenderer(&camera, &map);

  uint32_t localPlayerId = 1;  // Fixed player ID for embedded mode
  WorldConfig world(map.getWorldWidth(), map.getWorldHeight(),
                    &collisionSystem);
  ClientPrediction clientPrediction(&client, localPlayerId, world);

  ObjectiveDebugRenderer objectiveDebugRenderer(&camera, &clientPrediction);

  InputSystem inputSystem(&clientPrediction, &collisionDebugRenderer,
                          &musicZoneDebugRenderer, &objectiveDebugRenderer);

  AnimationSystem animationSystem;
  MusicSystem musicSystem(&clientPrediction, &map);

  RemotePlayerInterpolation remoteInterpolation(localPlayerId,
                                                &animationSystem);
  EnemyInterpolation enemyInterpolation(&animationSystem);
  CombatSystem combatSystem(&client, &clientPrediction, &enemyInterpolation);

  RenderSystem renderSystem(&window, &clientPrediction, &remoteInterpolation,
                            &enemyInterpolation, &camera, &map,
                            &collisionDebugRenderer, &musicZoneDebugRenderer,
                            &objectiveDebugRenderer);

  DamageNumberSystem damageNumberSystem(&camera,
                                        renderSystem.getSpriteRenderer());
  EffectTracker effectTracker;
  UISystem uiSystem(&window, &clientPrediction, &client, &damageNumberSystem,
                    &effectTracker);

  // Load local player animations
  Player& localPlayer = clientPrediction.getLocalPlayerMutable();
  AnimationAssetLoader::loadPlayerAnimations(
      *localPlayer.getAnimationController(), "assets/player_animated.png");
  animationSystem.registerEntity(&localPlayer);

  // Default player appearance
  localPlayer.r = 255;
  localPlayer.g = 255;
  localPlayer.b = 255;
  localPlayer.characterId = 0;

  // Add test items to inventory
  localPlayer.inventory[0] = ItemStack(1, 5);
  localPlayer.inventory[1] = ItemStack(2, 3);
  localPlayer.inventory[2] = ItemStack(3, 1);

  // Subscribe to UpdateEvent
  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
    window.pollEvents();

    // Tick the embedded game session (processes both server and client)
    gameSession->tick();

    if (!window.isOpen()) {
      gameLoop.stop();
    }

    if (e.frameNumber % Config::Timing::LOG_FRAME_INTERVAL == 0) {
      Logger::debug("Frame: " + std::to_string(e.frameNumber));
    }
  });

  Logger::info("Starting game loop");
  gameLoop.run();

  Logger::info("=== WASM Client Shutting Down ===");
  return EXIT_SUCCESS;
}
