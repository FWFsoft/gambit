#include "RenderSystem.h"

#include <vector>

#include "Logger.h"

RenderSystem::RenderSystem(Window* window, ClientPrediction* clientPrediction,
                           RemotePlayerInterpolation* remoteInterpolation,
                           Camera* camera, TiledMap* tiledMap,
                           TileRenderer* tileRenderer,
                           CollisionDebugRenderer* collisionDebugRenderer)
    : window(window),
      clientPrediction(clientPrediction),
      remoteInterpolation(remoteInterpolation),
      camera(camera),
      tiledMap(tiledMap),
      tileRenderer(tileRenderer),
      collisionDebugRenderer(collisionDebugRenderer) {
  EventBus::instance().subscribe<RenderEvent>(
      [this](const RenderEvent& e) { onRender(e); });
}

void RenderSystem::onRender(const RenderEvent& e) {
  SDL_Renderer* renderer = window->getRenderer();

  const Player& localPlayer = clientPrediction->getLocalPlayer();
  camera->follow(localPlayer.x, localPlayer.y);

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  std::vector<Player> allPlayers;
  allPlayers.push_back(localPlayer);

  auto remotePlayerIds = remoteInterpolation->getRemotePlayerIds();
  for (uint32_t playerId : remotePlayerIds) {
    Player remotePlayer;
    if (remoteInterpolation->getInterpolatedState(playerId, e.interpolation,
                                                  remotePlayer)) {
      allPlayers.push_back(remotePlayer);
    }
  }

  tileRenderer->render(*tiledMap, [&](float minDepth, float maxDepth) {
    for (const Player& player : allPlayers) {
      float playerDepth = player.x + player.y;
      if (playerDepth >= minDepth && playerDepth < maxDepth) {
        drawPlayer(player);
      }
    }
  });

  // Render collision debug overlay (if enabled)
  if (collisionDebugRenderer) {
    collisionDebugRenderer->render();
  }

  SDL_RenderPresent(renderer);
}

void RenderSystem::drawPlayer(const Player& player) {
  SDL_Renderer* renderer = window->getRenderer();

  int screenX, screenY;
  camera->worldToScreen(player.x, player.y, screenX, screenY);

  SDL_Rect rect;
  rect.x = screenX - PLAYER_SIZE / 2;
  rect.y = screenY - PLAYER_SIZE / 2;
  rect.w = PLAYER_SIZE;
  rect.h = PLAYER_SIZE;

  SDL_SetRenderDrawColor(renderer, player.r, player.g, player.b, 255);
  SDL_RenderFillRect(renderer, &rect);
}
