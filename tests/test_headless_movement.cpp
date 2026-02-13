// Integration test for headless client movement verification
// Demonstrates using InputScript and EventCapture for automated testing

#include <cmath>
#include <memory>
#include <thread>

#include "AnimationSystem.h"
#include "Camera.h"
#include "CharacterSelectionState.h"
#include "ClientPrediction.h"
#include "CollisionSystem.h"
#include "CombatSystem.h"
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
#include "test_utils.h"

// Helper function to run headless client for N frames
void runHeadlessClientForFrames(NetworkClient& client,
                                ClientPrediction& clientPrediction,
                                MockWindow& window, uint64_t maxFrames) {
  GameLoop gameLoop;
  uint64_t currentFrame = 0;

  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
    currentFrame = e.frameNumber;
    window.setFrameNumber(currentFrame);
    window.pollEvents();
    client.run();

    if (currentFrame >= maxFrames) {
      gameLoop.stop();
    }
  });

  gameLoop.run();
}

TEST(HeadlessMovement_RightMovement) {
  Logger::init();
  resetEventBus();

  // Load items
  ItemRegistry::instance().loadFromCSV("assets/items.csv");

  // Connect to server
  NetworkClient client(std::make_unique<ENetTransport>());
  assert(
      client.connect(Config::Network::SERVER_ADDRESS, Config::Network::PORT));

  // Initialize game state
  GameStateManager::instance().transitionTo(GameState::Playing);

  MockWindow window("Test Client", Config::Screen::WIDTH,
                    Config::Screen::HEIGHT);

  TiledMap map;
  assert(map.load("assets/maps/test_map.tmx"));

  CollisionSystem collisionSystem(map.getCollisionShapes());
  Camera camera(Config::Screen::WIDTH, Config::Screen::HEIGHT);
  camera.setWorldBounds(map.getWorldWidth(), map.getWorldHeight());

  uint32_t localPlayerId = (uint32_t)(uintptr_t)&client;
  WorldConfig world(map.getWorldWidth(), map.getWorldHeight(),
                    &collisionSystem);
  ClientPrediction clientPrediction(&client, localPlayerId, world);

  // Create input script: Move right for 30 frames
  InputScript inputScript;
  inputScript.addMove(10, 30, false, true, false, false);  // Move right
  window.setInputScript(&inputScript);

  InputSystem inputSystem(&clientPrediction, nullptr, nullptr);
  AnimationSystem animationSystem;
  MusicSystem musicSystem(&clientPrediction, &map);
  RemotePlayerInterpolation remoteInterpolation(localPlayerId,
                                                &animationSystem);
  EnemyInterpolation enemyInterpolation(&animationSystem);
  CombatSystem combatSystem(&client, &clientPrediction, &enemyInterpolation);
  HeadlessRenderSystem renderSystem;
  EffectTracker effectTracker;
  HeadlessUISystem uiSystem;

  // Capture events
  EventCapture<LocalInputEvent> inputEvents;
  EventCapture<NetworkPacketReceivedEvent> networkEvents;

  // Record starting position
  float startX = 0.0f;
  float startY = 0.0f;
  bool positionCaptured = false;

  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
    if (e.frameNumber == 5 && !positionCaptured) {
      const Player& player = clientPrediction.getLocalPlayer();
      startX = player.x;
      startY = player.y;
      positionCaptured = true;
    }
  });

  // Run for 50 frames (10 before movement + 30 during + 10 after)
  runHeadlessClientForFrames(client, clientPrediction, window, 50);

  // Get final position
  const Player& player = clientPrediction.getLocalPlayer();
  float endX = player.x;
  float endY = player.y;

  // Verify movement occurred
  assert(positionCaptured && "Should have captured starting position");

  // Player should have moved to the right (positive X direction)
  float deltaX = endX - startX;
  assert(deltaX > 0.0f && "Player should have moved right (positive X)");

  // Verify significant movement occurred
  float distance =
      std::sqrt(deltaX * deltaX + (endY - startY) * (endY - startY));
  assert(distance > 10.0f && "Player should have moved a reasonable distance");

  // Verify we captured input events during movement
  size_t rightInputs = inputEvents.countMatching(
      [](const LocalInputEvent& e) { return e.moveRight; });
  assert(rightInputs >= 20 &&
         "Should have captured at least 20 rightward input events");

  // Verify we received network state updates
  networkEvents.assertAtLeast(10);

  client.disconnect();
  Logger::info("HeadlessMovement_RightMovement: PASSED");
  Logger::info("  Start position: (" + std::to_string(startX) + ", " +
               std::to_string(startY) + ")");
  Logger::info("  End position: (" + std::to_string(endX) + ", " +
               std::to_string(endY) + ")");
  Logger::info("  Delta X: " + std::to_string(deltaX));
  Logger::info("  Right inputs captured: " + std::to_string(rightInputs));
}

TEST(HeadlessMovement_DiagonalMovement) {
  Logger::init();
  resetEventBus();

  ItemRegistry::instance().loadFromCSV("assets/items.csv");

  NetworkClient client(std::make_unique<ENetTransport>());
  assert(
      client.connect(Config::Network::SERVER_ADDRESS, Config::Network::PORT));

  GameStateManager::instance().transitionTo(GameState::Playing);

  MockWindow window("Test Client", Config::Screen::WIDTH,
                    Config::Screen::HEIGHT);

  TiledMap map;
  assert(map.load("assets/maps/test_map.tmx"));

  CollisionSystem collisionSystem(map.getCollisionShapes());
  Camera camera(Config::Screen::WIDTH, Config::Screen::HEIGHT);
  camera.setWorldBounds(map.getWorldWidth(), map.getWorldHeight());

  uint32_t localPlayerId = (uint32_t)(uintptr_t)&client;
  WorldConfig world(map.getWorldWidth(), map.getWorldHeight(),
                    &collisionSystem);
  ClientPrediction clientPrediction(&client, localPlayerId, world);

  // Create input script: Move diagonally (up-right) for 30 frames
  InputScript inputScript;
  inputScript.addMove(10, 30, false, true, true, false);  // Up-right
  window.setInputScript(&inputScript);

  InputSystem inputSystem(&clientPrediction, nullptr, nullptr);
  AnimationSystem animationSystem;
  MusicSystem musicSystem(&clientPrediction, &map);
  RemotePlayerInterpolation remoteInterpolation(localPlayerId,
                                                &animationSystem);
  EnemyInterpolation enemyInterpolation(&animationSystem);
  CombatSystem combatSystem(&client, &clientPrediction, &enemyInterpolation);
  HeadlessRenderSystem renderSystem;
  EffectTracker effectTracker;
  HeadlessUISystem uiSystem;

  // Capture events
  EventCapture<LocalInputEvent> inputEvents;

  // Record starting position
  float startX = 0.0f;
  float startY = 0.0f;
  bool positionCaptured = false;

  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
    if (e.frameNumber == 5 && !positionCaptured) {
      const Player& player = clientPrediction.getLocalPlayer();
      startX = player.x;
      startY = player.y;
      positionCaptured = true;
    }
  });

  // Run for 50 frames
  runHeadlessClientForFrames(client, clientPrediction, window, 50);

  // Get final position
  const Player& player = clientPrediction.getLocalPlayer();
  float endX = player.x;
  float endY = player.y;

  // Verify diagonal movement
  assert(positionCaptured && "Should have captured starting position");

  float deltaX = endX - startX;
  float deltaY = endY - startY;

  // Both X and Y should have changed (diagonal movement)
  assert(deltaX > 0.0f && "Player should have moved right");
  assert(deltaY < 0.0f && "Player should have moved up (negative Y)");

  // Verify magnitude of movement is reasonable
  float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
  assert(distance > 10.0f && "Player should have moved a reasonable distance");

  // Verify we captured both right and up inputs
  size_t rightInputs = inputEvents.countMatching(
      [](const LocalInputEvent& e) { return e.moveRight; });
  size_t upInputs = inputEvents.countMatching(
      [](const LocalInputEvent& e) { return e.moveUp; });

  assert(rightInputs >= 20 && "Should have captured right inputs");
  assert(upInputs >= 20 && "Should have captured up inputs");

  client.disconnect();
  Logger::info("HeadlessMovement_DiagonalMovement: PASSED");
  Logger::info("  Start position: (" + std::to_string(startX) + ", " +
               std::to_string(startY) + ")");
  Logger::info("  End position: (" + std::to_string(endX) + ", " +
               std::to_string(endY) + ")");
  Logger::info("  Movement distance: " + std::to_string(distance));
}

int main() {
  Logger::init();
  Logger::info("=== Running Headless Movement Tests ===");

  // Note: These tests require a running server on localhost:1234
  // The tests will fail if no server is available

  test_HeadlessMovement_RightMovement();
  test_HeadlessMovement_DiagonalMovement();

  Logger::info("=== All Headless Movement Tests Passed ===");
  return 0;
}
