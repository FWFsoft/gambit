#pragma once

#include <memory>

#include "InMemoryChannel.h"

class NetworkClient;
class NetworkServer;
class ServerGameState;
class TiledMap;
class CollisionSystem;
struct WorldConfig;

// GameSession: Orchestrator for embedded server mode
// Creates both client and server in the same process, communicating
// via in-memory message queues. Enables zero-network-latency local play.
//
// Usage:
//   auto session = GameSession::create();
//   NetworkClient* client = session->getClient();
//   // Use client as normal - server runs in same process
//
// The session owns the server-side state and transport. The client
// transport communicates with the server via InMemoryChannel.

class GameSession {
 public:
  // Create a fully-wired embedded game session
  // Loads the default map and initializes server game state
  static std::unique_ptr<GameSession> create();

  ~GameSession();

  // Access the client for game logic
  NetworkClient* getClient() { return client.get(); }

  // Access server state for debugging/testing
  ServerGameState* getServerState() { return serverGameState.get(); }

  // Tick both server and client (call from game loop)
  // This processes messages in both directions
  void tick();

 private:
  GameSession();

  std::shared_ptr<InMemoryChannel> channel;
  std::unique_ptr<NetworkServer> server;
  std::unique_ptr<NetworkClient> client;
  std::unique_ptr<ServerGameState> serverGameState;
  std::unique_ptr<TiledMap> map;
  std::unique_ptr<CollisionSystem> collisionSystem;
};
