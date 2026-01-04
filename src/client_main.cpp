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
#include "MusicSystem.h"
#include "MusicZoneDebugRenderer.h"
#include "NetworkClient.h"
#include "RemotePlayerInterpolation.h"
#include "RenderSystem.h"
#include "TileRenderer.h"
#include "TiledMap.h"
#include "UISystem.h"
#include "Window.h"
#include "WorldConfig.h"
#include "config/NetworkConfig.h"
#include "config/ScreenConfig.h"
#include "config/TimingConfig.h"

int main() {
  Logger::init();

  // Load item definitions
  if (!ItemRegistry::instance().loadFromCSV("assets/items.csv")) {
    Logger::error("Failed to load items.csv - inventory will be empty");
  }

  NetworkClient client;
  if (!client.initialize()) {
    return EXIT_FAILURE;
  }

  if (!client.connect(Config::Network::SERVER_ADDRESS, Config::Network::PORT)) {
    return EXIT_FAILURE;
  }

  // Start in title screen
  GameStateManager::instance().transitionTo(GameState::TitleScreen);

  Window window("Gambit Client", Config::Screen::WIDTH, Config::Screen::HEIGHT);
  window.initImGui();
  GameLoop gameLoop;

  TiledMap map;
  assert(map.load("assets/test_map.tmx") && "Failed to load required map");

  CollisionSystem collisionSystem(map.getCollisionShapes());
  Logger::info("Client collision system initialized");

  Camera camera(Config::Screen::WIDTH, Config::Screen::HEIGHT);
  camera.setWorldBounds(map.getWorldWidth(), map.getWorldHeight());

  CollisionDebugRenderer collisionDebugRenderer(&camera, &collisionSystem);
  MusicZoneDebugRenderer musicZoneDebugRenderer(&camera, &map);

  uint32_t localPlayerId = (uint32_t)(uintptr_t)&client;
  WorldConfig world(map.getWorldWidth(), map.getWorldHeight(),
                    &collisionSystem);
  ClientPrediction clientPrediction(&client, localPlayerId, world);

  InputSystem inputSystem(&clientPrediction, &collisionDebugRenderer,
                          &musicZoneDebugRenderer);

  // Initialize animation system first
  AnimationSystem animationSystem;

  // Create music system (zone-based)
  MusicSystem musicSystem(&clientPrediction, &map);

  // Create remote player interpolation with animation system
  RemotePlayerInterpolation remoteInterpolation(localPlayerId,
                                                &animationSystem);

  // Create enemy interpolation with animation system
  EnemyInterpolation enemyInterpolation(&animationSystem);

  // Create combat system
  CombatSystem combatSystem(&client, &clientPrediction, &enemyInterpolation);

  RenderSystem renderSystem(&window, &clientPrediction, &remoteInterpolation,
                            &enemyInterpolation, &camera, &map,
                            &collisionDebugRenderer, &musicZoneDebugRenderer);

  // Create damage number system
  DamageNumberSystem damageNumberSystem(&camera,
                                        renderSystem.getSpriteRenderer());

  // Create effect tracker
  EffectTracker effectTracker;

  UISystem uiSystem(&window, &clientPrediction, &client, &damageNumberSystem,
                    &effectTracker);

  // Load local player animations
  Player& localPlayer = clientPrediction.getLocalPlayerMutable();
  AnimationAssetLoader::loadPlayerAnimations(
      *localPlayer.getAnimationController(), "assets/player_animated.png");
  animationSystem.registerEntity(&localPlayer);

  // Apply character selection (color tint for now)
  if (CharacterSelectionState::instance().hasSelection()) {
    uint32_t selectedId =
        CharacterSelectionState::instance().getSelectedCharacterId();
    const CharacterDefinition* character =
        CharacterRegistry::instance().getCharacter(selectedId);

    if (character) {
      // Apply character color tint to local player
      localPlayer.r = character->r;
      localPlayer.g = character->g;
      localPlayer.b = character->b;
      localPlayer.characterId = selectedId;

      Logger::info("Applied character selection: " + character->name +
                   " (ID: " + std::to_string(selectedId) +
                   ", RGB: " + std::to_string(character->r) + "," +
                   std::to_string(character->g) + "," +
                   std::to_string(character->b) + ")");
    }
  } else {
    // Fallback: use default white if no selection
    localPlayer.r = 255;
    localPlayer.g = 255;
    localPlayer.b = 255;
    localPlayer.characterId = 0;
    Logger::info("No character selected, using default appearance");
  }

  // Add test items to inventory for demonstration
  localPlayer.inventory[0] = ItemStack(1, 5);    // 5x Health Potion
  localPlayer.inventory[1] = ItemStack(2, 3);    // 3x Greater Health Potion
  localPlayer.inventory[2] = ItemStack(3, 1);    // Iron Sword
  localPlayer.inventory[3] = ItemStack(6, 1);    // Leather Armor
  localPlayer.inventory[5] = ItemStack(4, 1);    // Steel Sword
  localPlayer.inventory[10] = ItemStack(9, 10);  // 10x Mana Potion

  // Equip items
  localPlayer.equipment[EQUIPMENT_WEAPON_SLOT] =
      ItemStack(5, 1);  // Dragon Sword
  localPlayer.equipment[EQUIPMENT_ARMOR_SLOT] = ItemStack(7, 1);  // Iron Armor

  // Subscribe to UpdateEvent for network processing
  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
    // Poll window events
    window.pollEvents();

    // Process network events
    client.run();

    // Auto-pickup detection for nearby world items
    GameState currentState = GameStateManager::instance().getCurrentState();
    if (currentState == GameState::Playing) {
      const Player& localPlayer = clientPrediction.getLocalPlayer();

      if (localPlayer.isAlive()) {
        const auto& worldItems = clientPrediction.getWorldItems();
        constexpr float PICKUP_RADIUS = 32.0f;

        for (const auto& [worldItemId, worldItem] : worldItems) {
          float dx = localPlayer.x - worldItem.x;
          float dy = localPlayer.y - worldItem.y;
          float distanceSquared = dx * dx + dy * dy;

          if (distanceSquared <= PICKUP_RADIUS * PICKUP_RADIUS) {
            // Send pickup request to server
            ItemPickupRequestPacket packet;
            packet.worldItemId = worldItemId;
            client.send(serialize(packet));

            Logger::debug("Requesting pickup of world item " +
                          std::to_string(worldItemId));
            break;  // Only one item per frame
          }
        }
      }
    }

    // Stop the game loop if window is closed
    if (!window.isOpen()) {
      gameLoop.stop();
    }

    // Log every second to verify target FPS
    if (e.frameNumber % Config::Timing::LOG_FRAME_INTERVAL == 0) {
      Logger::debug("Frame: " + std::to_string(e.frameNumber) +
                    " (deltaTime: " + std::to_string(e.deltaTime) + "ms)");
    }
  });

  // Subscribe to LocalInputEvent for testing (log when any input changes)
  bool lastInputState = false;
  EventBus::instance().subscribe<LocalInputEvent>(
      [&](const LocalInputEvent& e) {
        bool hasInput = e.moveLeft || e.moveRight || e.moveUp || e.moveDown;
        if (hasInput && !lastInputState) {
          Logger::info("Input active - L:" + std::to_string(e.moveLeft) +
                       " R:" + std::to_string(e.moveRight) +
                       " U:" + std::to_string(e.moveUp) +
                       " D:" + std::to_string(e.moveDown) +
                       " Seq:" + std::to_string(e.inputSequence));
        }
        lastInputState = hasInput;
      });

  // Run the game loop
  gameLoop.run();

  client.send("Window closed, disconnecting from Gambit Server!");
  client.disconnect();

  return EXIT_SUCCESS;
}
