#include "RenderSystem.h"

#include <glad/glad.h>

#include <vector>

#include "Logger.h"
#include "OpenGLUtils.h"

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
  SDL_Window* sdlWindow = window->getWindow();

  const Player& localPlayer = clientPrediction->getLocalPlayer();
  camera->follow(localPlayer.x, localPlayer.y);

  // Clear the screen with OpenGL
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // TODO: Implement OpenGL rendering in Phase 3
  // For now, just clear the screen to verify OpenGL works

  // Swap buffers
  SDL_GL_SwapWindow(sdlWindow);
  OpenGLUtils::checkGLError("RenderSystem::onRender");
}

void RenderSystem::drawPlayer(const Player& player) {
  // TODO: Implement OpenGL sprite rendering in Phase 3
  // This will use textured quads instead of SDL_RenderFillRect
  (void)player;  // Suppress unused parameter warning
}
