#pragma once

#include <memory>

#include "transport/INetworkTransport.h"

// Factory function for creating a stub (no-op) transport
// Used for WASM builds in offline/single-player mode
std::unique_ptr<INetworkTransport> createStubTransport();
