#include "RenderSystem.h"

#include "Logger.h"

RenderSystem::RenderSystem(Window* window, ClientPrediction* clientPrediction,
                           RemotePlayerInterpolation* remoteInterpolation)
    : window(window),
      clientPrediction(clientPrediction),
      remoteInterpolation(remoteInterpolation) {
  // Subscribe to RenderEvent
  EventBus::instance().subscribe<RenderEvent>(
      [this](const RenderEvent& e) { onRender(e); });
}

void RenderSystem::onRender(const RenderEvent& e) {
  SDL_Renderer* renderer = window->getRenderer();

  // Clear screen to black
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  // Draw local player (predicted position)
  const Player& localPlayer = clientPrediction->getLocalPlayer();
  drawPlayer(localPlayer);

  // Draw all remote players (interpolated positions)
  auto remotePlayerIds = remoteInterpolation->getRemotePlayerIds();
  for (uint32_t playerId : remotePlayerIds) {
    Player remotePlayer;
    if (remoteInterpolation->getInterpolatedState(playerId, e.interpolation,
                                                  remotePlayer)) {
      drawPlayer(remotePlayer);
    }
  }

  // Present the rendered frame
  SDL_RenderPresent(renderer);
}

void RenderSystem::drawPlayer(const Player& player) {
  SDL_Renderer* renderer = window->getRenderer();

  // Create a rectangle centered at the player's position
  SDL_Rect rect;
  rect.x = static_cast<int>(player.x - PLAYER_SIZE / 2);
  rect.y = static_cast<int>(player.y - PLAYER_SIZE / 2);
  rect.w = PLAYER_SIZE;
  rect.h = PLAYER_SIZE;

  // Set the draw color to the player's RGB color
  SDL_SetRenderDrawColor(renderer, player.r, player.g, player.b, 255);

  // Draw the filled rectangle
  SDL_RenderFillRect(renderer, &rect);
}
