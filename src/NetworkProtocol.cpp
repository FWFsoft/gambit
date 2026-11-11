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
  writeUint8(buffer, packet.buttons);
  writeFloat(buffer, packet.aimX);
  writeFloat(buffer, packet.aimY);

  assert(buffer.size() == 15);  // 1 + 4 + 1 + 1 + 4 + 4 = 15 bytes
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
    writeFloat(buffer, player.maxHealth);
    writeFloat(buffer, player.shield);
    writeUint8(buffer, player.characterId);
    writeUint8(buffer, player.flags);
    writeFloat(buffer, player.speedMult);
    writeFloat(buffer, player.aimX);
    writeFloat(buffer, player.aimY);
    writeUint8(buffer, player.charge);
    writeUint8(buffer, player.chargeSlot);
    for (uint16_t cooldown : player.cooldowns) {
      writeUint16(buffer, cooldown);
    }
    writeUint8(buffer, player.resource);
  }

  assert(buffer.size() == 7 + packet.players.size() * PLAYER_STATE_BYTES);
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
  assert(size >= 15);
  assert(data[0] == static_cast<uint8_t>(PacketType::ClientInput));

  ClientInputPacket packet;
  packet.inputSequence = readUint32(data + 1);

  uint8_t flags = readUint8(data + 5);
  packet.moveLeft = (flags & 0x01) != 0;
  packet.moveRight = (flags & 0x02) != 0;
  packet.moveUp = (flags & 0x04) != 0;
  packet.moveDown = (flags & 0x08) != 0;
  packet.buttons = readUint8(data + 6);
  packet.aimX = readFloat(data + 7);
  packet.aimY = readFloat(data + 11);

  return packet;
}

StateUpdatePacket deserializeStateUpdate(const uint8_t* data, size_t size) {
  assert(size >= 7);  // 1 + 4 + 2 = 7 bytes minimum
  assert(data[0] == static_cast<uint8_t>(PacketType::StateUpdate));

  StateUpdatePacket packet;
  packet.serverTick = readUint32(data + 1);
  uint16_t playerCount = readUint16(data + 5);

  assert(size >= 7 + (playerCount * PLAYER_STATE_BYTES));

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
    player.maxHealth = readFloat(data + offset);
    offset += 4;
    player.shield = readFloat(data + offset);
    offset += 4;
    player.characterId = readUint8(data + offset);
    offset += 1;
    player.flags = readUint8(data + offset);
    offset += 1;
    player.speedMult = readFloat(data + offset);
    offset += 4;
    player.aimX = readFloat(data + offset);
    offset += 4;
    player.aimY = readFloat(data + offset);
    offset += 4;
    player.charge = readUint8(data + offset);
    offset += 1;
    player.chargeSlot = readUint8(data + offset);
    offset += 1;
    for (uint16_t& cooldown : player.cooldowns) {
      cooldown = readUint16(data + offset);
      offset += 2;
    }
    player.resource = readUint8(data + offset);
    offset += 1;

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
    writeUint8(buffer, enemy.flags);
    writeUint8(buffer, enemy.windup);
  }

  assert(buffer.size() == 3 + packet.enemies.size() * ENEMY_STATE_BYTES);
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

// Dead code - EnemyDamagedPacket is never used
// TODO: Remove entirely if combat damage system doesn't need it
// std::vector<uint8_t> serialize(const EnemyDamagedPacket& packet) {
//   std::vector<uint8_t> buffer;
//
//   writeUint8(buffer, static_cast<uint8_t>(packet.type));
//   writeUint32(buffer, packet.enemyId);
//   writeFloat(buffer, packet.newHealth);
//   writeUint32(buffer, packet.attackerId);
//
//   assert(buffer.size() == 13);  // 1 + 4 + 4 + 4 = 13 bytes
//   return buffer;
// }

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

  assert(size >= 3 + (enemyCount * ENEMY_STATE_BYTES));

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
    enemy.flags = readUint8(data + offset);
    offset += 1;
    enemy.windup = readUint8(data + offset);
    offset += 1;

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

// Dead code - EnemyDamagedPacket is never used
// TODO: Remove entirely if combat damage system doesn't need it
// EnemyDamagedPacket deserializeEnemyDamaged(const uint8_t* data, size_t size)
// {
//   assert(size >= 13);
//   assert(data[0] == static_cast<uint8_t>(PacketType::EnemyDamaged));
//
//   EnemyDamagedPacket packet;
//   packet.enemyId = readUint32(data + 1);
//   packet.newHealth = readFloat(data + 5);
//   packet.attackerId = readUint32(data + 9);
//
//   return packet;
// }

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

// Effect packet serialization

std::vector<uint8_t> serialize(const EffectAppliedPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.targetId);
  writeUint8(buffer, packet.isEnemy ? 1 : 0);
  writeUint8(buffer, packet.effectType);
  writeUint8(buffer, packet.stacks);
  writeFloat(buffer, packet.remainingDuration);
  writeUint32(buffer, packet.sourceId);

  assert(buffer.size() == 16);  // 1 + 4 + 1 + 1 + 1 + 4 + 4
  return buffer;
}

std::vector<uint8_t> serialize(const EffectRemovedPacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.targetId);
  writeUint8(buffer, packet.isEnemy ? 1 : 0);
  writeUint8(buffer, packet.effectType);

  assert(buffer.size() == 7);  // 1 + 4 + 1 + 1
  return buffer;
}

std::vector<uint8_t> serialize(const EffectUpdatePacket& packet) {
  std::vector<uint8_t> buffer;

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.targetId);
  writeUint8(buffer, packet.isEnemy ? 1 : 0);
  writeUint16(buffer, static_cast<uint16_t>(packet.effects.size()));

  for (const auto& effect : packet.effects) {
    writeUint8(buffer, effect.effectType);
    writeUint8(buffer, effect.stacks);
    writeFloat(buffer, effect.remainingDuration);
  }

  assert(buffer.size() ==
         8 + packet.effects.size() * 6);  // 1 + 4 + 1 + 2 + (count * 6)
  return buffer;
}

// Effect packet deserialization

EffectAppliedPacket deserializeEffectApplied(const uint8_t* data, size_t size) {
  assert(size >= 16);
  assert(data[0] == static_cast<uint8_t>(PacketType::EffectApplied));

  EffectAppliedPacket packet;
  size_t offset = 1;

  packet.targetId = readUint32(data + offset);
  offset += 4;
  packet.isEnemy = readUint8(data + offset) != 0;
  offset += 1;
  packet.effectType = readUint8(data + offset);
  offset += 1;
  packet.stacks = readUint8(data + offset);
  offset += 1;
  packet.remainingDuration = readFloat(data + offset);
  offset += 4;
  packet.sourceId = readUint32(data + offset);

  return packet;
}

EffectRemovedPacket deserializeEffectRemoved(const uint8_t* data, size_t size) {
  assert(size >= 7);
  assert(data[0] == static_cast<uint8_t>(PacketType::EffectRemoved));

  EffectRemovedPacket packet;
  size_t offset = 1;

  packet.targetId = readUint32(data + offset);
  offset += 4;
  packet.isEnemy = readUint8(data + offset) != 0;
  offset += 1;
  packet.effectType = readUint8(data + offset);

  return packet;
}

EffectUpdatePacket deserializeEffectUpdate(const uint8_t* data, size_t size) {
  assert(size >= 8);
  assert(data[0] == static_cast<uint8_t>(PacketType::EffectUpdate));

  EffectUpdatePacket packet;
  size_t offset = 1;

  packet.targetId = readUint32(data + offset);
  offset += 4;
  packet.isEnemy = readUint8(data + offset) != 0;
  offset += 1;
  uint16_t effectCount = readUint16(data + offset);
  offset += 2;

  assert(size >= 8 + effectCount * 6);

  for (uint16_t i = 0; i < effectCount; i++) {
    NetworkEffect effect;
    effect.effectType = readUint8(data + offset);
    offset += 1;
    effect.stacks = readUint8(data + offset);
    offset += 1;
    effect.remainingDuration = readFloat(data + offset);
    offset += 4;

    packet.effects.push_back(effect);
  }

  return packet;
}

// CharacterSelected packet serialization
std::vector<uint8_t> serialize(const CharacterSelectedPacket& packet) {
  std::vector<uint8_t> buffer;
  buffer.reserve(5);  // 1 (type) + 4 (characterId)

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.characterId);

  assert(buffer.size() == 5 && "CharacterSelectedPacket size mismatch");
  return buffer;
}

CharacterSelectedPacket deserializeCharacterSelected(const uint8_t* data,
                                                     size_t size) {
  assert(size >= 5 && "CharacterSelectedPacket too small");

  CharacterSelectedPacket packet;
  packet.characterId = readUint32(data + 1);

  return packet;
}

// Objective packet serialization

std::vector<uint8_t> serialize(const ObjectiveStatePacket& packet) {
  std::vector<uint8_t> buffer;
  buffer.reserve(39);

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.objectiveId);
  writeUint8(buffer, packet.objectiveType);
  writeUint8(buffer, packet.objectiveState);
  writeFloat(buffer, packet.x);
  writeFloat(buffer, packet.y);
  writeFloat(buffer, packet.radius);
  writeFloat(buffer, packet.progress);
  writeInt32(buffer, packet.enemiesRequired);
  writeInt32(buffer, packet.enemiesKilled);
  writeFloat(buffer, packet.depositX);
  writeFloat(buffer, packet.depositY);

  assert(buffer.size() == 39);  // 1 + 4 + 1 + 1 + 4*6 + 4*2
  return buffer;
}

std::vector<uint8_t> serialize(const ObjectiveInteractPacket& packet) {
  std::vector<uint8_t> buffer;
  buffer.reserve(5);

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint32(buffer, packet.objectiveId);

  assert(buffer.size() == 5);  // 1 + 4
  return buffer;
}

// Objective packet deserialization

ObjectiveStatePacket deserializeObjectiveState(const uint8_t* data,
                                               size_t size) {
  assert(size >= 39 && "ObjectiveStatePacket too small");
  assert(data[0] == static_cast<uint8_t>(PacketType::ObjectiveState));

  ObjectiveStatePacket packet;
  size_t offset = 1;

  packet.objectiveId = readUint32(data + offset);
  offset += 4;
  packet.objectiveType = readUint8(data + offset);
  offset += 1;
  packet.objectiveState = readUint8(data + offset);
  offset += 1;
  packet.x = readFloat(data + offset);
  offset += 4;
  packet.y = readFloat(data + offset);
  offset += 4;
  packet.radius = readFloat(data + offset);
  offset += 4;
  packet.progress = readFloat(data + offset);
  offset += 4;
  packet.enemiesRequired = readInt32(data + offset);
  offset += 4;
  packet.enemiesKilled = readInt32(data + offset);
  offset += 4;
  packet.depositX = readFloat(data + offset);
  offset += 4;
  packet.depositY = readFloat(data + offset);

  return packet;
}

ObjectiveInteractPacket deserializeObjectiveInteract(const uint8_t* data,
                                                     size_t size) {
  assert(size >= 5 && "ObjectiveInteractPacket too small");
  assert(data[0] == static_cast<uint8_t>(PacketType::ObjectiveInteract));

  ObjectiveInteractPacket packet;
  packet.objectiveId = readUint32(data + 1);

  return packet;
}

std::vector<uint8_t> serialize(const ShipLocationPacket& packet) {
  std::vector<uint8_t> buffer;
  buffer.reserve(9);

  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeFloat(buffer, packet.x);
  writeFloat(buffer, packet.y);

  assert(buffer.size() == 9);  // 1 + 4 + 4
  return buffer;
}

ShipLocationPacket deserializeShipLocation(const uint8_t* data, size_t size) {
  assert(size >= 9 && "ShipLocationPacket too small");
  assert(data[0] == static_cast<uint8_t>(PacketType::ShipLocation));

  ShipLocationPacket packet;
  packet.x = readFloat(data + 1);
  packet.y = readFloat(data + 5);

  return packet;
}

// ---------------------------------------------------------------------------
// Arena packets
// ---------------------------------------------------------------------------

std::vector<uint8_t> serialize(const GameEventPacket& packet) {
  std::vector<uint8_t> buffer;
  buffer.reserve(37);
  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint8(buffer, static_cast<uint8_t>(packet.kind));
  writeUint32(buffer, packet.sourceId);
  writeUint8(buffer, packet.sourceIsEnemy ? 1 : 0);
  writeFloat(buffer, packet.x);
  writeFloat(buffer, packet.y);
  writeFloat(buffer, packet.x2);
  writeFloat(buffer, packet.y2);
  writeFloat(buffer, packet.value);
  writeFloat(buffer, packet.value2);
  writeUint8(buffer, packet.param);
  writeUint8(buffer, packet.param2);
  writeUint32(buffer, packet.extraId);
  assert(buffer.size() == 37);
  return buffer;
}

GameEventPacket deserializeGameEvent(const uint8_t* data, size_t size) {
  assert(size >= 37);
  assert(data[0] == static_cast<uint8_t>(PacketType::GameEvent));
  GameEventPacket packet;
  packet.kind = static_cast<GameEventKind>(readUint8(data + 1));
  packet.sourceId = readUint32(data + 2);
  packet.sourceIsEnemy = readUint8(data + 6) != 0;
  packet.x = readFloat(data + 7);
  packet.y = readFloat(data + 11);
  packet.x2 = readFloat(data + 15);
  packet.y2 = readFloat(data + 19);
  packet.value = readFloat(data + 23);
  packet.value2 = readFloat(data + 27);
  packet.param = readUint8(data + 31);
  packet.param2 = readUint8(data + 32);
  packet.extraId = readUint32(data + 33);
  return packet;
}

static constexpr size_t MATCH_HEADER_BYTES = 36;
static constexpr size_t MATCH_PLAYER_BYTES = 31;

std::vector<uint8_t> serialize(const MatchStatePacket& packet) {
  std::vector<uint8_t> buffer;
  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint8(buffer, packet.phase);
  writeUint8(buffer, packet.round);
  writeUint8(buffer, packet.roundCount);
  writeFloat(buffer, packet.timeRemaining);
  writeFloat(buffer, packet.shipHealth);
  writeFloat(buffer, packet.shipMaxHealth);
  writeFloat(buffer, packet.shipX);
  writeFloat(buffer, packet.shipY);
  writeFloat(buffer, packet.zoneRadius);
  writeUint16(buffer, packet.scrapBanked);
  writeUint8(buffer, packet.turretsOnline);
  writeUint32(buffer, packet.bossId);
  assert(packet.players.size() <= 255);
  writeUint8(buffer, static_cast<uint8_t>(packet.players.size()));
  assert(buffer.size() == MATCH_HEADER_BYTES);

  for (const auto& p : packet.players) {
    writeUint32(buffer, p.playerId);
    writeUint8(buffer, p.characterId);
    writeInt32(buffer, p.score);
    writeUint16(buffer, p.kills);
    writeUint8(buffer, p.deaths);
    writeUint8(buffer, p.ready);
    writeUint8(buffer, p.coreGenus);
    for (uint8_t v : p.treeGenus) writeUint8(buffer, v);
    for (uint8_t v : p.treeLevel) writeUint8(buffer, v);
    for (uint8_t v : p.inventory) writeUint8(buffer, v);
    writeUint8(buffer, p.carryingScrap);
    writeFloat(buffer, p.reviveProgress);
  }
  assert(buffer.size() ==
         MATCH_HEADER_BYTES + packet.players.size() * MATCH_PLAYER_BYTES);
  return buffer;
}

MatchStatePacket deserializeMatchState(const uint8_t* data, size_t size) {
  assert(size >= MATCH_HEADER_BYTES);
  assert(data[0] == static_cast<uint8_t>(PacketType::MatchState));
  MatchStatePacket packet;
  packet.phase = readUint8(data + 1);
  packet.round = readUint8(data + 2);
  packet.roundCount = readUint8(data + 3);
  packet.timeRemaining = readFloat(data + 4);
  packet.shipHealth = readFloat(data + 8);
  packet.shipMaxHealth = readFloat(data + 12);
  packet.shipX = readFloat(data + 16);
  packet.shipY = readFloat(data + 20);
  packet.zoneRadius = readFloat(data + 24);
  packet.scrapBanked = readUint16(data + 28);
  packet.turretsOnline = readUint8(data + 30);
  packet.bossId = readUint32(data + 31);
  uint8_t count = readUint8(data + 35);
  assert(size >= MATCH_HEADER_BYTES + count * MATCH_PLAYER_BYTES);

  size_t offset = MATCH_HEADER_BYTES;
  for (uint8_t i = 0; i < count; ++i) {
    MatchPlayerState p;
    p.playerId = readUint32(data + offset);
    offset += 4;
    p.characterId = readUint8(data + offset);
    offset += 1;
    p.score = readInt32(data + offset);
    offset += 4;
    p.kills = readUint16(data + offset);
    offset += 2;
    p.deaths = readUint8(data + offset);
    offset += 1;
    p.ready = readUint8(data + offset);
    offset += 1;
    p.coreGenus = readUint8(data + offset);
    offset += 1;
    for (uint8_t& v : p.treeGenus) v = readUint8(data + offset++);
    for (uint8_t& v : p.treeLevel) v = readUint8(data + offset++);
    for (uint8_t& v : p.inventory) v = readUint8(data + offset++);
    p.carryingScrap = readUint8(data + offset);
    offset += 1;
    p.reviveProgress = readFloat(data + offset);
    offset += 4;
    packet.players.push_back(p);
  }
  return packet;
}

std::vector<uint8_t> serialize(const InfusionSlotPacket& packet) {
  std::vector<uint8_t> buffer;
  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint8(buffer, packet.genus);
  writeUint8(buffer, packet.slot);
  return buffer;
}

InfusionSlotPacket deserializeInfusionSlot(const uint8_t* data, size_t size) {
  assert(size >= 3);
  assert(data[0] == static_cast<uint8_t>(PacketType::InfusionSlot));
  InfusionSlotPacket packet;
  packet.genus = readUint8(data + 1);
  packet.slot = readUint8(data + 2);
  return packet;
}

std::vector<uint8_t> serialize(const PlayerReadyPacket& packet) {
  std::vector<uint8_t> buffer;
  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint8(buffer, packet.ready);
  return buffer;
}

PlayerReadyPacket deserializePlayerReady(const uint8_t* data, size_t size) {
  assert(size >= 2);
  assert(data[0] == static_cast<uint8_t>(PacketType::PlayerReady));
  PlayerReadyPacket packet;
  packet.ready = readUint8(data + 1);
  return packet;
}

std::vector<uint8_t> serialize(const DebugCommandPacket& packet) {
  std::vector<uint8_t> buffer;
  writeUint8(buffer, static_cast<uint8_t>(packet.type));
  writeUint8(buffer, packet.command);
  writeUint8(buffer, packet.arg);
  return buffer;
}

DebugCommandPacket deserializeDebugCommand(const uint8_t* data, size_t size) {
  assert(size >= 3);
  assert(data[0] == static_cast<uint8_t>(PacketType::DebugCommand));
  DebugCommandPacket packet;
  packet.command = readUint8(data + 1);
  packet.arg = readUint8(data + 2);
  return packet;
}
