#include "NetworkProtocol.h"

#include <cstring>

// Helper functions for binary I/O (little-endian)

void writeUint32(std::vector<uint8_t>& buffer, uint32_t value) {
  buffer.push_back(value & 0xFF);
  buffer.push_back((value >> 8) & 0xFF);
  buffer.push_back((value >> 16) & 0xFF);
  buffer.push_back((value >> 24) & 0xFF);
}

void writeUint16(std::vector<uint8_t>& buffer, uint16_t value) {
  buffer.push_back(value & 0xFF);
  buffer.push_back((value >> 8) & 0xFF);
}

void writeFloat(std::vector<uint8_t>& buffer, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(float));
  writeUint32(buffer, bits);
}

void writeInt32(std::vector<uint8_t>& buffer, int32_t value) {
  uint32_t uvalue;
  std::memcpy(&uvalue, &value, sizeof(int32_t));
  writeUint32(buffer, uvalue);
}

void writeUint8(std::vector<uint8_t>& buffer, uint8_t value) {
  buffer.push_back(value);
}

uint32_t readUint32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

int32_t readInt32(const uint8_t* data) {
  uint32_t uvalue = readUint32(data);
  int32_t value;
  std::memcpy(&value, &uvalue, sizeof(int32_t));
  return value;
}

uint16_t readUint16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

float readFloat(const uint8_t* data) {
  uint32_t bits = readUint32(data);
  float value;
  std::memcpy(&value, &bits, sizeof(float));
  return value;
}

uint8_t readUint8(const uint8_t* data) { return data[0]; }

// Serialization

std::vector<uint8_t> serialize(const ClientInputPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.inputSequence);

  // Pack movement flags into single byte
  uint8_t flags = 0;
  if (packet.moveLeft) flags |= 0x01;
  if (packet.moveRight) flags |= 0x02;
  if (packet.moveUp) flags |= 0x04;
  if (packet.moveDown) flags |= 0x08;
  writeUint8(buffer, flags);

  assert(buffer.size() == 6);  // 1 + 4 + 1 = 6 bytes
  return buffer;
}

std::vector<uint8_t> serialize(const StateUpdatePacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.serverTick);
  writeUint16(buffer, static_cast<uint16_t>(packet.players.size()));

  for (const auto& player : packet.players) {
    writeUint32(buffer, player.playerId);
    writeFloat(buffer, player.x);
    writeFloat(buffer, player.y);
    writeFloat(buffer, player.vx);
    writeFloat(buffer, player.vy);
    writeFloat(buffer, player.health);
    writeUint8(buffer, player.r);
    writeUint8(buffer, player.g);
    writeUint8(buffer, player.b);
    writeUint32(buffer, player.lastInputSequence);
  }

  // 1 + 4 + 2 + (playerCount * 31) bytes
  return buffer;
}

std::vector<uint8_t> serialize(const PlayerJoinedPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.playerId);
  writeUint8(buffer, packet.r);
  writeUint8(buffer, packet.g);
  writeUint8(buffer, packet.b);

  assert(buffer.size() == 8);  // 1 + 4 + 1 + 1 + 1 = 8 bytes
  return buffer;
}

std::vector<uint8_t> serialize(const PlayerLeftPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.playerId);

  assert(buffer.size() == 5);  // 1 + 4 = 5 bytes
  return buffer;
}

std::vector<uint8_t> serialize(const PlayerDiedPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.playerId);

  assert(buffer.size() == 5);  // 1 + 4 = 5 bytes
  return buffer;
}

std::vector<uint8_t> serialize(const PlayerRespawnedPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.playerId);
  writeFloat(buffer, packet.x);
  writeFloat(buffer, packet.y);

  assert(buffer.size() == 13);  // 1 + 4 + 4 + 4 = 13 bytes
  return buffer;
}

// Deserialization

ClientInputPacket deserializeClientInput(const uint8_t* data, size_t size) {
  assert(size >= 6);
  assert(data[0] == static_cast<uint8_t>(PacketType::ClientInput));

  ClientInputPacket packet;
  packet.inputSequence = readUint32(data + 1);

  uint8_t flags = readUint8(data + 5);
  packet.moveLeft = (flags & 0x01) != 0;
  packet.moveRight = (flags & 0x02) != 0;
  packet.moveUp = (flags & 0x04) != 0;
  packet.moveDown = (flags & 0x08) != 0;

  return packet;
}

StateUpdatePacket deserializeStateUpdate(const uint8_t* data, size_t size) {
  assert(size >= 7);  // 1 + 4 + 2 = 7 bytes minimum
  assert(data[0] == static_cast<uint8_t>(PacketType::StateUpdate));

  StateUpdatePacket packet;
  packet.serverTick = readUint32(data + 1);
  uint16_t playerCount = readUint16(data + 5);

  assert(size >= 7 + (playerCount * 31));

  size_t offset = 7;
  for (uint16_t i = 0; i < playerCount; ++i) {
    PlayerState player;
    player.playerId = readUint32(data + offset);
    offset += 4;
    player.x = readFloat(data + offset);
    offset += 4;
    player.y = readFloat(data + offset);
    offset += 4;
    player.vx = readFloat(data + offset);
    offset += 4;
    player.vy = readFloat(data + offset);
    offset += 4;
    player.health = readFloat(data + offset);
    offset += 4;
    player.r = readUint8(data + offset);
    offset += 1;
    player.g = readUint8(data + offset);
    offset += 1;
    player.b = readUint8(data + offset);
    offset += 1;
    player.lastInputSequence = readUint32(data + offset);
    offset += 4;

    packet.players.push_back(player);
  }

  return packet;
}

PlayerJoinedPacket deserializePlayerJoined(const uint8_t* data, size_t size) {
  assert(size >= 8);
  assert(data[0] == static_cast<uint8_t>(PacketType::PlayerJoined));

  PlayerJoinedPacket packet;
  packet.playerId = readUint32(data + 1);
  packet.r = readUint8(data + 5);
  packet.g = readUint8(data + 6);
  packet.b = readUint8(data + 7);

  return packet;
}

PlayerLeftPacket deserializePlayerLeft(const uint8_t* data, size_t size) {
  assert(size >= 5);
  assert(data[0] == static_cast<uint8_t>(PacketType::PlayerLeft));

  PlayerLeftPacket packet;
  packet.playerId = readUint32(data + 1);

  return packet;
}

PlayerDiedPacket deserializePlayerDied(const uint8_t* data, size_t size) {
  assert(size >= 5);
  assert(data[0] == static_cast<uint8_t>(PacketType::PlayerDied));

  PlayerDiedPacket packet;
  packet.playerId = readUint32(data + 1);

  return packet;
}

PlayerRespawnedPacket deserializePlayerRespawned(const uint8_t* data,
                                                 size_t size) {
  assert(size >= 13);
  assert(data[0] == static_cast<uint8_t>(PacketType::PlayerRespawned));

  PlayerRespawnedPacket packet;
  packet.playerId = readUint32(data + 1);
  packet.x = readFloat(data + 5);
  packet.y = readFloat(data + 9);

  return packet;
}

// Enemy packet serialization

std::vector<uint8_t> serialize(const EnemyStateUpdatePacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint16(buffer, static_cast<uint16_t>(packet.enemies.size()));

  for (const auto& enemy : packet.enemies) {
    writeUint32(buffer, enemy.id);
    writeUint8(buffer, enemy.type);
    writeUint8(buffer, enemy.state);
    writeFloat(buffer, enemy.x);
    writeFloat(buffer, enemy.y);
    writeFloat(buffer, enemy.vx);
    writeFloat(buffer, enemy.vy);
    writeFloat(buffer, enemy.health);
    writeFloat(buffer, enemy.maxHealth);
  }

  // 1 + 2 + (enemyCount * 30) bytes
  // Per enemy: 4 + 1 + 1 + 4 + 4 + 4 + 4 + 4 + 4 = 30 bytes
  return buffer;
}

std::vector<uint8_t> serialize(const AttackEnemyPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.enemyId);
  writeFloat(buffer, packet.damage);

  assert(buffer.size() == 9);  // 1 + 4 + 4 = 9 bytes
  return buffer;
}

std::vector<uint8_t> serialize(const EnemyDamagedPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.enemyId);
  writeFloat(buffer, packet.newHealth);
  writeUint32(buffer, packet.attackerId);

  assert(buffer.size() == 13);  // 1 + 4 + 4 + 4 = 13 bytes
  return buffer;
}

std::vector<uint8_t> serialize(const EnemyDiedPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.enemyId);
  writeUint32(buffer, packet.killerId);

  assert(buffer.size() == 9);  // 1 + 4 + 4 = 9 bytes
  return buffer;
}

// Enemy packet deserialization

EnemyStateUpdatePacket deserializeEnemyStateUpdate(const uint8_t* data,
                                                   size_t size) {
  assert(size >= 3);  // 1 + 2 = 3 bytes minimum
  assert(data[0] == static_cast<uint8_t>(PacketType::EnemyStateUpdate));

  EnemyStateUpdatePacket packet;
  uint16_t enemyCount = readUint16(data + 1);

  assert(size >= 3 + (enemyCount * 30));

  size_t offset = 3;
  for (uint16_t i = 0; i < enemyCount; ++i) {
    NetworkEnemyState enemy;
    enemy.id = readUint32(data + offset);
    offset += 4;
    enemy.type = readUint8(data + offset);
    offset += 1;
    enemy.state = readUint8(data + offset);
    offset += 1;
    enemy.x = readFloat(data + offset);
    offset += 4;
    enemy.y = readFloat(data + offset);
    offset += 4;
    enemy.vx = readFloat(data + offset);
    offset += 4;
    enemy.vy = readFloat(data + offset);
    offset += 4;
    enemy.health = readFloat(data + offset);
    offset += 4;
    enemy.maxHealth = readFloat(data + offset);
    offset += 4;

    packet.enemies.push_back(enemy);
  }

  return packet;
}

AttackEnemyPacket deserializeAttackEnemy(const uint8_t* data, size_t size) {
  assert(size >= 9);
  assert(data[0] == static_cast<uint8_t>(PacketType::AttackEnemy));

  AttackEnemyPacket packet;
  packet.enemyId = readUint32(data + 1);
  packet.damage = readFloat(data + 5);

  return packet;
}

EnemyDamagedPacket deserializeEnemyDamaged(const uint8_t* data, size_t size) {
  assert(size >= 13);
  assert(data[0] == static_cast<uint8_t>(PacketType::EnemyDamaged));

  EnemyDamagedPacket packet;
  packet.enemyId = readUint32(data + 1);
  packet.newHealth = readFloat(data + 5);
  packet.attackerId = readUint32(data + 9);

  return packet;
}

EnemyDiedPacket deserializeEnemyDied(const uint8_t* data, size_t size) {
  assert(size >= 9);
  assert(data[0] == static_cast<uint8_t>(PacketType::EnemyDied));

  EnemyDiedPacket packet;
  packet.enemyId = readUint32(data + 1);
  packet.killerId = readUint32(data + 5);

  return packet;
}

// Inventory packet serialization

std::vector<uint8_t> serialize(const InventoryUpdatePacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.playerId);

  // Serialize inventory (20 slots)
  for (int i = 0; i < 20; i++) {
    writeUint32(buffer, packet.inventory[i].itemId);
    writeInt32(buffer, packet.inventory[i].quantity);
  }

  // Serialize equipment (2 slots)
  for (int i = 0; i < 2; i++) {
    writeUint32(buffer, packet.equipment[i].itemId);
    writeInt32(buffer, packet.equipment[i].quantity);
  }

  // 1 + 4 + (20 * 8) + (2 * 8) = 181 bytes
  assert(buffer.size() == 181);
  return buffer;
}

std::vector<uint8_t> serialize(const UseItemPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint8(buffer, packet.slotIndex);

  assert(buffer.size() == 2);
  return buffer;
}

std::vector<uint8_t> serialize(const EquipItemPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint8(buffer, packet.inventorySlot);
  writeUint8(buffer, packet.equipmentSlot);

  assert(buffer.size() == 3);
  return buffer;
}

// Inventory packet deserialization

InventoryUpdatePacket deserializeInventoryUpdate(const uint8_t* data,
                                                 size_t size) {
  assert(size >= 181);
  assert(data[0] == static_cast<uint8_t>(PacketType::InventoryUpdate));

  InventoryUpdatePacket packet;
  size_t offset = 1;

  packet.playerId = readUint32(data + offset);
  offset += 4;

  // Deserialize inventory (20 slots)
  for (int i = 0; i < 20; i++) {
    packet.inventory[i].itemId = readUint32(data + offset);
    offset += 4;
    packet.inventory[i].quantity = readInt32(data + offset);
    offset += 4;
  }

  // Deserialize equipment (2 slots)
  for (int i = 0; i < 2; i++) {
    packet.equipment[i].itemId = readUint32(data + offset);
    offset += 4;
    packet.equipment[i].quantity = readInt32(data + offset);
    offset += 4;
  }

  return packet;
}

UseItemPacket deserializeUseItem(const uint8_t* data, size_t size) {
  assert(size >= 2);
  assert(data[0] == static_cast<uint8_t>(PacketType::UseItem));

  UseItemPacket packet;
  packet.slotIndex = readUint8(data + 1);

  return packet;
}

EquipItemPacket deserializeEquipItem(const uint8_t* data, size_t size) {
  assert(size >= 3);
  assert(data[0] == static_cast<uint8_t>(PacketType::EquipItem));

  EquipItemPacket packet;
  packet.inventorySlot = readUint8(data + 1);
  packet.equipmentSlot = readUint8(data + 2);

  return packet;
}

// World item packet serialization

std::vector<uint8_t> serialize(const ItemSpawnedPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.worldItemId);
  writeUint32(buffer, packet.itemId);
  writeFloat(buffer, packet.x);
  writeFloat(buffer, packet.y);

  assert(buffer.size() == 17);
  return buffer;
}

std::vector<uint8_t> serialize(const ItemPickupRequestPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.worldItemId);

  assert(buffer.size() == 5);
  return buffer;
}

std::vector<uint8_t> serialize(const ItemPickedUpPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.worldItemId);
  writeUint32(buffer, packet.playerId);

  assert(buffer.size() == 9);
  return buffer;
}

// World item packet deserialization

ItemSpawnedPacket deserializeItemSpawned(const uint8_t* data, size_t size) {
  assert(size >= 17);
  assert(data[0] == static_cast<uint8_t>(PacketType::ItemSpawned));

  ItemSpawnedPacket packet;
  size_t offset = 1;

  packet.worldItemId = readUint32(data + offset);
  offset += 4;
  packet.itemId = readUint32(data + offset);
  offset += 4;
  packet.x = readFloat(data + offset);
  offset += 4;
  packet.y = readFloat(data + offset);

  return packet;
}

ItemPickupRequestPacket deserializeItemPickupRequest(const uint8_t* data,
                                                     size_t size) {
  assert(size >= 5);
  assert(data[0] == static_cast<uint8_t>(PacketType::ItemPickupRequest));

  ItemPickupRequestPacket packet;
  packet.worldItemId = readUint32(data + 1);

  return packet;
}

ItemPickedUpPacket deserializeItemPickedUp(const uint8_t* data, size_t size) {
  assert(size >= 9);
  assert(data[0] == static_cast<uint8_t>(PacketType::ItemPickedUp));

  ItemPickedUpPacket packet;
  size_t offset = 1;

  packet.worldItemId = readUint32(data + offset);
  offset += 4;
  packet.playerId = readUint32(data + offset);

  return packet;
}
