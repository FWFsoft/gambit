#include "GameSession.h"

#include <cassert>

#include "CollisionSystem.h"
#include "Logger.h"
#include "MapSelectionState.h"
#include "NetworkClient.h"
#include "NetworkServer.h"
#include "ServerGameState.h"
#include "TiledMap.h"
#include "WorldConfig.h"
#include "transport/InMemoryServerTransport.h"
#include "transport/InMemoryTransport.h"

GameSession::GameSession() = default;

GameSession::~GameSession() {
  // Ensure proper shutdown order
  serverGameState.reset();
  server.reset();
  client.reset();
  collisionSystem.reset();
  map.reset();
  channel.reset();
}

std::unique_ptr<GameSession> GameSession::create() {
  auto session = std::unique_ptr<GameSession>(new GameSession());

  Logger::info("GameSession: Creating embedded server mode");

  // Create shared communication channel
  session->channel = createInMemoryChannel();

  // Load the map (server needs it for game state)
  session->map = std::make_unique<TiledMap>();
  std::string mapPath = MapSelectionState::instance().getSelectedMapPath();
  if (!session->map->load(mapPath)) {
    Logger::error("GameSession: Failed to load map: " + mapPath);
    return nullptr;
  }

  // Create collision system
  session->collisionSystem =
      std::make_unique<CollisionSystem>(session->map->getCollisionShapes());
  Logger::info("GameSession: Collision system initialized with " +
               std::to_string(session->map->getCollisionShapes().size()) +
               " shapes");

  // Create server with in-memory transport
  auto serverTransport =
      std::make_unique<InMemoryServerTransport>(session->channel);
  session->server =
      std::make_unique<NetworkServer>(std::move(serverTransport));

  // Initialize server (no actual port binding in embedded mode)
  if (!session->server->initialize("embedded", 0)) {
    Logger::error("GameSession: Failed to initialize server");
    return nullptr;
  }

  // Create world config for server game state
  WorldConfig world(session->map->getWorldWidth(),
                    session->map->getWorldHeight(),
                    session->collisionSystem.get(), session->map.get());

  // Create server game state
  session->serverGameState =
      std::make_unique<ServerGameState>(session->server.get(), world);

  // Create client with in-memory transport
  auto clientTransport =
      std::make_unique<InMemoryTransport>(session->channel);
  session->client = std::make_unique<NetworkClient>(std::move(clientTransport));

  // Connect client to "server" (immediate in embedded mode)
  if (!session->client->connect("embedded", 0)) {
    Logger::error("GameSession: Failed to connect client");
    return nullptr;
  }

  Logger::info("GameSession: Embedded server mode ready");
  return session;
}

void GameSession::tick() {
  // Poll server to process client messages and generate responses
  if (server) {
    server->poll();
  }

  // Poll client to receive server messages
  if (client) {
    client->run();
  }
}
