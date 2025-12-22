#include "NetworkProtocol.h"
#include "Logger.h"
#include <iostream>
#include <cassert>

int testsRun = 0;
int testsPassed = 0;

#define TEST(name) \
    void test_##name(); \
    void run_##name() { \
        testsRun++; \
        std::cout << "Running test: " << #name << "..."; \
        test_##name(); \
        testsPassed++; \
        std::cout << " PASSED\n"; \
    } \
    void test_##name()

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

    auto deserialized = deserializeClientInput(serialized.data(), serialized.size());
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

    original.players.push_back(player1);
    original.players.push_back(player2);

    auto serialized = serialize(original);
    assert(serialized.size() == 6 + (2 * 27));  // 60 bytes for 2 players

    auto deserialized = deserializeStateUpdate(serialized.data(), serialized.size());
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

    auto deserialized = deserializePlayerJoined(serialized.data(), serialized.size());
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

    auto deserialized = deserializePlayerLeft(serialized.data(), serialized.size());
    assert(deserialized.playerId == 456);
}

int main() {
    Logger::init();

    std::cout << "\n=== Running Gambit Engine Tests ===\n\n";

    run_ClientInputSerialization();
    run_StateUpdateSerialization();
    run_PlayerJoinedSerialization();
    run_PlayerLeftSerialization();

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
