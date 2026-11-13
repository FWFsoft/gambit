#include <cstdio>
#include <cstdlib>
#include <memory>

#include "AnimationAssetLoader.h"
#include "AnimationSystem.h"
#include "ArenaClient.h"
#include "ArenaView.h"
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
  //   --embedded          run the server inside this process (solo play)
  //   --connect <host>    server address (default 127.0.0.1)
  //   --map <path.tmx>    map to load (must match the server's map)
  //   --test-mode         scripted input + screenshots (see visual-test skill)
  //   --record <n>        with --test-mode: save every nth frame for clips
  bool embeddedMode = false;
  std::string serverAddress = Config::Network::SERVER_ADDRESS;
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--test-mode") {
      Config::TestMode::testConfig.enabled = true;
      Logger::info("Test mode enabled");
    } else if (arg == "--embedded") {
      embeddedMode = true;
      Logger::info("Embedded server mode enabled");
    } else if (arg == "--record" && i + 1 < argc) {
      Config::TestMode::testConfig.recordInterval = std::atoi(argv[++i]);
    } else if (arg == "--connect" && i + 1 < argc) {
      serverAddress = argv[++i];
    } else if (arg == "--map" && i + 1 < argc) {
      MapSelectionState::instance().setSelectedMap(argv[++i]);
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
    Logger::info("Connecting to " + serverAddress + ":" +
                 std::to_string(Config::Network::PORT));
    if (!networkClient.connect(serverAddress, Config::Network::PORT)) {
      Logger::error("Could not reach a server at " + serverAddress +
                    ". Start ./build/Server first, or run with --embedded.");
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
                    &effectTracker, &remoteInterpolation, &enemyInterpolation,
                    &camera);

  // The match as the client sees it, and the view that draws it.
  ArenaClient arenaClient;
  ArenaView arenaView(&arenaClient, &clientPrediction, &effectTracker, &camera);
  uiSystem.setArenaView(&arenaView);
  // The look: dark world, light pools, cast shadows, bloom and grade.
  arenaView.setPresentation(renderSystem.getPresentation());
  uiSystem.setPresentation(renderSystem.getPresentation());
  renderSystem.setLegacyEntityRendering(false);

  // Load local player animations
  Player& localPlayer = clientPrediction.getLocalPlayerMutable();
  AnimationAssetLoader::loadPlayerAnimations(
      *localPlayer.getAnimationController(), "assets/player_animated.png");
  animationSystem.registerEntity(&localPlayer);

  // Initialize test input reader if test mode enabled
  std::unique_ptr<TestInputReader> testInputReader;
  if (Config::TestMode::testConfig.enabled) {
    testInputReader = std::make_unique<TestInputReader>(
        Config::TestMode::testConfig.inputCommandPath);
  }

  // ImGui's SDL backend ignores events that do not belong to a window, so
  // scripted events must carry ours.
  const Uint32 testWindowId = SDL_GetWindowID(window.getWindow());

  // Where scripted test input last put the mouse.
  int testMouseX = Config::Screen::WIDTH / 2;
  int testMouseY = Config::Screen::HEIGHT / 2;

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
              SDL_zero(keyEvent);
              keyEvent.key.windowID = testWindowId;
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
              SDL_zero(keyEvent);
              keyEvent.key.windowID = testWindowId;
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
            testMouseX = command->intArg;
            testMouseY = command->intArg2;
            // Move the real pointer too: ImGui polls the global mouse
            // position every frame and would otherwise override the
            // scripted one, so UI hover and clicks would miss.
            SDL_WarpMouseInWindow(window.getWindow(), testMouseX, testMouseY);
            SDL_Event mouseEvent;
            SDL_zero(mouseEvent);
            mouseEvent.motion.windowID = testWindowId;
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
            SDL_zero(mouseDownEvent);
            mouseDownEvent.button.windowID = testWindowId;
            mouseDownEvent.type = SDL_MOUSEBUTTONDOWN;
            mouseDownEvent.button.button = button;
            mouseDownEvent.button.clicks = 1;
            // Scripted clicks land where the script last moved the mouse
            // (there is no real pointer under a virtual display).
            int mouseX = testMouseX, mouseY = testMouseY;
            mouseDownEvent.button.x = mouseX;
            mouseDownEvent.button.y = mouseY;
            SDL_PushEvent(&mouseDownEvent);

            // Mouse up event (immediately after)
            SDL_Event mouseUpEvent;
            SDL_zero(mouseUpEvent);
            mouseUpEvent.button.windowID = testWindowId;
            mouseUpEvent.type = SDL_MOUSEBUTTONUP;
            mouseUpEvent.button.button = button;
            mouseUpEvent.button.clicks = 1;
            mouseUpEvent.button.x = mouseX;
            mouseUpEvent.button.y = mouseY;
            SDL_PushEvent(&mouseUpEvent);
            break;
          }
          case TestInputReader::CommandType::MOUSE_DOWN:
          case TestInputReader::CommandType::MOUSE_UP: {
            bool down =
                command->type == TestInputReader::CommandType::MOUSE_DOWN;
            SDL_Event buttonEvent;
            SDL_zero(buttonEvent);
            buttonEvent.button.windowID = testWindowId;
            buttonEvent.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            buttonEvent.button.button =
                command->stringArg == "right" ? SDL_BUTTON_RIGHT
                                              : SDL_BUTTON_LEFT;
            buttonEvent.button.clicks = 1;
            buttonEvent.button.x = testMouseX;
            buttonEvent.button.y = testMouseY;
            SDL_PushEvent(&buttonEvent);
            break;
          }
          default:
            break;
        }
      }

      // Periodic screenshot capture
      Config::TestMode::testConfig.frameCount++;
      auto& testConfig = Config::TestMode::testConfig;
      if (testConfig.recordInterval > 0 &&
          testConfig.frameCount % testConfig.recordInterval == 0 &&
          GameStateManager::instance().getCurrentState() ==
              GameState::Playing) {
        char name[64];
        std::snprintf(name, sizeof(name), "test_output/rec_%05d.png",
                      ++testConfig.recordedFrames);
        renderSystem.captureScreenshot(name);
      }
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
          // Arena pickups (infusions, scrap, medkits) are collected by the
          // server when you walk over them.
          if (worldItem.itemId >= Arena::PickupId::InfusionBase) continue;
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
