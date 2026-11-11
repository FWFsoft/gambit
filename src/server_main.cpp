#include <cassert>
#include <csignal>
#include <memory>
#include <string>

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
#include "transport/ENetServerTransport.h"

volatile bool serverRunning = true;

void signalHandler(int signum) { serverRunning = false; }

int main(int argc, char* argv[]) {
  Logger::init();

  //   --map <path.tmx>    map to host (clients must load the same one)
  std::string mapPath = "assets/maps/arena.tmx";
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--map" && i + 1 < argc) mapPath = argv[++i];
  }
  signal(SIGINT, signalHandler);

  // Load item definitions
  if (!ItemRegistry::instance().loadFromCSV("assets/items.csv")) {
    Logger::error("Failed to load items.csv - inventory will be empty");
  }

  auto transport = std::make_unique<ENetServerTransport>();
  NetworkServer server(std::move(transport));
  if (!server.initialize(Config::Network::SERVER_BIND_ADDRESS,
                         Config::Network::PORT)) {
    return EXIT_FAILURE;
  }

  TiledMap map;
  if (!map.load(mapPath)) {
    Logger::error("Failed to load map: " + mapPath);
    return EXIT_FAILURE;
  }
  Logger::info("Hosting map " + mapPath);

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
