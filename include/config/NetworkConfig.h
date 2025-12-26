#pragma once

#include <cstddef>

// Network configuration
// Settings for client-server networking

namespace Config {
namespace Network {

// Connection settings
constexpr const char* SERVER_ADDRESS = "127.0.0.1";  // Localhost for clients
constexpr const char* SERVER_BIND_ADDRESS =
    "0.0.0.0";  // All interfaces for server
constexpr int PORT = 1234;

// Server capacity
constexpr size_t MAX_CLIENTS = 32;
constexpr size_t CHANNEL_COUNT = 2;  // Reliable + unreliable

// Timeouts
constexpr int POLL_TIMEOUT_MS = 1000;  // ENet poll timeout

}  // namespace Network
}  // namespace Config
