// Transport Layer Interface Contract
// Feature: 001-wasm-cross-platform
//
// This is a design contract, not compilable code.
// It defines the interfaces that implementations must satisfy.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// --- Events returned by transport polling ---

enum class TransportEventType {
    CONNECT,     // A client connected (server) or connection established (client)
    RECEIVE,     // A packet was received
    DISCONNECT,  // A client disconnected (server) or connection lost (client)
    NONE         // No event available
};

struct TransportEvent {
    TransportEventType type = TransportEventType::NONE;
    uint32_t clientId = 0;            // Identifies the remote peer
    std::vector<uint8_t> data;        // Received packet data (RECEIVE only)
};

// --- Client-side transport interface ---

class INetworkTransport {
  public:
    virtual ~INetworkTransport() = default;

    // Connect to a remote server. Returns true on success.
    virtual bool connect(const std::string& host, uint16_t port) = 0;

    // Disconnect from the server.
    virtual void disconnect() = 0;

    // Send a binary packet to the server.
    // |reliable| hints whether the transport should guarantee delivery.
    // (WebSocket and InMemory are always reliable; ENet uses channels.)
    virtual void send(const uint8_t* data, size_t length, bool reliable = true) = 0;

    // Non-blocking poll. Returns true if an event was available.
    virtual bool poll(TransportEvent& event) = 0;

    // Query connection state.
    virtual bool isConnected() const = 0;
};

// --- Server-side transport interface ---

class IServerTransport {
  public:
    virtual ~IServerTransport() = default;

    // Bind to address:port and start listening. Returns true on success.
    virtual bool initialize(const std::string& address, uint16_t port) = 0;

    // Non-blocking poll for connect/receive/disconnect events.
    virtual bool poll(TransportEvent& event) = 0;

    // Send a binary packet to a specific client.
    virtual void send(uint32_t clientId, const uint8_t* data, size_t length) = 0;

    // Broadcast a binary packet to all connected clients.
    virtual void broadcast(const uint8_t* data, size_t length) = 0;

    // Stop listening and disconnect all clients.
    virtual void stop() = 0;
};

// --- In-memory channel for embedded server mode ---

// Shared between InMemoryTransport (client-side) and
// InMemoryServerTransport (server-side).
// Both sides push/pop from opposite ends of a message queue pair.

struct InMemoryChannel {
    // Messages from server to client
    std::vector<std::vector<uint8_t>> serverToClient;

    // Messages from client to server
    std::vector<std::vector<uint8_t>> clientToServer;

    // Connection state
    bool connected = false;
};
