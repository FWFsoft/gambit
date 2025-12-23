#pragma once

#include <SDL2/SDL.h>

#include "ClientPrediction.h"
#include "EventBus.h"
#include "RemotePlayerInterpolation.h"
#include "Window.h"

// RenderSystem handles all rendering logic for the game
// Subscribes to RenderEvent and draws local player + remote players
class RenderSystem {
 public:
  RenderSystem(Window* window, ClientPrediction* clientPrediction,
               RemotePlayerInterpolation* remoteInterpolation);

  // Cleanup (unsubscribe handled by EventBus::clear if needed)
  ~RenderSystem() = default;

 private:
  Window* window;
  ClientPrediction* clientPrediction;
  RemotePlayerInterpolation* remoteInterpolation;

  // Handle render events
  void onRender(const RenderEvent& e);

  // Draw a single player as a 32x32 colored rectangle
  void drawPlayer(const Player& player);

  // Constants
  static constexpr int PLAYER_SIZE = 32;  // 32x32 pixel rectangle
};
