#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

// InMemoryChannel: Shared bidirectional message queue for embedded server mode
// Used by InMemoryTransport (client) and InMemoryServerTransport (server)
// to communicate without actual network sockets.
//
// Thread-safety: Uses mutex for queue access. Safe for use when client and
// server tick on the same thread (synchronous) or different threads.

struct InMemoryChannel {
  // Messages from server to client
  std::queue<std::vector<uint8_t>> serverToClient;
  std::mutex serverToClientMutex;

  // Messages from client to server
  std::queue<std::vector<uint8_t>> clientToServer;
  std::mutex clientToServerMutex;

  // Connection state
  bool connected = false;
  bool clientWantsConnect = false;
  bool clientWantsDisconnect = false;

  // Push a message from client to server
  void pushClientToServer(const uint8_t* data, size_t length) {
    std::lock_guard<std::mutex> lock(clientToServerMutex);
    clientToServer.emplace(data, data + length);
  }

  // Push a message from server to client
  void pushServerToClient(const uint8_t* data, size_t length) {
    std::lock_guard<std::mutex> lock(serverToClientMutex);
    serverToClient.emplace(data, data + length);
  }

  // Pop a message destined for client (returns empty if none)
  std::vector<uint8_t> popServerToClient() {
    std::lock_guard<std::mutex> lock(serverToClientMutex);
    if (serverToClient.empty()) {
      return {};
    }
    auto msg = std::move(serverToClient.front());
    serverToClient.pop();
    return msg;
  }

  // Pop a message destined for server (returns empty if none)
  std::vector<uint8_t> popClientToServer() {
    std::lock_guard<std::mutex> lock(clientToServerMutex);
    if (clientToServer.empty()) {
      return {};
    }
    auto msg = std::move(clientToServer.front());
    clientToServer.pop();
    return msg;
  }

  // Check if there are messages for the client
  bool hasServerToClient() {
    std::lock_guard<std::mutex> lock(serverToClientMutex);
    return !serverToClient.empty();
  }

  // Check if there are messages for the server
  bool hasClientToServer() {
    std::lock_guard<std::mutex> lock(clientToServerMutex);
    return !clientToServer.empty();
  }
};

// Factory for creating a shared channel
inline std::shared_ptr<InMemoryChannel> createInMemoryChannel() {
  return std::make_shared<InMemoryChannel>();
}
