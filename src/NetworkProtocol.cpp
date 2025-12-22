#include "NetworkProtocol.h"
#include <cstring>

// Helper functions for binary I/O (little-endian)

void writeUint32(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(value & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 24) & 0xFF);
}

void writeFloat(std::vector<uint8_t>& buffer, float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(float));
    writeUint32(buffer, bits);
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

float readFloat(const uint8_t* data) {
    uint32_t bits = readUint32(data);
    float value;
    std::memcpy(&value, &bits, sizeof(float));
    return value;
}

uint8_t readUint8(const uint8_t* data) {
    return data[0];
}

// Serialization

std::vector<uint8_t> serialize(const ClientInputPacket& packet) {
    std::vector<uint8_t> buffer;

    writeUint8(buffer, static_cast<uint8_t>(packet.type));
    writeUint32(buffer, packet.inputSequence);

    // Pack movement flags into single byte
    uint8_t flags = 0;
    if (packet.moveLeft)  flags |= 0x01;
    if (packet.moveRight) flags |= 0x02;
    if (packet.moveUp)    flags |= 0x04;
    if (packet.moveDown)  flags |= 0x08;
    writeUint8(buffer, flags);

    assert(buffer.size() == 6);  // 1 + 4 + 1 = 6 bytes
    return buffer;
}

std::vector<uint8_t> serialize(const StateUpdatePacket& packet) {
    std::vector<uint8_t> buffer;

    writeUint8(buffer, static_cast<uint8_t>(packet.type));
    writeUint32(buffer, packet.serverTick);
    writeUint8(buffer, static_cast<uint8_t>(packet.players.size()));

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
    }

    // 1 + 4 + 1 + (playerCount * 27) bytes
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

// Deserialization

ClientInputPacket deserializeClientInput(const uint8_t* data, size_t size) {
    assert(size >= 6);
    assert(data[0] == static_cast<uint8_t>(PacketType::ClientInput));

    ClientInputPacket packet;
    packet.inputSequence = readUint32(data + 1);

    uint8_t flags = readUint8(data + 5);
    packet.moveLeft  = (flags & 0x01) != 0;
    packet.moveRight = (flags & 0x02) != 0;
    packet.moveUp    = (flags & 0x04) != 0;
    packet.moveDown  = (flags & 0x08) != 0;

    return packet;
}

StateUpdatePacket deserializeStateUpdate(const uint8_t* data, size_t size) {
    assert(size >= 6);
    assert(data[0] == static_cast<uint8_t>(PacketType::StateUpdate));

    StateUpdatePacket packet;
    packet.serverTick = readUint32(data + 1);
    uint8_t playerCount = readUint8(data + 5);

    assert(size >= 6 + (playerCount * 27));

    size_t offset = 6;
    for (uint8_t i = 0; i < playerCount; ++i) {
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
