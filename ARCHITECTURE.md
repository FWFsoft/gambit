# Gambit Engine - Architecture

## System Overview

Gambit uses an **event-driven architecture** built around a central `EventBus` that enables decoupled, asynchronous communication between all systems.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                         EventBus                            │
│              (Global Pub-Sub Communication)                 │
└─────────────────────────────────────────────────────────────┘
                            ▲  │
                            │  │ publish/subscribe
                            │  ▼
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   GameLoop   │───▶│ UpdateEvent  │───▶│ All Systems  │
│  (60 FPS)    │    │ RenderEvent  │    │              │
└──────────────┘    └──────────────┘    └──────────────┘

Client:                          Server:
┌──────────────┐                 ┌──────────────────┐
│    Window    │                 │  NetworkServer   │
│ (SDL Events) │                 │   (ENet Poll)    │
└──────┬───────┘                 └────────┬─────────┘
       │                                  │
       ▼ KeyDown/KeyUp                   ▼ ClientConnected/Disconnected
┌──────────────┐                 ┌──────────────────┐
│ InputSystem  │                 │ ServerGameState  │
│ (WASD State) │                 │ (Players Map)    │
└──────┬───────┘                 └────────┬─────────┘
       │                                  │
       ▼ LocalInputEvent                 ▼ StateUpdate
       │                                  │
       │         Network Protocol         │
       └─────────────────┬────────────────┘
                         │
              ┌──────────┴──────────┐
              │  Binary Packets     │
              │ (6-114 bytes)       │
              └─────────────────────┘
```

## Core Components

### EventBus (Central Nervous System)

**Location**: `include/EventBus.h`

Global singleton implementing type-safe publish-subscribe pattern:

- **Subscribe**: `EventBus::instance().subscribe<EventType>(handler)`
- **Publish**: `EventBus::instance().publish(event)`
- **Type Safety**: Uses C++ templates and `std::type_index` for compile-time event type checking
- **Decoupling**: Systems never directly call each other - only through events

**Core Event Types**:
- `UpdateEvent` - Fixed 60 FPS game logic tick
- `RenderEvent` - Variable-rate rendering with interpolation
- `KeyDownEvent`/`KeyUpEvent` - Raw SDL keyboard input
- `LocalInputEvent` - Processed WASD state (published 60x/sec)
- `NetworkPacketReceivedEvent` - Binary network data received
- `ClientConnectedEvent`/`ClientDisconnectedEvent` - Server-side connection lifecycle

### GameLoop (The Heartbeat)

**Location**: `include/GameLoop.h`, `src/GameLoop.cpp`

Fixed 60 FPS game loop that drives all systems:

```cpp
while (running) {
    // 1. Update loop ALWAYS fires at 60 FPS
    EventBus::instance().publish(UpdateEvent{16.67ms, frameNumber++});

    // 2. Render loop (can drop frames if needed)
    EventBus::instance().publish(RenderEvent{interpolation});

    // 3. Sleep to maintain 60 FPS
    SDL_Delay(sleepTime);
}
```

**Design Principles**:
- **Fixed Timestep**: Update always runs at 16.67ms intervals
- **Variable Render**: Render can be skipped if frame budget exceeded
- **Deterministic**: Physics/gameplay always advance at same rate

### InputSystem (Keyboard Handler)

**Location**: `include/InputSystem.h`, `src/InputSystem.cpp`

Converts raw SDL keyboard events into game input:

**Event Flow**:
1. `Window` publishes `KeyDownEvent`/`KeyUpEvent` from SDL
2. `InputSystem` subscribes and updates internal WASD state
3. On `UpdateEvent`, `InputSystem` publishes `LocalInputEvent` with current movement state
4. Input sequence counter increments every frame (used for client-side prediction)

**Why Every Frame?**
- Consistent 60 FPS input sampling
- Enables client-side prediction and reconciliation
- Server receives timestamped inputs for replay protection

### Network Protocol (Binary Communication)

**Location**: `include/NetworkProtocol.h`, `src/NetworkProtocol.cpp`

Efficient binary packet format with 4 packet types:

| Packet Type | Size | Description |
|-------------|------|-------------|
| `ClientInput` | 6 bytes | Client → Server: Movement input with sequence number |
| `StateUpdate` | 6 + 27*N bytes | Server → Clients: Authoritative player states |
| `PlayerJoined` | 8 bytes | Server → Clients: New player connected |
| `PlayerLeft` | 5 bytes | Server → Clients: Player disconnected |

**Serialization**:
- Little-endian byte order
- Helper functions: `writeUint32()`, `writeFloat()`, `readUint32()`, `readFloat()`
- Movement flags packed into single byte (4 booleans → 1 byte)
- Tiger Style: Asserts verify packet sizes and validate types on deserialize

**Example - ClientInputPacket** (6 bytes):
```
[Type:1][InputSequence:4][Flags:1]
Flags = 0x01 (left) | 0x02 (right) | 0x04 (up) | 0x08 (down)
```

### Player Entity (Shared Representation)

**Location**: `include/Player.h`

Common player structure used by both client and server:

```cpp
struct Player {
    uint32_t id;
    float x, y;              // Position (pixels)
    float vx, vy;            // Velocity (pixels/second)
    float health;
    uint8_t r, g, b;         // Color

    uint32_t lastInputSequence;  // Server-only: input validation
    uint32_t lastServerTick;     // Client-only: reconciliation
};
```

**Shared Movement Logic**:

`applyInput()` function is used identically by both client prediction and server authority:
- 8-directional movement with diagonal normalization (×0.707)
- Speed: 200 pixels/second
- World bounds clamping (800×600)
- Identical logic prevents client-server desync

### ServerGameState (Authoritative State)

**Location**: `include/ServerGameState.h`, `src/ServerGameState.cpp`

Server brain that maintains truth and broadcasts to clients:

**State**:
- `std::unordered_map<uint32_t, Player> players` - All connected players
- `std::unordered_map<ENetPeer*, uint32_t> peerToPlayerId` - Peer → Player mapping
- `uint32_t serverTick` - Monotonic server frame counter

**Event Handling**:
1. **ClientConnectedEvent**:
   - Spawns player at random position
   - Assigns color (cycles through Red, Green, Blue, Yellow)
   - Broadcasts `PlayerJoinedPacket` to all clients

2. **ClientDisconnectedEvent**:
   - Removes player from game state
   - Broadcasts `PlayerLeftPacket` to all clients

3. **NetworkPacketReceivedEvent**:
   - Deserializes `ClientInputPacket`
   - Validates input sequence (prevents replay attacks)
   - Applies movement via `applyInput()`

4. **UpdateEvent** (60 FPS):
   - Increments `serverTick`
   - Broadcasts `StateUpdatePacket` with all player states

**Bandwidth**: With 4 players, StateUpdate = 114 bytes @ 60 FPS = ~55 Kbps per client

### NetworkServer (ENet Integration)

**Location**: `include/NetworkServer.h`, `src/NetworkServer.cpp`

Event-driven network layer:

**Key Methods**:
- `poll()`: Non-blocking ENet event processing (called every frame)
- `broadcastPacket(vector<uint8_t>)`: Send binary data to all clients
- `send(ENetPeer*, vector<uint8_t>)`: Send binary data to specific client

**Event Publishing**:
- ENet connect → `ClientConnectedEvent`
- ENet disconnect → `ClientDisconnectedEvent`
- ENet receive → `NetworkPacketReceivedEvent`

## Data Flow Examples

### Player Movement (Full Round Trip)

```
Client Side:
1. User presses 'W'
   → SDL_KEYDOWN event

2. Window::pollEvents()
   → Publishes KeyDownEvent{SDLK_w}

3. InputSystem::onKeyDown()
   → Sets moveUp = true

4. GameLoop publishes UpdateEvent
   → InputSystem::onUpdate()
   → Publishes LocalInputEvent{moveUp=true, inputSequence=42}

5. ClientPrediction (TODO: Phase 6)
   → Applies input locally (instant response)
   → Serializes ClientInputPacket
   → Sends to server

Network:
   ClientInputPacket (6 bytes) →

Server Side:
6. NetworkServer::poll()
   → Receives ENet packet
   → Publishes NetworkPacketReceivedEvent

7. ServerGameState::onNetworkPacketReceived()
   → Deserializes ClientInputPacket
   → Validates input sequence (anti-replay)
   → Calls applyInput() to move player

8. GameLoop publishes UpdateEvent
   → ServerGameState::onUpdate()
   → Increments serverTick
   → Broadcasts StateUpdatePacket with all player states

Network:
   ← StateUpdatePacket (114 bytes for 4 players)

Client Side:
9. NetworkClient::run()
   → Receives ENet packet
   → Publishes NetworkPacketReceivedEvent

10. ClientPrediction (TODO: Phase 6)
    → Deserializes StateUpdatePacket
    → Reconciles predicted position with server truth

11. RemotePlayerInterpolation (TODO: Phase 7)
    → Buffers snapshots for remote players
    → Interpolates between snapshots for smooth movement

12. GameLoop publishes RenderEvent
    → RenderSystem (TODO: Phase 8)
    → Draws local player (predicted position)
    → Draws remote players (interpolated positions)
```

### Player Join Flow

```
1. Client connects to server
   → NetworkServer::poll() receives ENET_EVENT_TYPE_CONNECT
   → Publishes ClientConnectedEvent{peer, clientId}

2. ServerGameState::onClientConnected()
   → Creates Player struct
   → Random spawn position (100-700, 100-500)
   → Assigns color based on player count % 4
   → Stores in players map
   → Broadcasts PlayerJoinedPacket to all clients

3. All clients receive PlayerJoinedPacket
   → NetworkClient::run() publishes NetworkPacketReceivedEvent
   → (Future) Client game state adds remote player to render list
```

## Design Principles

### Tiger Style

- **Assert on Negative Space**: Crash early on invalid state
- **Bounds Checking**: All packet deserialization validates size and type
- **Input Validation**: Server checks input sequence to prevent replay attacks
- **Performance Monitoring**: Future work to assert if FPS drops below 58

### Event-Driven Architecture

- **No Direct Coupling**: Systems never call each other directly
- **Asynchronous Communication**: Events decouple producer from consumer
- **Easy Testing**: Can mock events for unit testing individual systems
- **Scalable**: New systems subscribe without modifying existing code

### Fixed Timestep

- **Deterministic Gameplay**: Physics always advance at same rate
- **Network Prediction**: Client can replay inputs with identical results
- **Decoupled Render**: Rendering can drop frames without affecting gameplay

## Current Status

**Core Architecture** (Implemented):
- ✅ EventBus with type-safe pub-sub
- ✅ GameLoop running at fixed 60 FPS
- ✅ InputSystem capturing WASD input
- ✅ Binary network protocol with 4 packet types
- ✅ Player entity with shared movement logic
- ✅ ServerGameState with player spawning, input processing, state broadcasting
- ✅ ClientPrediction - Instant local response with server reconciliation
- ✅ RemotePlayerInterpolation - Smooth remote player movement
- ✅ RenderSystem - OpenGL-based isometric rendering
- ✅ Window Management - SDL2 integration with ImGui support

**Additional Systems** (Implemented):
- ✅ Combat System - Player health, damage, and respawning
- ✅ Enemy System - Enemy spawning and AI
- ✅ UI System - ImGui-based settings and HUD
- ✅ Music System - Background music with combat layering
- ✅ Tile Rendering - Isometric tile-based maps
- ✅ Item System - Item registry and CSV loading

## File Structure

```
include/
  EventBus.h                 - Global event bus + all event definitions
  GameLoop.h                 - Fixed 60 FPS game loop
  InputSystem.h              - Keyboard input → LocalInputEvent
  NetworkProtocol.h          - Binary packet definitions + serialization
  Player.h                   - Player struct + applyInput()
  ServerGameState.h          - Server authoritative game state
  NetworkServer.h            - ENet server wrapper (event-driven)
  NetworkClient.h            - ENet client wrapper (event-driven)
  Window.h                   - SDL window + event polling

src/
  GameLoop.cpp
  InputSystem.cpp
  NetworkProtocol.cpp
  ServerGameState.cpp
  NetworkServer.cpp
  NetworkClient.cpp
  Window.cpp
  server_main.cpp            - Server entry point
  client_main.cpp            - Client entry point
```

## Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| Update Rate | 60 FPS | Fixed timestep, always fires |
| Render Rate | Up to 60 FPS | Can drop frames if needed |
| Network Tick | 60 Hz | Server broadcasts every frame |
| State Packet Size | 114 bytes | 4 players + header |
| Input Packet Size | 6 bytes | Movement flags + sequence |
| Bandwidth/Client | ~55 Kbps | Receiving state updates |
| Latency (Local) | < 1ms | Input to movement |
| Player Speed | 200 px/s | WASD movement |
| World Size | 800×600 | Hardcoded bounds |

## Next Steps

See DESIGN.md for detailed design requirements and decisions. The core event-driven architecture is in place, and the engine continues to evolve with new gameplay systems and features.
