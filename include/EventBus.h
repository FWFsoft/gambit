#pragma once

#include <SDL2/SDL.h>
#include <enet/enet.h>

#include <cassert>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

// Core Events

struct UpdateEvent {
  float deltaTime;  // Always 16.67ms for fixed timestep
  uint64_t frameNumber;
};

struct RenderEvent {
  float interpolation;  // 0.0-1.0 for sub-frame interpolation
};

struct SwapBuffersEvent {};

struct KeyDownEvent {
  SDL_Keycode key;
};

struct KeyUpEvent {
  SDL_Keycode key;
};

struct LocalInputEvent {
  bool moveLeft;
  bool moveRight;
  bool moveUp;
  bool moveDown;
  uint32_t inputSequence;
};

struct AttackInputEvent {};

struct ToggleMuteEvent {};

struct NetworkPacketReceivedEvent {
  ENetPeer* peer;  // nullptr on client
  uint8_t* data;
  size_t size;
};

struct ClientConnectedEvent {
  ENetPeer* peer;
  uint32_t clientId;
};

struct ClientDisconnectedEvent {
  uint32_t clientId;
};

// Global singleton event bus for pub-sub architecture
class EventBus {
 public:
  static EventBus& instance() {
    static EventBus instance;
    return instance;
  }

  EventBus(const EventBus&) = delete;
  EventBus& operator=(const EventBus&) = delete;

  template <typename EventType>
  void subscribe(std::function<void(const EventType&)> handler) {
    auto typeIndex = std::type_index(typeid(EventType));

    // Wrap typed handler to match void* signature
    auto wrapper = [handler](const void* eventPtr) {
      const EventType* event = static_cast<const EventType*>(eventPtr);
      handler(*event);
    };

    handlers[typeIndex].push_back(wrapper);
  }

  template <typename EventType>
  void publish(const EventType& event) {
    auto typeIndex = std::type_index(typeid(EventType));

    auto it = handlers.find(typeIndex);
    if (it != handlers.end()) {
      for (auto& handler : it->second) {
        handler(&event);
      }
    }
  }

  void clear() { handlers.clear(); }

 private:
  EventBus() = default;

  std::unordered_map<std::type_index,
                     std::vector<std::function<void(const void*)>>>
      handlers;
};
