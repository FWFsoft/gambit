#include "Camera.h"
#include "ClientPrediction.h"
#include "EventBus.h"
#include "GameLoop.h"
#include "InputSystem.h"
#include "Logger.h"
#include "NetworkClient.h"
#include "RemotePlayerInterpolation.h"
#include "RenderSystem.h"
#include "TileRenderer.h"
#include "TiledMap.h"
#include "Window.h"

int main() {
  Logger::init();

  NetworkClient client;
  if (!client.initialize()) {
    return EXIT_FAILURE;
  }

  if (!client.connect("127.0.0.1", 1234)) {
    return EXIT_FAILURE;
  }

  Window window("Gambit Client", 800, 600);
  GameLoop gameLoop;
  InputSystem inputSystem;

  TiledMap map;
  assert(map.load("assets/test_map.tmx") && "Failed to load required map");

  Camera camera(800, 600);
  camera.setWorldBounds(map.getWorldWidth(), map.getWorldHeight());

  TileRenderer tileRenderer(window.getRenderer(), &camera);

  uint32_t localPlayerId = (uint32_t)(uintptr_t)&client;
  ClientPrediction clientPrediction(&client, localPlayerId, map.getWorldWidth(),
                                    map.getWorldHeight());
  RemotePlayerInterpolation remoteInterpolation(localPlayerId);
  RenderSystem renderSystem(&window, &clientPrediction, &remoteInterpolation,
                            &camera, &map, &tileRenderer);

  // Subscribe to UpdateEvent for network processing
  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
    // Poll window events
    window.pollEvents();

    // Process network events
    client.run();

    // Stop the game loop if window is closed
    if (!window.isOpen()) {
      gameLoop.stop();
    }

    // Log every 60 frames (once per second) to verify 60 FPS
    if (e.frameNumber % 60 == 0) {
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
