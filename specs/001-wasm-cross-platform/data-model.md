# Data Model: WebAssembly Cross-Platform Support

**Feature**: 001-wasm-cross-platform
**Date**: 2026-01-29

## New Entities

### INetworkTransport (Interface)

The core abstraction enabling cross-platform networking. Replaces direct ENet usage throughout the codebase.

**Interface**:
- `connect(host, port) -> bool` — Establish connection
- `disconnect()` — Close connection gracefully
- `send(data, length, reliable) -> void` — Send binary packet
- `poll(event) -> bool` — Non-blocking receive; fills event struct
- `isConnected() -> bool` — Connection state query

**Implementations**:
| Backend | Platform | Transport | Use Case |
|---------|----------|-----------|----------|
| ENetTransport | Native | UDP | Networked multiplayer (current behavior) |
| WebSocketTransport | WASM | TCP/WebSocket | Browser-to-server networking |
| InMemoryTransport | Both | Shared queue | Embedded server mode |

### IServerTransport (Interface)

Server-side transport abstraction for accepting multiple client connections.

**Interface**:
- `initialize(address, port) -> bool` — Bind and listen
- `poll(event) -> bool` — Non-blocking accept/receive/disconnect
- `send(clientId, data, length) -> void` — Send to specific client
- `broadcast(data, length) -> void` — Send to all clients
- `stop()` — Shutdown

**Implementations**:
| Backend | Protocol | Use Case |
|---------|----------|----------|
| ENetServerTransport | UDP/ENet | Native clients |
| WebSocketServerTransport | TCP/WebSocket | Browser clients |
| InMemoryServerTransport | Shared queue | Embedded mode (single client) |

### NetworkEvent (Struct)

Unified event returned by transport `poll()` methods.

**Fields**:
- `type: EventType` — CONNECT, RECEIVE, DISCONNECT
- `clientId: uint32_t` — Identifies the client
- `data: const uint8_t*` — Received packet data (RECEIVE only)
- `dataLength: size_t` — Packet size (RECEIVE only)

### InMemoryChannel (Internal)

Shared message queue pair for embedded server mode.

**Fields**:
- `serverToClient: std::deque<std::vector<uint8_t>>` — Messages from server to client
- `clientToServer: std::deque<std::vector<uint8_t>>` — Messages from client to server
- `connected: bool` — Connection state flag

**Relationships**:
- One `InMemoryChannel` connects exactly one `InMemoryTransport` (client-side) to one `InMemoryServerTransport` (server-side)
- Both sides reference the same channel instance

### GameSession (Orchestrator)

Coordinates server and client logic within a single process for embedded mode.

**Fields**:
- `serverState: ServerGameState` — Server-side game state
- `clientPrediction: ClientPrediction` — Client-side prediction
- `channel: InMemoryChannel` — Shared communication channel
- `mode: SessionMode` — EMBEDDED or NETWORKED

**State Transitions**:
```
INITIALIZING → RUNNING → STOPPING → STOPPED
```

## Modified Entities

### NetworkServer

**Changes**:
- Currently takes raw ENet host. Will be refactored to take `IServerTransport*` instead.
- `poll()` and `run()` delegate to transport's `poll()` method.
- Event publishing (`ClientConnectedEvent`, `NetworkPacketReceivedEvent`, `ClientDisconnectedEvent`) remains unchanged.

### NetworkClient

**Changes**:
- Currently takes raw ENet host/peer. Will be refactored to take `INetworkTransport*` instead.
- `connect()`, `send()`, `run()` delegate to transport methods.
- Event publishing remains unchanged.

### GameLoop

**Changes**:
- Extract single-iteration method `tick()` from `run()`.
- Emscripten build uses `emscripten_set_main_loop_arg(&tick, ...)`.
- Native build continues using `while(running) { tick(); }`.

### Build Target Configuration

**New CMake targets**:
| Target | Platform | Description |
|--------|----------|-------------|
| Server | Native | Game server (ENet + WebSocket) |
| Client | Native | Game client (ENet transport) |
| WasmClient | WASM | Browser client (WebSocket + embedded server) |
| HeadlessClient | Native | Headless testing (unchanged) |

## Entity Relationship Diagram

```
GameSession (embedded mode)
├── ServerGameState
│   └── IServerTransport (InMemoryServerTransport)
│       └── InMemoryChannel ←──── shared ────→ InMemoryTransport
├── ClientPrediction                                │
│   └── INetworkTransport (InMemoryTransport) ──────┘
└── GameLoop (ticks both sides)

Networked Mode (native)
├── Server process
│   └── NetworkServer
│       ├── IServerTransport (ENetServerTransport)
│       └── IServerTransport (WebSocketServerTransport)
└── Client process
    └── NetworkClient
        └── INetworkTransport (ENetTransport)

Networked Mode (browser)
├── Server process (native)
│   └── NetworkServer
│       └── IServerTransport (WebSocketServerTransport)
└── Browser client
    └── NetworkClient
        └── INetworkTransport (WebSocketTransport)
```
