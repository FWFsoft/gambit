#include "Logger.h"
#include "NetworkProtocol.h"
#include "test_utils.h"

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
  assert(serialized.size() == 7 + (2 * 31));

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

TEST(PlayerLeftSerialization) {
  PlayerLeftPacket original;
  original.playerId = 456;

  auto serialized = serialize(original);
  assert(serialized.size() == 5);

  auto deserialized =
      deserializePlayerLeft(serialized.data(), serialized.size());
  assert(deserialized.playerId == 456);
}

TEST(NetworkProtocol_StateUpdate_ZeroPlayers) {
  StateUpdatePacket original;
  original.serverTick = 100;

  auto serialized = serialize(original);
  assert(serialized.size() == 7);

  auto deserialized =
      deserializeStateUpdate(serialized.data(), serialized.size());
  assert(deserialized.serverTick == 100);
  assert(deserialized.players.size() == 0);
}

TEST(NetworkProtocol_StateUpdate_OnePlayer) {
  StateUpdatePacket original;
  original.serverTick = 200;

  PlayerState player;
  player.playerId = 1;
  player.x = 50.0f;
  player.y = 100.0f;
  player.vx = 5.0f;
  player.vy = 10.0f;
  player.health = 100.0f;
  player.r = 255;
  player.g = 0;
  player.b = 0;
  player.lastInputSequence = 10;
  original.players.push_back(player);

  auto serialized = serialize(original);
  auto deserialized =
      deserializeStateUpdate(serialized.data(), serialized.size());
  assert(deserialized.serverTick == 200);
  assert(deserialized.players.size() == 1);
  assert(deserialized.players[0].playerId == 1);
}

TEST(NetworkProtocol_StateUpdate_100Players) {
  StateUpdatePacket original;
  original.serverTick = 500;

  for (int i = 0; i < 100; i++) {
    PlayerState player;
    player.playerId = i;
    player.x = i * 10.0f;
    player.y = i * 20.0f;
    player.vx = 0.0f;
    player.vy = 0.0f;
    player.health = 100.0f;
    player.r = 255;
    player.g = 0;
    player.b = 0;
    player.lastInputSequence = 0;
    original.players.push_back(player);
  }

  auto serialized = serialize(original);
  auto deserialized =
      deserializeStateUpdate(serialized.data(), serialized.size());
  assert(deserialized.serverTick == 500);
  assert(deserialized.players.size() == 100);
}

TEST(NetworkProtocol_StateUpdate_1000Players) {
  StateUpdatePacket original;
  original.serverTick = 1000;

  for (int i = 0; i < 1000; i++) {
    PlayerState player;
    player.playerId = i;
    player.x = i * 1.0f;
    player.y = i * 2.0f;
    player.vx = 0.0f;
    player.vy = 0.0f;
    player.health = 100.0f;
    player.r = 100;
    player.g = 150;
    player.b = 200;
    player.lastInputSequence = 0;
    original.players.push_back(player);
  }

  auto serialized = serialize(original);
  auto deserialized =
      deserializeStateUpdate(serialized.data(), serialized.size());
  assert(deserialized.serverTick == 1000);
  assert(deserialized.players.size() == 1000);
}

int main() {
  Logger::init();

  test_ClientInputSerialization();
  test_StateUpdateSerialization();
  test_PlayerJoinedSerialization();
  test_PlayerLeftSerialization();
  test_NetworkProtocol_StateUpdate_ZeroPlayers();
  test_NetworkProtocol_StateUpdate_OnePlayer();
  test_NetworkProtocol_StateUpdate_100Players();
  test_NetworkProtocol_StateUpdate_1000Players();

  return 0;
}
