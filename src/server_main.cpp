#include <cassert>
#include <csignal>

#include "CollisionSystem.h"
#include "EventBus.h"
#include "GameLoop.h"
#include "ItemRegistry.h"
#include "Logger.h"
#include "NetworkServer.h"
#include "ServerGameState.h"
#include "TiledMap.h"
#include "WorldConfig.h"
#include "config/NetworkConfig.h"

volatile bool serverRunning = true;

void signalHandler(int signum) { serverRunning = false; }

int main() {
  Logger::init();
  signal(SIGINT, signalHandler);

  // Load item definitions
  if (!ItemRegistry::instance().loadFromCSV("assets/items.csv")) {
    Logger::error("Failed to load items.csv - inventory will be empty");
  }

  NetworkServer server(Config::Network::SERVER_BIND_ADDRESS,
                       Config::Network::PORT);
  if (!server.initialize()) {
    return EXIT_FAILURE;
  }

  TiledMap map;
  assert(map.load("assets/test_map.tmx") && "Failed to load required map");

  CollisionSystem collisionSystem(map.getCollisionShapes());
  Logger::info("Collision system initialized with " +
               std::to_string(map.getCollisionShapes().size()) + " shapes");

  GameLoop gameLoop;
  WorldConfig world(map.getWorldWidth(), map.getWorldHeight(), &collisionSystem,
                    &map);
  ServerGameState gameState(&server, world);

  // Subscribe to UpdateEvent to process network events
  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
    server.poll();  // Process network events

    if (!serverRunning) {
      gameLoop.stop();
    }
  });

  // Run the game loop (which will drive ServerGameState updates)
  gameLoop.run();

  Logger::info("Server shutting down");
  return EXIT_SUCCESS;
}
