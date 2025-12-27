#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

enum class PacketType : uint8_t {
  ClientInput = 1,
  StateUpdate = 2,
  PlayerJoined = 3,
  PlayerLeft = 4,
  EnemyStateUpdate = 5,
  EnemyDamaged = 6,
  EnemyDied = 7,
  AttackEnemy = 8
};

struct ClientInputPacket {
  PacketType type = PacketType::ClientInput;
  uint32_t inputSequence;
  bool moveLeft;
  bool moveRight;
  bool moveUp;
  bool moveDown;
};

struct PlayerState {
  uint32_t playerId;
  float x, y;
  float vx, vy;
  float health;
  uint8_t r, g, b;
  uint32_t lastInputSequence;  // Last input sequence processed by server
};

struct StateUpdatePacket {
  PacketType type = PacketType::StateUpdate;
  uint32_t serverTick;
  std::vector<PlayerState> players;
};

struct PlayerJoinedPacket {
  PacketType type = PacketType::PlayerJoined;
  uint32_t playerId;
  uint8_t r, g, b;
};

struct PlayerLeftPacket {
  PacketType type = PacketType::PlayerLeft;
  uint32_t playerId;
};

// Enemy-related packets

struct NetworkEnemyState {
  uint32_t id;
  uint8_t type;    // EnemyType enum
  uint8_t state;   // EnemyState enum
  float x, y;
  float vx, vy;
  float health;
  float maxHealth;
};

struct EnemyStateUpdatePacket {
  PacketType type = PacketType::EnemyStateUpdate;
  std::vector<NetworkEnemyState> enemies;
};

struct AttackEnemyPacket {
  PacketType type = PacketType::AttackEnemy;
  uint32_t enemyId;
  float damage;
};

struct EnemyDamagedPacket {
  PacketType type = PacketType::EnemyDamaged;
  uint32_t enemyId;
  float newHealth;
  uint32_t attackerId;
};

struct EnemyDiedPacket {
  PacketType type = PacketType::EnemyDied;
  uint32_t enemyId;
  uint32_t killerId;
};

// Serialization functions
std::vector<uint8_t> serialize(const ClientInputPacket& packet);
std::vector<uint8_t> serialize(const StateUpdatePacket& packet);
std::vector<uint8_t> serialize(const PlayerJoinedPacket& packet);
std::vector<uint8_t> serialize(const PlayerLeftPacket& packet);
std::vector<uint8_t> serialize(const EnemyStateUpdatePacket& packet);
std::vector<uint8_t> serialize(const AttackEnemyPacket& packet);
std::vector<uint8_t> serialize(const EnemyDamagedPacket& packet);
std::vector<uint8_t> serialize(const EnemyDiedPacket& packet);

// Deserialization functions
ClientInputPacket deserializeClientInput(const uint8_t* data, size_t size);
StateUpdatePacket deserializeStateUpdate(const uint8_t* data, size_t size);
PlayerJoinedPacket deserializePlayerJoined(const uint8_t* data, size_t size);
PlayerLeftPacket deserializePlayerLeft(const uint8_t* data, size_t size);
EnemyStateUpdatePacket deserializeEnemyStateUpdate(const uint8_t* data,
                                                    size_t size);
AttackEnemyPacket deserializeAttackEnemy(const uint8_t* data, size_t size);
EnemyDamagedPacket deserializeEnemyDamaged(const uint8_t* data, size_t size);
EnemyDiedPacket deserializeEnemyDied(const uint8_t* data, size_t size);

// Helper functions for binary I/O
void writeUint32(std::vector<uint8_t>& buffer, uint32_t value);
void writeUint16(std::vector<uint8_t>& buffer, uint16_t value);
void writeFloat(std::vector<uint8_t>& buffer, float value);
void writeUint8(std::vector<uint8_t>& buffer, uint8_t value);

uint32_t readUint32(const uint8_t* data);
uint16_t readUint16(const uint8_t* data);
float readFloat(const uint8_t* data);
uint8_t readUint8(const uint8_t* data);
