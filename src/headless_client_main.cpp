#include <cstdlib>
#include <cstring>
#include <memory>

#include "AnimationSystem.h"
#include "Camera.h"
#include "CharacterRegistry.h"
#include "CharacterSelectionState.h"
#include "ClientPrediction.h"
#include "CollisionSystem.h"
#include "CombatSystem.h"
#include "DamageNumberSystem.h"
#include "EffectTracker.h"
#include "EnemyInterpolation.h"
#include "EventBus.h"
#include "GameLoop.h"
#include "GameStateManager.h"
#include "HeadlessRenderSystem.h"
#include "HeadlessUISystem.h"
#include "InputScript.h"
#include "InputSystem.h"
#include "ItemRegistry.h"
#include "Logger.h"
#include "MockWindow.h"
#include "MusicSystem.h"
#include "NetworkClient.h"
#include "RemotePlayerInterpolation.h"
#include "transport/ENetTransport.h"
#include "TiledMap.h"
#include "WorldConfig.h"
#include "config/NetworkConfig.h"
#include "config/ScreenConfig.h"
#include "config/TimingConfig.h"
#include "test_utils.h"

void printUsage(const char* programName) {
  std::cout << "Usage: " << programName << " [OPTIONS]\n\n"
            << "Options:\n"
            << "  --frames N        Run for exactly N frames then exit "
               "(default: unlimited)\n"
            << "  --log-events      Enable event logging to stdout\n"
            << "  --verbose         Enable verbose event logging\n"
            << "  --help            Show this help message\n"
            << "\n"
            << "Headless Client Mode - runs game logic without graphics\n"
            << "Useful for automated testing and verification\n";
}

int main(int argc, char* argv[]) {
  // Parse command line arguments
  uint64_t maxFrames = 0;  // 0 = unlimited
  bool logEvents = false;
  bool verbose = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
      maxFrames = std::stoull(argv[++i]);
    } else if (strcmp(argv[i], "--log-events") == 0) {
      logEvents = true;
    } else if (strcmp(argv[i], "--verbose") == 0) {
      verbose = true;
      logEvents = true;  // verbose implies log-events
    } else if (strcmp(argv[i], "--help") == 0) {
      printUsage(argv[0]);
      return EXIT_SUCCESS;
    } else {
      std::cerr << "Unknown option: " << argv[i] << "\n";
      printUsage(argv[0]);
      return EXIT_FAILURE;
    }
  }

  Logger::init();
  Logger::info("=== Headless Client Starting ===");
  Logger::info("Max frames: " + (maxFrames == 0 ? std::string("unlimited")
                                                : std::to_string(maxFrames)));
  Logger::info("Event logging: " +
               std::string(logEvents ? "enabled" : "disabled"));

  // Initialize event logger if requested
  EventLogger* eventLogger = nullptr;
  if (logEvents) {
    eventLogger = new EventLogger(true);
    eventLogger->setVerbose(verbose);
  }

  // Load item definitions
  if (!ItemRegistry::instance().loadFromCSV("assets/items.csv")) {
    Logger::error("Failed to load items.csv - inventory will be empty");
  }

  auto transport = std::make_unique<ENetTransport>();
  NetworkClient client(std::move(transport));
  if (!client.connect(Config::Network::SERVER_ADDRESS, Config::Network::PORT)) {
    return EXIT_FAILURE;
  }

  // Start in title screen
  GameStateManager::instance().transitionTo(GameState::TitleScreen);

  // Use MockWindow instead of Window
  MockWindow window("Headless Gambit Client", Config::Screen::WIDTH,
                    Config::Screen::HEIGHT);
  window.initImGui();
  GameLoop gameLoop;

  TiledMap map;
  assert(map.load("assets/maps/test_map.tmx") && "Failed to load required map");

  CollisionSystem collisionSystem(map.getCollisionShapes());
  Logger::info("Client collision system initialized");

  Camera camera(Config::Screen::WIDTH, Config::Screen::HEIGHT);
  camera.setWorldBounds(map.getWorldWidth(), map.getWorldHeight());

  uint32_t localPlayerId = (uint32_t)(uintptr_t)&client;
  WorldConfig world(map.getWorldWidth(), map.getWorldHeight(),
                    &collisionSystem);
  ClientPrediction clientPrediction(&client, localPlayerId, world);

  // Create input script for programmatic input
  InputScript inputScript;
  window.setInputScript(&inputScript);

  // Note: Headless mode doesn't use CollisionDebugRenderer or
  // MusicZoneDebugRenderer
  InputSystem inputSystem(&clientPrediction, nullptr, nullptr);

  // Initialize animation system
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

  // Use HeadlessRenderSystem instead of RenderSystem
  HeadlessRenderSystem renderSystem;

  // Create effect tracker
  EffectTracker effectTracker;

  // Use HeadlessUISystem instead of UISystem
  HeadlessUISystem uiSystem;

  // Load local player animations (no texture needed in headless mode)
  Player& localPlayer = clientPrediction.getLocalPlayerMutable();
  // Skip texture loading: AnimationAssetLoader::loadPlayerAnimations(...)
  animationSystem.registerEntity(&localPlayer);

  // Apply character selection
  if (CharacterSelectionState::instance().hasSelection()) {
    uint32_t selectedId =
        CharacterSelectionState::instance().getSelectedCharacterId();
    const CharacterDefinition* character =
        CharacterRegistry::instance().getCharacter(selectedId);

    if (character) {
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
    localPlayer.r = 255;
    localPlayer.g = 255;
    localPlayer.b = 255;
    localPlayer.characterId = 0;
    Logger::info("No character selected, using default appearance");
  }

  // Add test items to inventory
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

  // Track frame count for --frames limit
  uint64_t currentFrame = 0;

  // Subscribe to UpdateEvent for network processing and frame tracking
  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
    currentFrame = e.frameNumber;

    // Update window's frame number for input scripting
    window.setFrameNumber(currentFrame);

    // Poll window events (processes scripted input)
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

    // Stop if window is closed or max frames reached
    if (!window.isOpen() || (maxFrames > 0 && currentFrame >= maxFrames)) {
      if (maxFrames > 0 && currentFrame >= maxFrames) {
        Logger::info("=== Reached max frames (" + std::to_string(maxFrames) +
                     ") - stopping ===");
      }
      gameLoop.stop();
    }

    // Log progress periodically
    if (e.frameNumber % Config::Timing::LOG_FRAME_INTERVAL == 0) {
      Logger::debug("Frame: " + std::to_string(e.frameNumber) +
                    " (deltaTime: " + std::to_string(e.deltaTime) + "ms)");
    }
  });

  // Run the game loop
  gameLoop.run();

  Logger::info("=== Headless Client Shutting Down ===");
  Logger::info("Total frames executed: " + std::to_string(currentFrame));

  client.send("Headless client disconnecting");
  client.disconnect();

  // Cleanup
  delete eventLogger;

  return EXIT_SUCCESS;
}
