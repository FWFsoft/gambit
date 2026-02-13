#include "transport/INetworkTransport.h"

// StubTransport: A no-op transport for WASM builds without networking
// Used for single-player/offline mode in the browser MVP

class StubTransport : public INetworkTransport {
 public:
  StubTransport() = default;
  ~StubTransport() override = default;

  bool connect(const std::string& /*host*/, uint16_t /*port*/) override {
    // No networking in stub mode - always "connected" locally
    return true;
  }

  void disconnect() override {
    // No-op
  }

  void send(const uint8_t* /*data*/, size_t /*length*/,
            bool /*reliable*/) override {
    // No-op - messages go nowhere
  }

  bool poll(TransportEvent& /*event*/) override {
    // No events in stub mode
    return false;
  }

  bool isConnected() const override {
    // Always report connected so game logic runs
    return true;
  }
};

// Factory function for creating stub transport
std::unique_ptr<INetworkTransport> createStubTransport() {
  return std::make_unique<StubTransport>();
}
