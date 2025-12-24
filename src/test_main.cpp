#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#include "ClientPrediction.h"
#include "EventBus.h"
#include "GameLoop.h"
#include "InputSystem.h"
#include "Logger.h"
#include "NetworkClient.h"
#include "NetworkProtocol.h"
#include "Player.h"
#include "RemotePlayerInterpolation.h"

int testsRun = 0;
int testsPassed = 0;

#define TEST(name)                                   \
  void test_##name();                                \
  void run_##name() {                                \
    testsRun++;                                      \
    std::cout << "Running test: " << #name << "..."; \
    test_##name();                                   \
    testsPassed++;                                   \
    std::cout << " PASSED\n";                        \
  }                                                  \
  void test_##name()

// Helper function for float comparison
bool floatEqual(float a, float b, float epsilon = 0.001f) {
  return std::abs(a - b) < epsilon;
}

// Helper to clear EventBus and reset state
void resetEventBus() { EventBus::instance().clear(); }

// Helper to publish a key down event and update
LocalInputEvent publishKeyAndUpdate(InputSystem& inputSystem, int sdlKey,
                                    bool isKeyDown) {
  LocalInputEvent capturedEvent;
  bool captured = false;

  EventBus::instance().subscribe<LocalInputEvent>(
      [&](const LocalInputEvent& e) {
        if (!captured) {
          capturedEvent = e;
          captured = true;
        }
      });

  if (isKeyDown) {
    EventBus::instance().publish(KeyDownEvent{sdlKey});
  } else {
    EventBus::instance().publish(KeyUpEvent{sdlKey});
  }

  EventBus::instance().publish(UpdateEvent{16.67f, 1});
  return capturedEvent;
}

// Helper to create and publish StateUpdatePacket
void publishStateUpdate(uint32_t serverTick,
                        const std::vector<PlayerState>& players) {
  StateUpdatePacket packet;
  packet.serverTick = serverTick;
  packet.players = players;
  auto serialized = serialize(packet);
  EventBus::instance().publish(NetworkPacketReceivedEvent{
      nullptr, serialized.data(), serialized.size()});
}

// Helper to create and publish PlayerJoinedPacket
void publishPlayerJoined(uint32_t playerId, uint8_t r, uint8_t g, uint8_t b) {
  PlayerJoinedPacket packet;
  packet.playerId = playerId;
  packet.r = r;
  packet.g = g;
  packet.b = b;
  auto serialized = serialize(packet);
  EventBus::instance().publish(NetworkPacketReceivedEvent{
      nullptr, serialized.data(), serialized.size()});
}

// Test ClientInput serialization round-trip
TEST(ClientInputSerialization) {
  ClientInputPacket original;
  original.inputSequence = 42;
  original.moveLeft = true;
  original.moveRight = false;
  original.moveUp = true;
  original.moveDown = false;

  auto serialized = serialize(original);
  assert(serialized.size() == 6);

  auto deserialized =
      deserializeClientInput(serialized.data(), serialized.size());
  assert(deserialized.inputSequence == 42);
  assert(deserialized.moveLeft == true);
  assert(deserialized.moveRight == false);
  assert(deserialized.moveUp == true);
  assert(deserialized.moveDown == false);
}

// Test StateUpdate serialization round-trip
TEST(StateUpdateSerialization) {
  StateUpdatePacket original;
  original.serverTick = 100;

  PlayerState player1;
  player1.playerId = 1;
  player1.x = 100.5f;
  player1.y = 200.5f;
  player1.vx = 10.0f;
  player1.vy = 20.0f;
  player1.health = 100.0f;
  player1.r = 255;
  player1.g = 0;
  player1.b = 0;
  player1.lastInputSequence = 5;

  PlayerState player2;
  player2.playerId = 2;
  player2.x = 300.5f;
  player2.y = 400.5f;
  player2.vx = -10.0f;
  player2.vy = -20.0f;
  player2.health = 75.0f;
  player2.r = 0;
  player2.g = 255;
  player2.b = 0;
  player2.lastInputSequence = 3;

  original.players.push_back(player1);
  original.players.push_back(player2);

  auto serialized = serialize(original);
  assert(serialized.size() ==
         7 + (2 * 31));  // 69 bytes for 2 players (1+4+2+62)

  auto deserialized =
      deserializeStateUpdate(serialized.data(), serialized.size());
  assert(deserialized.serverTick == 100);
  assert(deserialized.players.size() == 2);

  assert(deserialized.players[0].playerId == 1);
  assert(deserialized.players[0].x == 100.5f);
  assert(deserialized.players[0].y == 200.5f);
  assert(deserialized.players[0].health == 100.0f);
  assert(deserialized.players[0].r == 255);

  assert(deserialized.players[1].playerId == 2);
  assert(deserialized.players[1].x == 300.5f);
  assert(deserialized.players[1].health == 75.0f);
  assert(deserialized.players[1].g == 255);
}

// Test PlayerJoined serialization round-trip
TEST(PlayerJoinedSerialization) {
  PlayerJoinedPacket original;
  original.playerId = 123;
  original.r = 255;
  original.g = 128;
  original.b = 64;

  auto serialized = serialize(original);
  assert(serialized.size() == 8);

  auto deserialized =
      deserializePlayerJoined(serialized.data(), serialized.size());
  assert(deserialized.playerId == 123);
  assert(deserialized.r == 255);
  assert(deserialized.g == 128);
  assert(deserialized.b == 64);
}

// Test PlayerLeft serialization round-trip
TEST(PlayerLeftSerialization) {
  PlayerLeftPacket original;
  original.playerId = 456;

  auto serialized = serialize(original);
  assert(serialized.size() == 5);

  auto deserialized =
      deserializePlayerLeft(serialized.data(), serialized.size());
  assert(deserialized.playerId == 456);
}

// ============================================================================
// PHASE 1: EventBus Tests
// ============================================================================

TEST(EventBus_SingleSubscriberPublish) {
  EventBus::instance().clear();  // Clear any existing handlers
  int callCount = 0;

  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
    callCount++;
    assert(e.deltaTime == 16.67f);
    assert(e.frameNumber == 42);
  });

  UpdateEvent event{16.67f, 42};
  EventBus::instance().publish(event);

  assert(callCount == 1);
  EventBus::instance().clear();
}

TEST(EventBus_MultipleSubscribers) {
  EventBus::instance().clear();
  int call1 = 0, call2 = 0, call3 = 0;

  EventBus::instance().subscribe<UpdateEvent>(
      [&](const UpdateEvent&) { call1++; });
  EventBus::instance().subscribe<UpdateEvent>(
      [&](const UpdateEvent&) { call2++; });
  EventBus::instance().subscribe<UpdateEvent>(
      [&](const UpdateEvent&) { call3++; });

  UpdateEvent event{16.67f, 1};
  EventBus::instance().publish(event);

  assert(call1 == 1);
  assert(call2 == 1);
  assert(call3 == 1);
  EventBus::instance().clear();
}

TEST(EventBus_MultipleEventTypes) {
  EventBus::instance().clear();
  int updateCalls = 0, renderCalls = 0;

  EventBus::instance().subscribe<UpdateEvent>(
      [&](const UpdateEvent&) { updateCalls++; });
  EventBus::instance().subscribe<RenderEvent>(
      [&](const RenderEvent&) { renderCalls++; });

  EventBus::instance().publish(UpdateEvent{16.67f, 1});
  EventBus::instance().publish(RenderEvent{0.5f});

  assert(updateCalls == 1);
  assert(renderCalls == 1);
  EventBus::instance().clear();
}

TEST(EventBus_PublishWithNoSubscribers) {
  EventBus::instance().clear();
  // Should not crash
  EventBus::instance().publish(UpdateEvent{16.67f, 1});
}

TEST(EventBus_Clear) {
  EventBus::instance().clear();
  int callCount = 0;

  EventBus::instance().subscribe<UpdateEvent>(
      [&](const UpdateEvent&) { callCount++; });
  EventBus::instance().publish(UpdateEvent{16.67f, 1});
  assert(callCount == 1);

  EventBus::instance().clear();
  EventBus::instance().publish(UpdateEvent{16.67f, 2});
  assert(callCount == 1);  // Should not have increased
}

// ============================================================================
// PHASE 4: Player Movement Tests
// ============================================================================

TEST(Player_MoveRight) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  applyInput(player, false, true, false, false, 16.67f, 800.0f, 600.0f);

  float expectedSpeed = 200.0f * 0.707107f;
  assert(floatEqual(player.vx, expectedSpeed));
  assert(floatEqual(player.vy, -expectedSpeed));
  assert(player.x > 100.0f);
  assert(player.y < 100.0f);
}

TEST(Player_MoveLeft) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  applyInput(player, true, false, false, false, 16.67f, 800.0f, 600.0f);

  float expectedSpeed = 200.0f * 0.707107f;
  assert(floatEqual(player.vx, -expectedSpeed));
  assert(floatEqual(player.vy, expectedSpeed));
  assert(player.x < 100.0f);
  assert(player.y > 100.0f);
}

TEST(Player_MoveUp) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  applyInput(player, false, false, true, false, 16.67f, 800.0f, 600.0f);

  float expectedSpeed = 200.0f * 0.707107f;
  assert(floatEqual(player.vx, -expectedSpeed));
  assert(floatEqual(player.vy, -expectedSpeed));
  assert(player.x < 100.0f);
  assert(player.y < 100.0f);
}

TEST(Player_MoveDown) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  applyInput(player, false, false, false, true, 16.67f, 800.0f, 600.0f);

  float expectedSpeed = 200.0f * 0.707107f;
  assert(floatEqual(player.vx, expectedSpeed));
  assert(floatEqual(player.vy, expectedSpeed));
  assert(player.x > 100.0f);
  assert(player.y > 100.0f);
}

TEST(Player_DiagonalMovement) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  applyInput(player, false, true, false, true, 16.67f, 800.0f, 600.0f);

  assert(floatEqual(player.vx, 200.0f));
  assert(floatEqual(player.vy, 0.0f));
  assert(player.x > 100.0f);
  assert(floatEqual(player.y, 100.0f));
}

TEST(Player_Stationary) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  applyInput(player, false, false, false, false, 16.67f, 800.0f,
             600.0f);  // No movement

  assert(floatEqual(player.vx, 0.0f));
  assert(floatEqual(player.vy, 0.0f));
  assert(floatEqual(player.x, 100.0f));
  assert(floatEqual(player.y, 100.0f));
}

TEST(Player_BoundsClampingLeft) {
  Player player;
  player.x = 5.0f;
  player.y = 100.0f;

  applyInput(player, true, false, false, false, 16.67f, 800.0f,
             600.0f);  // Move left

  assert(player.x >= 0.0f);  // Should be clamped to 0
  assert(player.x <= 5.0f);  // Should not move right
}

TEST(Player_BoundsClampingRight) {
  Player player;
  player.x = 795.0f;
  player.y = 100.0f;

  applyInput(player, false, true, false, false, 16.67f, 800.0f,
             600.0f);  // Move right

  assert(player.x <= 800.0f);  // Should be clamped to 800
}

TEST(Player_BoundsClampingTop) {
  Player player;
  player.x = 100.0f;
  player.y = 5.0f;

  applyInput(player, false, false, true, false, 16.67f, 800.0f,
             600.0f);  // Move up

  assert(player.y >= 0.0f);  // Should be clamped to 0
}

TEST(Player_BoundsClampingBottom) {
  Player player;
  player.x = 100.0f;
  player.y = 595.0f;

  applyInput(player, false, false, false, true, 16.67f, 800.0f,
             600.0f);  // Move down

  assert(player.y <= 600.0f);  // Should be clamped to 600
}

TEST(Player_IsometricMapBounds_BottomRight) {
  Player player;
  player.x = 970.0f;
  player.y = 970.0f;

  applyInput(player, false, true, false, true, 16.67f, 976.0f, 976.0f);

  assert(player.x <= 976.0f);
  assert(player.y <= 976.0f);
}

TEST(Player_IsometricMapBounds_TopLeft) {
  Player player;
  player.x = 5.0f;
  player.y = 5.0f;

  applyInput(player, false, false, true, false, 16.67f, 976.0f, 976.0f);

  assert(player.x >= 0.0f);
  assert(player.y >= 0.0f);
}

TEST(Player_IsometricMapBounds_AllCorners) {
  float worldSize = 976.0f;

  Player topLeft;
  topLeft.x = 0.0f;
  topLeft.y = 0.0f;
  applyInput(topLeft, false, false, true, false, 100.0f, worldSize, worldSize);
  assert(topLeft.x >= 0.0f && topLeft.y >= 0.0f);

  Player topRight;
  topRight.x = worldSize;
  topRight.y = 0.0f;
  applyInput(topRight, false, true, false, false, 100.0f, worldSize, worldSize);
  assert(topRight.x <= worldSize && topRight.y >= 0.0f);

  Player bottomLeft;
  bottomLeft.x = 0.0f;
  bottomLeft.y = worldSize;
  applyInput(bottomLeft, true, false, false, false, 100.0f, worldSize,
             worldSize);
  assert(bottomLeft.x >= 0.0f && bottomLeft.y <= worldSize);

  Player bottomRight;
  bottomRight.x = worldSize;
  bottomRight.y = worldSize;
  applyInput(bottomRight, false, false, false, true, 100.0f, worldSize,
             worldSize);
  assert(bottomRight.x <= worldSize && bottomRight.y <= worldSize);
}

// ============================================================================
// PHASE 3: Network Protocol Stress Tests
// ============================================================================

TEST(NetworkProtocol_StateUpdate_ZeroPlayers) {
  StateUpdatePacket original;
  original.serverTick = 100;
  // No players

  auto serialized = serialize(original);
  assert(serialized.size() == 7);  // Header only (1+4+2)

  auto deserialized =
      deserializeStateUpdate(serialized.data(), serialized.size());
  assert(deserialized.serverTick == 100);
  assert(deserialized.players.size() == 0);
}

TEST(NetworkProtocol_StateUpdate_OnePlayer) {
  StateUpdatePacket original;
  original.serverTick = 100;

  PlayerState player;
  player.playerId = 1;
  player.x = 100.5f;
  player.y = 200.5f;
  player.vx = 10.0f;
  player.vy = 20.0f;
  player.health = 100.0f;
  player.r = 255;
  player.g = 0;
  player.b = 0;
  player.lastInputSequence = 0;
  original.players.push_back(player);

  auto serialized = serialize(original);
  assert(serialized.size() == 7 + 31);  // 38 bytes for 1 player (1+4+2+31)

  auto deserialized =
      deserializeStateUpdate(serialized.data(), serialized.size());
  assert(deserialized.players.size() == 1);
  assert(deserialized.players[0].playerId == 1);
}

TEST(NetworkProtocol_StateUpdate_100Players) {
  StateUpdatePacket original;
  original.serverTick = 1000;

  // Add 100 players
  for (uint32_t i = 0; i < 100; i++) {
    PlayerState player;
    player.playerId = i;
    player.x = static_cast<float>(i * 10);
    player.y = static_cast<float>(i * 5);
    player.vx = static_cast<float>(i);
    player.vy = static_cast<float>(i);
    player.health = 100.0f;
    player.r = static_cast<uint8_t>(i % 256);
    player.g = static_cast<uint8_t>((i * 2) % 256);
    player.b = static_cast<uint8_t>((i * 3) % 256);
    player.lastInputSequence = i;
    original.players.push_back(player);
  }

  auto serialized = serialize(original);
  size_t expectedSize = 7 + (100 * 31);  // 3107 bytes (1+4+2+3100)
  assert(serialized.size() == expectedSize);

  auto deserialized =
      deserializeStateUpdate(serialized.data(), serialized.size());
  assert(deserialized.serverTick == 1000);
  assert(deserialized.players.size() == 100);

  // Verify first and last player
  assert(deserialized.players[0].playerId == 0);
  assert(deserialized.players[99].playerId == 99);
  assert(floatEqual(deserialized.players[99].x, 990.0f));
}

TEST(NetworkProtocol_StateUpdate_1000Players) {
  StateUpdatePacket original;
  original.serverTick = 10000;

  // Add 1000 players
  for (uint32_t i = 0; i < 1000; i++) {
    PlayerState player;
    player.playerId = i;
    player.x = static_cast<float>(i);
    player.y = static_cast<float>(i);
    player.vx = 0.0f;
    player.vy = 0.0f;
    player.health = 100.0f;
    player.r = static_cast<uint8_t>(i % 256);
    player.g = 0;
    player.b = 0;
    player.lastInputSequence = i;
    original.players.push_back(player);
  }

  auto serialized = serialize(original);
  size_t expectedSize = 7 + (1000 * 31);  // 31007 bytes (1+4+2+31000)
  assert(serialized.size() == expectedSize);

  auto deserialized =
      deserializeStateUpdate(serialized.data(), serialized.size());
  assert(deserialized.serverTick == 10000);
  assert(deserialized.players.size() == 1000);

  // Verify random players
  assert(deserialized.players[0].playerId == 0);
  assert(deserialized.players[500].playerId == 500);
  assert(deserialized.players[999].playerId == 999);
  assert(floatEqual(deserialized.players[999].x, 999.0f));
}

// ============================================================================
// Phase 2: InputSystem Tests
// ============================================================================

// Parameterized test for key bindings
TEST(InputSystem_KeyBindings) {
  struct KeyTest {
    int sdlKey;
    bool* flagPtr(LocalInputEvent& e) const {
      if (sdlKey == SDLK_w || sdlKey == SDLK_UP) return &e.moveUp;
      if (sdlKey == SDLK_s || sdlKey == SDLK_DOWN) return &e.moveDown;
      if (sdlKey == SDLK_a || sdlKey == SDLK_LEFT) return &e.moveLeft;
      if (sdlKey == SDLK_d || sdlKey == SDLK_RIGHT) return &e.moveRight;
      return nullptr;
    }
  };

  KeyTest keys[] = {
      {SDLK_w}, {SDLK_UP},    // moveUp
      {SDLK_s}, {SDLK_DOWN},  // moveDown
      {SDLK_a}, {SDLK_LEFT},  // moveLeft
      {SDLK_d}, {SDLK_RIGHT}  // moveRight
  };

  for (const auto& keyTest : keys) {
    resetEventBus();
    InputSystem inputSystem;

    // Test key down
    LocalInputEvent downEvent =
        publishKeyAndUpdate(inputSystem, keyTest.sdlKey, true);
    assert(*keyTest.flagPtr(downEvent) == true);

    // Test key up
    resetEventBus();
    InputSystem inputSystem2;
    publishKeyAndUpdate(inputSystem2, keyTest.sdlKey, true);  // Press first
    LocalInputEvent upEvent =
        publishKeyAndUpdate(inputSystem2, keyTest.sdlKey, false);
    assert(*keyTest.flagPtr(upEvent) == false);
  }
}

TEST(InputSystem_AllDirections) {
  resetEventBus();
  InputSystem inputSystem;

  // Press all WASD keys
  EventBus::instance().publish(KeyDownEvent{SDLK_w});
  EventBus::instance().publish(KeyDownEvent{SDLK_a});
  EventBus::instance().publish(KeyDownEvent{SDLK_s});
  EventBus::instance().publish(KeyDownEvent{SDLK_d});

  LocalInputEvent capturedEvent;
  EventBus::instance().subscribe<LocalInputEvent>(
      [&](const LocalInputEvent& e) { capturedEvent = e; });

  EventBus::instance().publish(UpdateEvent{16.67f, 1});

  assert(capturedEvent.moveUp && capturedEvent.moveDown &&
         capturedEvent.moveLeft && capturedEvent.moveRight);
}

TEST(InputSystem_SequenceIncrement) {
  resetEventBus();
  InputSystem inputSystem;

  uint32_t sequences[3] = {0};
  int count = 0;

  EventBus::instance().subscribe<LocalInputEvent>(
      [&](const LocalInputEvent& e) {
        if (count < 3) sequences[count++] = e.inputSequence;
      });

  for (int i = 0; i < 3; i++) {
    EventBus::instance().publish(UpdateEvent{16.67f, static_cast<uint32_t>(i)});
  }

  assert(sequences[1] == sequences[0] + 1);
  assert(sequences[2] == sequences[1] + 1);
}

// ============================================================================
// Phase 7: RemotePlayerInterpolation Tests
// ============================================================================

TEST(RemotePlayerInterpolation_PlayerLifecycle) {
  resetEventBus();
  RemotePlayerInterpolation interpolation(999);  // Temporary local player ID

  // First PlayerJoined is for local player (server assigns real ID)
  publishPlayerJoined(1, 0, 0, 255);  // Local player gets ID 1
  assert(interpolation.getRemotePlayerIds().size() ==
         0);  // Not added as remote

  // Test adding remote player
  publishPlayerJoined(2, 255, 0, 0);
  assert(interpolation.getRemotePlayerIds().size() == 1);
  assert(interpolation.getRemotePlayerIds()[0] == 2);

  // Test removing remote player
  PlayerLeftPacket leftPacket;
  leftPacket.playerId = 2;
  auto leftSerialized = serialize(leftPacket);
  EventBus::instance().publish(NetworkPacketReceivedEvent{
      nullptr, leftSerialized.data(), leftSerialized.size()});
  assert(interpolation.getRemotePlayerIds().size() == 0);
}

TEST(RemotePlayerInterpolation_SkipLocalPlayer) {
  resetEventBus();
  RemotePlayerInterpolation interpolation(999);  // Temporary ID

  // First PlayerJoined is for local player
  publishPlayerJoined(1, 255, 0, 0);
  assert(interpolation.getRemotePlayerIds().size() == 0);

  // State update with only local player (should be ignored)
  PlayerState ps{1, 100.0f, 100.0f, 0.0f, 0.0f, 100.0f, 255, 0, 0, 0};
  publishStateUpdate(1, {ps});
  assert(interpolation.getRemotePlayerIds().size() == 0);
}

TEST(RemotePlayerInterpolation_Interpolation) {
  resetEventBus();
  RemotePlayerInterpolation interpolation(999);  // Temporary ID

  // First PlayerJoined is for local player
  publishPlayerJoined(1, 0, 0, 255);

  // Second PlayerJoined is for remote player
  publishPlayerJoined(2, 255, 0, 0);

  // Test with no snapshots
  Player player;
  assert(interpolation.getInterpolatedState(2, 0.5f, player));
  assert(player.id == 2);

  // Test with 1 snapshot
  PlayerState ps1{2, 100.0f, 200.0f, 10.0f, 20.0f, 90.0f, 255, 0, 0, 0};
  publishStateUpdate(1, {ps1});
  interpolation.getInterpolatedState(2, 0.5f, player);
  assert(floatEqual(player.x, 100.0f));
  assert(floatEqual(player.y, 200.0f));

  // Test with 2 snapshots - parameterized interpolation
  PlayerState ps2{2, 200.0f, 400.0f, 20.0f, 40.0f, 80.0f, 255, 0, 0, 0};
  publishStateUpdate(2, {ps2});

  struct InterpolationTest {
    float t;
    float expectedX;
    float expectedY;
  };

  InterpolationTest tests[] = {
      {0.0f, 100.0f, 200.0f},  // Start
      {0.5f, 150.0f, 300.0f},  // Middle
      {1.0f, 200.0f, 400.0f}   // End
  };

  for (const auto& test : tests) {
    interpolation.getInterpolatedState(2, test.t, player);
    assert(floatEqual(player.x, test.expectedX));
    assert(floatEqual(player.y, test.expectedY));
  }
}

TEST(RemotePlayerInterpolation_MultipleRemotePlayers) {
  resetEventBus();
  RemotePlayerInterpolation interpolation(999);  // Temporary ID

  // First PlayerJoined is for local player
  publishPlayerJoined(1, 0, 0, 255);

  // Add 3 remote players
  for (uint32_t id = 2; id <= 4; id++) {
    publishPlayerJoined(id, static_cast<uint8_t>(id * 50), 0, 0);
  }

  assert(interpolation.getRemotePlayerIds().size() == 3);

  // Send state update with all players
  std::vector<PlayerState> states;
  for (uint32_t id = 2; id <= 4; id++) {
    PlayerState ps{id,
                   float(id * 100),
                   float(id * 100),
                   0.0f,
                   0.0f,
                   100.0f,
                   static_cast<uint8_t>(id * 50),
                   0,
                   0,
                   0};
    states.push_back(ps);
  }
  publishStateUpdate(1, states);

  // Verify each player can be queried
  for (uint32_t id = 2; id <= 4; id++) {
    Player player;
    assert(interpolation.getInterpolatedState(id, 0.0f, player));
    assert(floatEqual(player.x, float(id * 100)));
  }
}

// ============================================================================
// Phase 6: ClientPrediction Tests
// ============================================================================

TEST(ClientPrediction_ColorSyncDuringReconciliation) {
  resetEventBus();
  NetworkClient client;  // Dummy client for testing
  ClientPrediction prediction(&client, 1, 800.0f, 600.0f);

  // Simulate server sending a state update with a specific color (Red)
  PlayerState serverState{1, 100.0f, 100.0f, 0.0f, 0.0f, 100.0f, 255, 0, 0, 5};
  publishStateUpdate(1, {serverState});

  // Verify local player has the correct color from server
  const Player& localPlayer = prediction.getLocalPlayer();
  assert(localPlayer.r == 255);
  assert(localPlayer.g == 0);
  assert(localPlayer.b == 0);
}

TEST(ClientPrediction_ColorPersistsAcrossMultipleReconciles) {
  resetEventBus();
  NetworkClient client;
  ClientPrediction prediction(&client, 1, 800.0f, 600.0f);

  // Send multiple state updates with the same color
  for (uint32_t tick = 1; tick <= 5; tick++) {
    PlayerState serverState{1,      static_cast<float>(tick * 10),
                            100.0f, 0.0f,
                            0.0f,   100.0f,
                            0,      255,
                            0,      tick};
    publishStateUpdate(tick, {serverState});
  }

  // Verify color is still Green after multiple reconciliations
  const Player& localPlayer = prediction.getLocalPlayer();
  assert(localPlayer.r == 0);
  assert(localPlayer.g == 255);
  assert(localPlayer.b == 0);
}

int main() {
  Logger::init();

  std::cout << "\n=== Running Gambit Engine Tests ===\n\n";

  // Phase 3: Network Protocol Tests
  std::cout << "Phase 3: Network Protocol\n";
  run_ClientInputSerialization();
  run_StateUpdateSerialization();
  run_PlayerJoinedSerialization();
  run_PlayerLeftSerialization();
  run_NetworkProtocol_StateUpdate_ZeroPlayers();
  run_NetworkProtocol_StateUpdate_OnePlayer();
  run_NetworkProtocol_StateUpdate_100Players();
  run_NetworkProtocol_StateUpdate_1000Players();

  // Phase 1: EventBus Tests
  std::cout << "\nPhase 1: EventBus\n";
  run_EventBus_SingleSubscriberPublish();
  run_EventBus_MultipleSubscribers();
  run_EventBus_MultipleEventTypes();
  run_EventBus_PublishWithNoSubscribers();
  run_EventBus_Clear();

  // Phase 4: Player Movement Tests
  std::cout << "\nPhase 4: Player Movement\n";
  run_Player_MoveRight();
  run_Player_MoveLeft();
  run_Player_MoveUp();
  run_Player_MoveDown();
  run_Player_DiagonalMovement();
  run_Player_Stationary();
  run_Player_BoundsClampingLeft();
  run_Player_BoundsClampingRight();
  run_Player_BoundsClampingTop();
  run_Player_BoundsClampingBottom();
  run_Player_IsometricMapBounds_BottomRight();
  run_Player_IsometricMapBounds_TopLeft();
  run_Player_IsometricMapBounds_AllCorners();

  // Phase 2: InputSystem Tests
  std::cout << "\nPhase 2: InputSystem\n";
  run_InputSystem_KeyBindings();
  run_InputSystem_AllDirections();
  run_InputSystem_SequenceIncrement();

  // Phase 7: RemotePlayerInterpolation Tests
  std::cout << "\nPhase 7: RemotePlayerInterpolation\n";
  run_RemotePlayerInterpolation_PlayerLifecycle();
  run_RemotePlayerInterpolation_SkipLocalPlayer();
  run_RemotePlayerInterpolation_Interpolation();
  run_RemotePlayerInterpolation_MultipleRemotePlayers();

  // Phase 6: ClientPrediction Tests
  std::cout << "\nPhase 6: ClientPrediction\n";
  run_ClientPrediction_ColorSyncDuringReconciliation();
  run_ClientPrediction_ColorPersistsAcrossMultipleReconciles();

  std::cout << "\n=== Test Results ===\n";
  std::cout << "Tests run: " << testsRun << "\n";
  std::cout << "Tests passed: " << testsPassed << "\n";

  if (testsRun == testsPassed) {
    std::cout << "\n✓ All tests passed!\n\n";
    return 0;
  } else {
    std::cout << "\n✗ Some tests failed!\n\n";
    return 1;
  }
}
