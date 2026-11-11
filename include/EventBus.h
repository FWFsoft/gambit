#pragma once

#include <SDL2/SDL.h>

#include <cassert>
#include <functional>
#include <memory>
#include <string>
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

struct MouseMoveEvent {
  int x, y;  // window pixels
};

struct MouseButtonEvent {
  uint8_t button;  // SDL_BUTTON_LEFT / SDL_BUTTON_RIGHT / ...
  bool down;
  int x, y;
};

struct LocalInputEvent {
  bool moveLeft;
  bool moveRight;
  bool moveUp;
  bool moveDown;
  uint32_t inputSequence;
  uint8_t buttons = 0;  // InputButtons bitmask (LMB, RMB, Q, SHIFT, E)
  int mouseX = 0;       // cursor in window pixels
  int mouseY = 0;
};

struct AttackInputEvent {};

struct ToggleMuteEvent {};

struct NetworkPacketReceivedEvent {
  uint32_t clientId;  // 0 on client side
  const uint8_t* data;
  size_t size;
};

struct ClientConnectedEvent {
  uint32_t clientId;
};

struct ClientDisconnectedEvent {
  uint32_t clientId;
};

struct ItemPickedUpEvent {
  uint32_t itemId;  // The ItemDefinition ID that was picked up
  uint32_t quantity;
};

struct InteractInputEvent {};

// Event fired when an objective's state or progress changes (client-side)
struct ObjectiveUpdatedEvent {
  uint32_t objectiveId;
  uint8_t state;     // ObjectiveState enum value
  float progress;    // 0.0 - 1.0
  std::string name;  // Objective name for display
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
