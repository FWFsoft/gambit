#include <memory>

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
#include "TestInputReader.h"
#include "TileRenderer.h"
#include "TiledMap.h"
#include "UISystem.h"
#include "Window.h"
#include "WorldConfig.h"
#include "config/NetworkConfig.h"
#include "config/ScreenConfig.h"
#include "config/TestMode.h"
#include "config/TimingConfig.h"
#include "transport/ENetTransport.h"
#include "transport/InMemoryTransport.h"
#include "GameSession.h"

int main(int argc, char* argv[]) {
  Logger::init();

  // Parse command line arguments
  bool embeddedMode = false;
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--test-mode") {
      Config::TestMode::testConfig.enabled = true;
      Logger::info("Test mode enabled");
    } else if (arg == "--embedded") {
      embeddedMode = true;
      Logger::info("Embedded server mode enabled");
    }
  }

  // Load item definitions
  if (!ItemRegistry::instance().loadFromCSV("assets/items.csv")) {
    Logger::error("Failed to load items.csv - inventory will be empty");
  }

  // Create client with appropriate transport
  std::unique_ptr<GameSession> gameSession;
  NetworkClient* clientPtr = nullptr;

  if (embeddedMode) {
    // Embedded server mode: client and server in same process
    gameSession = GameSession::create();
    if (!gameSession) {
      Logger::error("Failed to create embedded game session");
      return EXIT_FAILURE;
    }
    clientPtr = gameSession->getClient();
  } else {
    // Network mode: connect to remote server
    static auto transport = std::make_unique<ENetTransport>();
    static NetworkClient networkClient(std::move(transport));
    if (!networkClient.connect(Config::Network::SERVER_ADDRESS,
                               Config::Network::PORT)) {
      return EXIT_FAILURE;
    }
    clientPtr = &networkClient;
  }

  NetworkClient& client = *clientPtr;

  // Start in title screen
  GameStateManager::instance().transitionTo(GameState::TitleScreen);

  Window window("Gambit Client", Config::Screen::WIDTH, Config::Screen::HEIGHT);
  window.initImGui();
  GameLoop gameLoop;

  TiledMap map;
  std::string mapPath = MapSelectionState::instance().getSelectedMapPath();
  assert(map.load(mapPath) && "Failed to load required map");

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

  ObjectiveDebugRenderer objectiveDebugRenderer(&camera, &clientPrediction);

  InputSystem inputSystem(&clientPrediction, &collisionDebugRenderer,
                          &musicZoneDebugRenderer, &objectiveDebugRenderer);

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
                            &collisionDebugRenderer, &musicZoneDebugRenderer,
                            &objectiveDebugRenderer);

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

  // Initialize test input reader if test mode enabled
  std::unique_ptr<TestInputReader> testInputReader;
  if (Config::TestMode::testConfig.enabled) {
    testInputReader = std::make_unique<TestInputReader>(
        Config::TestMode::testConfig.inputCommandPath);
  }

  // Subscribe to UpdateEvent for network processing
  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
    // Test mode: process input commands and capture screenshots
    if (Config::TestMode::testConfig.enabled && testInputReader) {
      testInputReader->tick();

      // Try to read next command
      auto command = testInputReader->readNextCommand();
      if (command) {
        switch (command->type) {
          case TestInputReader::CommandType::KEY_DOWN: {
            SDL_Scancode scancode =
                TestInputReader::stringToScancode(command->stringArg);
            if (scancode != SDL_SCANCODE_UNKNOWN) {
              SDL_Event keyEvent;
              keyEvent.type = SDL_KEYDOWN;
              keyEvent.key.keysym.scancode = scancode;
              keyEvent.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
              keyEvent.key.keysym.mod = 0;
              SDL_PushEvent(&keyEvent);
            }
            break;
          }
          case TestInputReader::CommandType::KEY_UP: {
            SDL_Scancode scancode =
                TestInputReader::stringToScancode(command->stringArg);
            if (scancode != SDL_SCANCODE_UNKNOWN) {
              SDL_Event keyEvent;
              keyEvent.type = SDL_KEYUP;
              keyEvent.key.keysym.scancode = scancode;
              keyEvent.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
              keyEvent.key.keysym.mod = 0;
              SDL_PushEvent(&keyEvent);
            }
            break;
          }
          case TestInputReader::CommandType::SCREENSHOT: {
            std::string filename = Config::TestMode::testConfig.screenshotPath;
            if (!command->stringArg.empty()) {
              filename = "test_output/" + command->stringArg + ".png";
            }
            renderSystem.captureScreenshot(filename);
            break;
          }
          case TestInputReader::CommandType::MOUSE_MOVE: {
            SDL_Event mouseEvent;
            mouseEvent.type = SDL_MOUSEMOTION;
            mouseEvent.motion.x = command->intArg;
            mouseEvent.motion.y = command->intArg2;
            mouseEvent.motion.xrel = 0;
            mouseEvent.motion.yrel = 0;
            mouseEvent.motion.state = 0;
            SDL_PushEvent(&mouseEvent);
            break;
          }
          case TestInputReader::CommandType::MOUSE_CLICK: {
            // Determine button
            Uint8 button = SDL_BUTTON_LEFT;
            if (command->stringArg == "right") {
              button = SDL_BUTTON_RIGHT;
            } else if (command->stringArg == "middle") {
              button = SDL_BUTTON_MIDDLE;
            }

            // Mouse down event
            SDL_Event mouseDownEvent;
            mouseDownEvent.type = SDL_MOUSEBUTTONDOWN;
            mouseDownEvent.button.button = button;
            mouseDownEvent.button.clicks = 1;
            // Get current mouse position
            int mouseX, mouseY;
            SDL_GetMouseState(&mouseX, &mouseY);
            mouseDownEvent.button.x = mouseX;
            mouseDownEvent.button.y = mouseY;
            SDL_PushEvent(&mouseDownEvent);

            // Mouse up event (immediately after)
            SDL_Event mouseUpEvent;
            mouseUpEvent.type = SDL_MOUSEBUTTONUP;
            mouseUpEvent.button.button = button;
            mouseUpEvent.button.clicks = 1;
            mouseUpEvent.button.x = mouseX;
            mouseUpEvent.button.y = mouseY;
            SDL_PushEvent(&mouseUpEvent);
            break;
          }
          default:
            break;
        }
      }

      // Periodic screenshot capture
      Config::TestMode::testConfig.frameCount++;
      if (Config::TestMode::testConfig.frameCount %
              Config::TestMode::testConfig.screenshotInterval ==
          0) {
        renderSystem.captureScreenshot(
            Config::TestMode::testConfig.screenshotPath);
      }
    }
    // Poll window events
    window.pollEvents();

    // Process network events (embedded mode ticks both server and client)
    if (gameSession) {
      gameSession->tick();
    } else {
      client.run();
    }

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

  // Set up objective event handlers (InteractInputEvent + ObjectiveState
  // packets) in ClientPrediction (shared with WASM client)
  clientPrediction.setupObjectiveEventHandlers();

  // Run the game loop
  gameLoop.run();

  client.send("Window closed, disconnecting from Gambit Server!");
  client.disconnect();

  return EXIT_SUCCESS;
}
