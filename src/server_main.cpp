#include <csignal>

#include "EventBus.h"
#include "GameLoop.h"
#include "Logger.h"
#include "NetworkServer.h"
#include "ServerGameState.h"

volatile bool serverRunning = true;

void signalHandler(int signum) { serverRunning = false; }

int main() {
  Logger::init();
  signal(SIGINT, signalHandler);

  NetworkServer server("0.0.0.0", 1234);
  if (!server.initialize()) {
    return EXIT_FAILURE;
  }

  GameLoop gameLoop;
  ServerGameState gameState(&server);

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
