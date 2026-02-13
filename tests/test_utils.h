#pragma once

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#include "EventBus.h"
#include "InputSystem.h"
#include "NetworkProtocol.h"
#include "Player.h"

#define TEST(name)    \
  void test_##name(); \
  void test_##name()

bool floatEqual(float a, float b, float epsilon = 0.001f) {
  return std::abs(a - b) < epsilon;
}

void resetEventBus() { EventBus::instance().clear(); }

LocalInputEvent publishKeyAndUpdate(InputSystem& inputSystem, int sdlKey,
                                    bool isKeyDown) {
  LocalInputEvent capturedEvent;
  bool captured = false;

  EventBus::instance().subscribe<LocalInputEvent>(
      [&](const LocalInputEvent& e) {
        if (!captured) {
          capturedEvent = e;
          captured = true;
        }
      });

  if (isKeyDown) {
    EventBus::instance().publish(KeyDownEvent{sdlKey});
  } else {
    EventBus::instance().publish(KeyUpEvent{sdlKey});
  }

  EventBus::instance().publish(UpdateEvent{16.67f, 1});
  return capturedEvent;
}

void publishStateUpdate(uint32_t serverTick,
                        const std::vector<PlayerState>& players) {
  StateUpdatePacket packet;
  packet.serverTick = serverTick;
  packet.players = players;
  auto serialized = serialize(packet);
  EventBus::instance().publish(NetworkPacketReceivedEvent{
      0, serialized.data(), serialized.size()});
}

void publishPlayerJoined(uint32_t playerId, uint8_t r, uint8_t g, uint8_t b) {
  PlayerJoinedPacket packet;
  packet.playerId = playerId;
  packet.r = r;
  packet.g = g;
  packet.b = b;
  auto serialized = serialize(packet);
  EventBus::instance().publish(NetworkPacketReceivedEvent{
      0, serialized.data(), serialized.size()});
}

// Event Capture - Collects all events of a specific type for assertions
template <typename EventType>
class EventCapture {
 public:
  EventCapture() {
    EventBus::instance().subscribe<EventType>(
        [this](const EventType& e) { events.push_back(e); });
  }

  const std::vector<EventType>& getEvents() const { return events; }

  size_t count() const { return events.size(); }

  bool empty() const { return events.empty(); }

  const EventType& first() const {
    assert(!events.empty() && "No events captured");
    return events[0];
  }

  const EventType& last() const {
    assert(!events.empty() && "No events captured");
    return events.back();
  }

  const EventType& at(size_t index) const {
    assert(index < events.size() && "Index out of bounds");
    return events[index];
  }

  void assertCount(size_t expected) const {
    if (events.size() != expected) {
      std::cerr << "Event count assertion failed: expected " << expected
                << ", got " << events.size() << std::endl;
      assert(false);
    }
  }

  void assertAtLeast(size_t minimum) const {
    if (events.size() < minimum) {
      std::cerr << "Event count assertion failed: expected at least " << minimum
                << ", got " << events.size() << std::endl;
      assert(false);
    }
  }

  template <typename Predicate>
  void assertAny(Predicate predicate) const {
    for (const auto& event : events) {
      if (predicate(event)) {
        return;  // Found matching event
      }
    }
    std::cerr << "assertAny failed: no matching events found" << std::endl;
    assert(false);
  }

  template <typename Predicate>
  void assertAll(Predicate predicate) const {
    for (size_t i = 0; i < events.size(); ++i) {
      if (!predicate(events[i])) {
        std::cerr << "assertAll failed at index " << i << std::endl;
        assert(false);
      }
    }
  }

  template <typename Predicate>
  size_t countMatching(Predicate predicate) const {
    size_t count = 0;
    for (const auto& event : events) {
      if (predicate(event)) {
        count++;
      }
    }
    return count;
  }

  void clear() { events.clear(); }

 private:
  std::vector<EventType> events;
};

// Event Logger - Logs events to stdout for debugging
class EventLogger {
 public:
  EventLogger(bool enabled = true) : enabled(enabled), frameNumber(0) {
    if (!enabled) return;

    EventBus::instance().subscribe<UpdateEvent>([this](const UpdateEvent& e) {
      frameNumber = e.frameNumber;
      if (verbose) {
        std::cout << "[Frame " << frameNumber
                  << "] UpdateEvent (deltaTime: " << e.deltaTime << "ms)"
                  << std::endl;
      }
    });

    EventBus::instance().subscribe<RenderEvent>([this](const RenderEvent& e) {
      if (verbose) {
        std::cout << "[Frame " << frameNumber
                  << "] RenderEvent (interpolation: " << e.interpolation << ")"
                  << std::endl;
      }
    });

    EventBus::instance().subscribe<LocalInputEvent>(
        [this](const LocalInputEvent& e) {
          std::cout << "[Frame " << frameNumber
                    << "] LocalInputEvent (sequence: " << e.inputSequence
                    << ", moveLeft: " << e.moveLeft
                    << ", moveRight: " << e.moveRight
                    << ", moveUp: " << e.moveUp << ", moveDown: " << e.moveDown
                    << ")" << std::endl;
        });

    EventBus::instance().subscribe<NetworkPacketReceivedEvent>(
        [this](const NetworkPacketReceivedEvent& e) {
          if (e.size >= 1) {
            PacketType type =
                static_cast<PacketType>(static_cast<const uint8_t*>(e.data)[0]);
            std::cout << "[Frame " << frameNumber
                      << "] NetworkPacketReceivedEvent (type: "
                      << static_cast<int>(type) << ", size: " << e.size
                      << " bytes)" << std::endl;
          }
        });
  }

  void setVerbose(bool v) { verbose = v; }

  void logMessage(const std::string& message) {
    if (enabled) {
      std::cout << "[Frame " << frameNumber << "] " << message << std::endl;
    }
  }

 private:
  bool enabled;
  bool verbose = false;
  uint64_t frameNumber;
};
