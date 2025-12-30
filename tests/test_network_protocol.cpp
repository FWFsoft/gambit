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

// Note: Malformed packet tests removed - Tiger Style approach asserts on
// invalid data The deserializers assert on malformed input rather than handling
// gracefully

TEST(NetworkProtocol_AllPacketTypes_RoundTrip) {
  // Data-driven test for all packet types
  struct PacketTest {
    const char* name;
    void (*testFunc)();
  };

  auto testAttackEnemy = []() {
    AttackEnemyPacket original;
    original.enemyId = 42;
    original.damage = 25.5f;

    auto serialized = serialize(original);
    assert(serialized.size() == 9);  // 1 + 4 + 4
    assert(serialized[0] == static_cast<uint8_t>(PacketType::AttackEnemy));

    auto deserialized =
        deserializeAttackEnemy(serialized.data(), serialized.size());
    assert(deserialized.enemyId == 42);
    assert(floatEqual(deserialized.damage, 25.5f));
  };

  auto testUseItem = []() {
    UseItemPacket original;
    original.slotIndex = 5;

    auto serialized = serialize(original);
    assert(serialized.size() == 2);
    assert(serialized[0] == static_cast<uint8_t>(PacketType::UseItem));

    auto deserialized =
        deserializeUseItem(serialized.data(), serialized.size());
    assert(deserialized.slotIndex == 5);
  };

  auto testEquipItem = []() {
    EquipItemPacket original;
    original.inventorySlot = 3;
    original.equipmentSlot = 1;

    auto serialized = serialize(original);
    assert(serialized.size() == 3);

    auto deserialized =
        deserializeEquipItem(serialized.data(), serialized.size());
    assert(deserialized.inventorySlot == 3);
    assert(deserialized.equipmentSlot == 1);
  };

  auto testItemSpawned = []() {
    ItemSpawnedPacket original;
    original.worldItemId = 100;
    original.itemId = 42;
    original.x = 123.45f;
    original.y = 678.90f;

    auto serialized = serialize(original);
    assert(serialized.size() == 17);

    auto deserialized =
        deserializeItemSpawned(serialized.data(), serialized.size());
    assert(deserialized.worldItemId == 100);
    assert(deserialized.itemId == 42);
    assert(floatEqual(deserialized.x, 123.45f));
    assert(floatEqual(deserialized.y, 678.90f));
  };

  auto testItemPickupRequest = []() {
    ItemPickupRequestPacket original;
    original.worldItemId = 55;

    auto serialized = serialize(original);
    assert(serialized.size() == 5);

    auto deserialized =
        deserializeItemPickupRequest(serialized.data(), serialized.size());
    assert(deserialized.worldItemId == 55);
  };

  auto testItemPickedUp = []() {
    ItemPickedUpPacket original;
    original.worldItemId = 33;
    original.playerId = 77;

    auto serialized = serialize(original);
    assert(serialized.size() == 9);

    auto deserialized =
        deserializeItemPickedUp(serialized.data(), serialized.size());
    assert(deserialized.worldItemId == 33);
    assert(deserialized.playerId == 77);
  };

  auto testInventoryUpdate = []() {
    InventoryUpdatePacket original;
    original.playerId = 123;

    // Add some inventory items
    for (int i = 0; i < 5; i++) {
      original.inventory[i] =
          NetworkItemStack{static_cast<uint32_t>(i + 1), i * 2};
    }

    // Add equipment
    original.equipment[0] = NetworkItemStack{10, 1};
    original.equipment[1] = NetworkItemStack{20, 1};

    auto serialized = serialize(original);
    auto deserialized =
        deserializeInventoryUpdate(serialized.data(), serialized.size());

    assert(deserialized.playerId == 123);
    assert(deserialized.inventory[0].itemId == 1);
    assert(deserialized.inventory[0].quantity == 0);
    assert(deserialized.equipment[0].itemId == 10);
  };

  auto testPlayerDied = []() {
    PlayerDiedPacket original;
    original.playerId = 42;

    auto serialized = serialize(original);
    assert(serialized.size() == 5);  // 1 + 4

    auto deserialized =
        deserializePlayerDied(serialized.data(), serialized.size());
    assert(deserialized.playerId == 42);
  };

  auto testPlayerRespawned = []() {
    PlayerRespawnedPacket original;
    original.playerId = 7;
    original.x = 100.5f;
    original.y = 200.5f;

    auto serialized = serialize(original);
    assert(serialized.size() == 13);  // 1 + 4 + 4 + 4

    auto deserialized =
        deserializePlayerRespawned(serialized.data(), serialized.size());
    assert(deserialized.playerId == 7);
    assert(floatEqual(deserialized.x, 100.5f));
    assert(floatEqual(deserialized.y, 200.5f));
  };

  auto testEnemyStateUpdate = []() {
    EnemyStateUpdatePacket original;

    // Add some enemies
    for (int i = 0; i < 3; i++) {
      NetworkEnemyState enemy;
      enemy.id = i;
      enemy.type = static_cast<uint8_t>(i % 2);  // Alternate types
      enemy.state = static_cast<uint8_t>(i);
      enemy.x = i * 10.0f;
      enemy.y = i * 20.0f;
      enemy.vx = 1.0f;
      enemy.vy = 2.0f;
      enemy.health = 50.0f;
      enemy.maxHealth = 100.0f;
      original.enemies.push_back(enemy);
    }

    auto serialized = serialize(original);
    auto deserialized =
        deserializeEnemyStateUpdate(serialized.data(), serialized.size());

    assert(deserialized.enemies.size() == 3);
    assert(deserialized.enemies[0].id == 0);
    assert(floatEqual(deserialized.enemies[1].x, 10.0f));
    assert(floatEqual(deserialized.enemies[2].health, 50.0f));
  };

  auto testEnemyDied = []() {
    EnemyDiedPacket original;
    original.enemyId = 99;
    original.killerId = 5;

    auto serialized = serialize(original);
    assert(serialized.size() == 9);  // 1 + 4 + 4

    auto deserialized =
        deserializeEnemyDied(serialized.data(), serialized.size());
    assert(deserialized.enemyId == 99);
    assert(deserialized.killerId == 5);
  };

  PacketTest tests[] = {
      {"AttackEnemy", testAttackEnemy},
      {"UseItem", testUseItem},
      {"EquipItem", testEquipItem},
      {"ItemSpawned", testItemSpawned},
      {"ItemPickupRequest", testItemPickupRequest},
      {"ItemPickedUp", testItemPickedUp},
      {"InventoryUpdate", testInventoryUpdate},
      {"PlayerDied", testPlayerDied},
      {"PlayerRespawned", testPlayerRespawned},
      {"EnemyStateUpdate", testEnemyStateUpdate},
      {"EnemyDied", testEnemyDied},
  };

  for (const auto& test : tests) {
    test.testFunc();
  }
}

TEST(NetworkProtocol_BoundaryValues) {
  // Test with maximum/minimum values
  struct BoundaryTest {
    const char* name;
    void (*testFunc)();
  };

  auto testMaxUint32 = []() {
    PlayerJoinedPacket original;
    original.playerId = UINT32_MAX;
    original.r = 255;
    original.g = 255;
    original.b = 255;

    auto serialized = serialize(original);
    auto deserialized =
        deserializePlayerJoined(serialized.data(), serialized.size());
    assert(deserialized.playerId == UINT32_MAX);
  };

  auto testMaxFloat = []() {
    StateUpdatePacket original;
    original.serverTick = 0;

    PlayerState player;
    player.playerId = 1;
    player.x = 99999.99f;
    player.y = -99999.99f;
    player.vx = 1000.0f;
    player.vy = -1000.0f;
    player.health = 10000.0f;
    player.r = 255;
    player.g = 255;
    player.b = 255;
    player.lastInputSequence = UINT32_MAX;
    original.players.push_back(player);

    auto serialized = serialize(original);
    auto deserialized =
        deserializeStateUpdate(serialized.data(), serialized.size());
    assert(deserialized.players[0].x == 99999.99f);
    assert(deserialized.players[0].y == -99999.99f);
  };

  auto testZeroValues = []() {
    StateUpdatePacket original;
    original.serverTick = 0;

    PlayerState player;
    player.playerId = 0;
    player.x = 0.0f;
    player.y = 0.0f;
    player.vx = 0.0f;
    player.vy = 0.0f;
    player.health = 0.0f;
    player.r = 0;
    player.g = 0;
    player.b = 0;
    player.lastInputSequence = 0;
    original.players.push_back(player);

    auto serialized = serialize(original);
    auto deserialized =
        deserializeStateUpdate(serialized.data(), serialized.size());
    assert(deserialized.players[0].playerId == 0);
    assert(deserialized.players[0].x == 0.0f);
    assert(deserialized.players[0].health == 0.0f);
  };

  BoundaryTest tests[] = {
      {"MaxUint32", testMaxUint32},
      {"MaxFloat", testMaxFloat},
      {"ZeroValues", testZeroValues},
  };

  for (const auto& test : tests) {
    test.testFunc();
  }
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
  test_NetworkProtocol_AllPacketTypes_RoundTrip();
  test_NetworkProtocol_BoundaryValues();

  return 0;
}
