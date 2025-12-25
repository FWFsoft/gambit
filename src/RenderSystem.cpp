#include "RenderSystem.h"

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "Logger.h"
#include "OpenGLUtils.h"
#include "Texture.h"

RenderSystem::RenderSystem(Window* window, ClientPrediction* clientPrediction,
                           RemotePlayerInterpolation* remoteInterpolation,
                           Camera* camera, TiledMap* tiledMap,
                           CollisionDebugRenderer* collisionDebugRenderer)
    : window(window),
      clientPrediction(clientPrediction),
      remoteInterpolation(remoteInterpolation),
      camera(camera),
      tiledMap(tiledMap),
      collisionDebugRenderer(collisionDebugRenderer) {
  // Initialize sprite renderer
  spriteRenderer = std::make_unique<SpriteRenderer>();

  // Create a white pixel texture for colored rectangles
  whitePixelTexture = std::make_unique<Texture>();
  whitePixelTexture->createWhitePixel();

  // Set up orthographic projection (800x600 screen space)
  glm::mat4 projection = glm::ortho(0.0f, 800.0f, 600.0f, 0.0f, -1.0f, 1.0f);
  spriteRenderer->setProjection(projection);

  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Create tile renderer with sprite renderer and white pixel texture
  tileRenderer = std::make_unique<TileRenderer>(camera, spriteRenderer.get(),
                                                whitePixelTexture.get());

  Logger::info("RenderSystem initialized with OpenGL");

  EventBus::instance().subscribe<RenderEvent>(
      [this](const RenderEvent& e) { onRender(e); });
}

void RenderSystem::onRender(const RenderEvent& e) {
  SDL_Window* sdlWindow = window->getWindow();

  const Player& localPlayer = clientPrediction->getLocalPlayer();
  camera->follow(localPlayer.x, localPlayer.y);

  // Clear the screen with OpenGL
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // Black background
  glClear(GL_COLOR_BUFFER_BIT);

  // Gather all players
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

  // Render tiles with depth-sorted players
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

  // Swap buffers
  SDL_GL_SwapWindow(sdlWindow);
  OpenGLUtils::checkGLError("RenderSystem::onRender");
}

void RenderSystem::drawPlayer(const Player& player) {
  int screenX, screenY;
  camera->worldToScreen(player.x, player.y, screenX, screenY);

  // Draw player as colored rectangle using white pixel texture
  if (whitePixelTexture) {
    float r = player.r / 255.0f;
    float g = player.g / 255.0f;
    float b = player.b / 255.0f;

    spriteRenderer->draw(*whitePixelTexture, screenX - PLAYER_SIZE / 2.0f,
                         screenY - PLAYER_SIZE / 2.0f, PLAYER_SIZE, PLAYER_SIZE,
                         r, g, b, 1.0f);
  }
}
