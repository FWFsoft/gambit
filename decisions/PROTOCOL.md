# Network Protocol Serialization Decision

**Date:** 2024-12-22
**Status:** Accepted
**Decision Makers:** Core team

## Context

We need a network serialization format for the Gambit game engine to transmit game state between clients and server at 60 FPS. The key requirements are:

- Efficient bandwidth usage (targeting ~55 Kbps per client)
- Fast serialization/deserialization (must fit within 16ms frame budget)
- Cross-platform compatibility (little-endian/big-endian)
- Type safety to prevent bugs
- Maintainability for future protocol changes

## Decision

**We chose manual binary serialization** for the initial implementation (Phases 1-6).

Our current protocol uses hand-written little-endian serialization with helper functions like `writeUint32()`, `writeFloat()`, `readUint32()`, etc.

## Alternatives Considered

### 1. Protocol Buffers (protobuf)

**What it is:** Google's language-neutral, platform-neutral serialization format.

**How it works:**
```protobuf
// game.proto
syntax = "proto3";

message ClientInputPacket {
    uint32 input_sequence = 1;
    bool move_left = 2;
    bool move_right = 3;
    bool move_up = 4;
    bool move_down = 5;
}
```

Code generation creates serialization methods automatically.

**Pros:**
- Auto-generated serialization code (no manual format munging)
- Built-in versioning (backward/forward compatibility)
- Schema is self-documenting
- Cross-language support (if we add web client, tools, etc.)
- Battle-tested by Google
- Simpler API than manual

**Cons:**
- Larger payloads (~10-15% overhead for small messages)
- Requires protoc compiler build step
- Not as fast as manual for tiny packets
- External dependency

**Packet size comparison:**
- ClientInput: 6 bytes (manual) → ~10 bytes (protobuf) = **+67% overhead**
- StateUpdate (4 players): 114 bytes (manual) → ~130 bytes (protobuf) = **+14% overhead**

**Bandwidth impact:** ~55 Kbps → ~62 Kbps per client (**+7 Kbps / +13%**)

### 2. FlatBuffers

**What it is:** Google's zero-copy serialization library, designed for games.

**Used by:** Unity, Unreal Engine, Cocos2d-x

**How it works:**
```flatbuffers
// game.fbs
table ClientInputPacket {
    input_sequence: uint32;
    move_left: bool;
    move_right: bool;
    move_up: bool;
    move_down: bool;
}
```

**Pros:**
- **Zero-copy deserialization** - can read directly from network buffer without parsing
- Faster than protobuf for reading (no intermediate object creation)
- Designed specifically for games and real-time systems
- Supports versioning and schema evolution
- Smaller payloads than protobuf (closer to manual)
- Industry standard for game networking

**Cons:**
- Slightly larger payloads than protobuf (~8-12 bytes for ClientInput)
- More complex API than protobuf
- Requires flatc compiler build step
- Less familiar than protobuf

**Packet size comparison:**
- ClientInput: 6 bytes (manual) → ~9 bytes (FlatBuffers) = **+50% overhead**
- StateUpdate (4 players): 114 bytes (manual) → ~125 bytes (FlatBuffers) = **+10% overhead**

**Performance:** Zero-copy makes reads faster, but writes are similar to protobuf.

### 3. Cap'n Proto

**What it is:** Successor to protobuf by protobuf's original author.

**Pros:**
- Zero-copy like FlatBuffers
- Even better performance characteristics
- No encoding/decoding step at all

**Cons:**
- Less widely adopted than FlatBuffers or protobuf
- Smaller community/ecosystem
- Overkill for our use case

### 4. Manual Binary Serialization (Our Choice)

**What it is:** Hand-written binary encoding with explicit byte layout.

**Example:**
```cpp
// ClientInputPacket: [Type:1][InputSequence:4][Flags:1] = 6 bytes
void writeUint32(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(value & 0xFF);           // LSB
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 24) & 0xFF);   // MSB
}
```

**Pros:**
- **Maximum control** over every byte
- **Smallest possible packets** (6 bytes for ClientInput, 114 bytes for 4-player StateUpdate)
- **Zero external dependencies** - just standard C++
- **Deterministic performance** - no hidden allocations or copies
- **Educational value** - understanding low-level networking

**Cons:**
- **Tedious and error-prone** - manual endianness handling, alignment, pointer arithmetic
- **Hard to evolve** - adding a field requires rewriting serialization/deserialization
- **No versioning support** - protocol changes break compatibility immediately
- **Manual testing required** - need unit tests for round-trip correctness (we have 4 tests)
- **"Format munging"** - lots of bit-shifting, masking, byte packing
- **Maintenance burden** - every protocol change touches multiple functions

## Comparison Matrix

| Feature | Manual | Protobuf | FlatBuffers | Cap'n Proto |
|---------|--------|----------|-------------|-------------|
| **Packet Size** | ★★★★★ (smallest) | ★★★☆☆ (+67% small, +14% large) | ★★★★☆ (+50% small, +10% large) | ★★★★☆ |
| **Serialization Speed** | ★★★★☆ | ★★★☆☆ | ★★★★☆ | ★★★★★ |
| **Deserialization Speed** | ★★★★☆ | ★★☆☆☆ | ★★★★★ (zero-copy) | ★★★★★ |
| **Ease of Use** | ★☆☆☆☆ | ★★★★☆ | ★★★☆☆ | ★★★☆☆ |
| **Maintainability** | ★☆☆☆☆ | ★★★★★ | ★★★★☆ | ★★★★☆ |
| **Versioning** | ★☆☆☆☆ (none) | ★★★★★ | ★★★★☆ | ★★★★★ |
| **Dependencies** | ★★★★★ (none) | ★★★☆☆ | ★★★☆☆ | ★★☆☆☆ |
| **Industry Adoption (Games)** | ★★★☆☆ | ★★★☆☆ | ★★★★★ | ★★☆☆☆ |
| **Cross-Language** | ★☆☆☆☆ | ★★★★★ | ★★★★★ | ★★★★☆ |

## Rationale

We chose **manual binary serialization** for the following reasons:

### For the MVP (Current State)

1. **Learning opportunity**: Understanding low-level networking is valuable for the team
2. **Simplicity of setup**: No build system changes, no external dependencies
3. **Performance baseline**: Establishes the absolute minimum bandwidth requirements
4. **Full control**: Perfect for rapid prototyping and experimentation

### Known Trade-offs We're Accepting

1. **Maintenance burden**: We acknowledge that adding/modifying packets is tedious
2. **No versioning**: Protocol changes require coordinated client/server updates
3. **Error-prone**: We mitigate with unit tests (currently 4 tests covering all packet types)
4. **Hard to debug**: Binary blobs are harder to inspect than text/schema formats

## When to Reconsider

We should **strongly consider switching** to FlatBuffers or Protobuf when:

### Triggers for Re-evaluation

1. **Protocol churn**: If we're adding/changing packets frequently (> 1/week)
2. **Maintenance pain**: If serialization bugs consume significant debugging time
3. **Versioning needs**: When we need to support multiple client/server versions
4. **Cross-language**: If we add web clients, tools, or server-side scripting
5. **Team growth**: New contributors struggle with manual serialization
6. **Post-MVP**: After Phase 9 (Polish & Testing) when we have a stable baseline

### Recommended Migration Path

**Phase 1:** Evaluate based on pain points
- If maintenance is painful → **Protobuf** (simplest migration)
- If performance matters → **FlatBuffers** (industry standard for games)

**Phase 2:** Parallel implementation
- Keep manual serialization working
- Add protobuf/FlatBuffers alongside
- A/B test bandwidth and performance

**Phase 3:** Cutover
- Switch default to new format
- Remove manual serialization code
- Update documentation

## Bandwidth Analysis

### Current Manual Serialization

**Per Client (60 FPS):**
- Outgoing: 60 ClientInputPackets/sec × 6 bytes = **360 bytes/sec**
- Incoming: 60 StateUpdatePackets/sec × 114 bytes = **6,840 bytes/sec**
- **Total: 7,200 bytes/sec = ~57 Kbps**

**4-Player Server:**
- Incoming: 4 clients × 360 bytes/sec = **1,440 bytes/sec**
- Outgoing: 4 clients × 6,840 bytes/sec = **27,360 bytes/sec**
- **Total: 28,800 bytes/sec = ~230 Kbps**

### Protobuf (Estimated)

**Per Client (60 FPS):**
- Outgoing: 60 × ~10 bytes = **600 bytes/sec**
- Incoming: 60 × ~130 bytes = **7,800 bytes/sec**
- **Total: 8,400 bytes/sec = ~67 Kbps** (+10 Kbps / +17%)

### FlatBuffers (Estimated)

**Per Client (60 FPS):**
- Outgoing: 60 × ~9 bytes = **540 bytes/sec**
- Incoming: 60 × ~125 bytes = **7,500 bytes/sec**
- **Total: 8,040 bytes/sec = ~64 Kbps** (+7 Kbps / +12%)

### Is the Overhead Worth Avoiding?

**No.** The bandwidth difference is negligible on modern networks:
- +7-10 Kbps is **< 0.1%** of a 100 Mbps connection
- Even on 4G LTE (~10 Mbps), this is **< 0.1%**
- The maintainability win far outweighs the bandwidth cost

## Code Examples

### Current Manual Approach

```cpp
// Serialization (in NetworkProtocol.cpp)
std::vector<uint8_t> serialize(const ClientInputPacket& packet) {
    std::vector<uint8_t> buffer;
    writeUint8(buffer, static_cast<uint8_t>(packet.type));
    writeUint32(buffer, packet.inputSequence);

    uint8_t flags = 0;
    if (packet.moveLeft)  flags |= 0x01;
    if (packet.moveRight) flags |= 0x02;
    if (packet.moveUp)    flags |= 0x04;
    if (packet.moveDown)  flags |= 0x08;
    writeUint8(buffer, flags);

    assert(buffer.size() == 6);  // Tiger Style: verify size
    return buffer;
}

// Deserialization
ClientInputPacket deserializeClientInput(const uint8_t* data, size_t size) {
    assert(size >= 6);  // Tiger Style: bounds check
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
```

### Equivalent Protobuf Approach

```protobuf
// game.proto
syntax = "proto3";

message ClientInputPacket {
    uint32 input_sequence = 1;
    bool move_left = 2;
    bool move_right = 3;
    bool move_up = 4;
    bool move_down = 5;
}
```

```cpp
// Usage (auto-generated code handles serialization)
// Serialize
ClientInputPacket packet;
packet.set_input_sequence(42);
packet.set_move_left(true);
std::string data = packet.SerializeAsString();

// Deserialize
ClientInputPacket received;
received.ParseFromString(data);
bool moved_left = received.move_left();
```

## Current Implementation Files

- `include/NetworkProtocol.h` - Packet definitions and function declarations
- `src/NetworkProtocol.cpp` - Manual serialization/deserialization (~185 lines)
- `src/test_main.cpp` - Unit tests for round-trip correctness (4 tests, all passing)

## Lessons Learned (To Be Updated)

*This section will be updated as we gain experience with the manual protocol.*

- 2024-12-22: Initial implementation complete, unit tests passing
- Caught bug in PlayerJoinedPacket size (was 7 bytes in plan, actually 8 bytes due to RGB taking 3 bytes)

## References

- [Protocol Buffers Documentation](https://protobuf.dev/)
- [FlatBuffers Documentation](https://google.github.io/flatbuffers/)
- [Cap'n Proto](https://capnproto.org/)
- [Game Networking Resources](https://gafferongames.com/) - Glenn Fiedler's articles on game networking
- [Valve's Source Engine Networking](https://developer.valvesoftware.com/wiki/Source_Multiplayer_Networking) - uses custom binary protocol
- [Unreal Engine Replication](https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/Networking/Actors/Properties/) - uses FlatBuffers-like approach

## Future Considerations

If we migrate to a schema-based format, we should:

1. **Benchmark first**: Measure actual bandwidth and CPU usage with real gameplay
2. **Consider compression**: Add LZ4/Snappy on top of any format for further savings
3. **Version strategy**: Plan for multiple protocol versions in production
4. **Tooling**: Build packet inspectors, replay systems, debug visualizers
5. **Delta compression**: Only send changed state (more important than format choice)

---

**Next Review Date:** After Phase 9 (Polish & Testing) completion
