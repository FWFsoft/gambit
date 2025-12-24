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
      nullptr, serialized.data(), serialized.size()});
}

void publishPlayerJoined(uint32_t playerId, uint8_t r, uint8_t g, uint8_t b) {
  PlayerJoinedPacket packet;
  packet.playerId = playerId;
  packet.r = r;
  packet.g = g;
  packet.b = b;
  auto serialized = serialize(packet);
  EventBus::instance().publish(NetworkPacketReceivedEvent{
      nullptr, serialized.data(), serialized.size()});
}
