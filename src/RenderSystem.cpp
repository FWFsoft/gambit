#include "RenderSystem.h"

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "Logger.h"
#include "OpenGLUtils.h"
#include "Texture.h"
#include "TextureManager.h"
#include "config/PlayerConfig.h"
#include "config/ScreenConfig.h"

RenderSystem::RenderSystem(Window* window, ClientPrediction* clientPrediction,
                           RemotePlayerInterpolation* remoteInterpolation,
                           Camera* camera, TiledMap* tiledMap,
                           CollisionDebugRenderer* collisionDebugRenderer,
                           MusicZoneDebugRenderer* musicZoneDebugRenderer)
    : window(window),
      clientPrediction(clientPrediction),
      remoteInterpolation(remoteInterpolation),
      camera(camera),
      tiledMap(tiledMap),
      collisionDebugRenderer(collisionDebugRenderer),
      musicZoneDebugRenderer(musicZoneDebugRenderer) {
  // Initialize sprite renderer
  spriteRenderer = std::make_unique<SpriteRenderer>();

  // Create a white pixel texture for colored rectangles
  whitePixelTexture = std::make_unique<Texture>();
  whitePixelTexture->createWhitePixel();

  // Set up orthographic projection
  glm::mat4 projection =
      glm::ortho(Config::Screen::ORTHO_LEFT, Config::Screen::ORTHO_RIGHT,
                 Config::Screen::ORTHO_BOTTOM, Config::Screen::ORTHO_TOP,
                 Config::Screen::ORTHO_NEAR, Config::Screen::ORTHO_FAR);
  spriteRenderer->setProjection(projection);

  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Create tile renderer with sprite renderer and white pixel texture
  tileRenderer = std::make_unique<TileRenderer>(camera, spriteRenderer.get(),
                                                whitePixelTexture.get());

  // Try to load animated player sprite sheet (fallback to colored rectangle if
  // not found)
  playerTexture = TextureManager::instance().get("assets/player_animated.png");
  if (!playerTexture) {
    Logger::info("Player sprite sheet not found, using colored rectangles");
    playerTexture = whitePixelTexture.get();
  }

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

  // Render music zone debug overlay (if enabled)
  if (musicZoneDebugRenderer) {
    musicZoneDebugRenderer->render();
  }

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

  if (playerTexture) {
    // If using actual sprite, render without color tint
    // If using white pixel fallback, tint with player color
    bool isPlaceholder = (playerTexture == whitePixelTexture.get());

    float r = isPlaceholder ? player.r / 255.0f : 1.0f;
    float g = isPlaceholder ? player.g / 255.0f : 1.0f;
    float b = isPlaceholder ? player.b / 255.0f : 1.0f;

    // Use animation frame if animation controller exists
    const AnimationController* controller = player.getAnimationController();
    if (controller && !isPlaceholder) {
      int srcX, srcY, srcW, srcH;
      controller->getCurrentFrame(srcX, srcY, srcW, srcH);

      spriteRenderer->drawRegion(
          *playerTexture, screenX - Config::Player::SIZE / 2.0f,
          screenY - Config::Player::SIZE / 2.0f, Config::Player::SIZE,
          Config::Player::SIZE, srcX, srcY, srcW, srcH, r, g, b, 1.0f);
    } else {
      // Fallback to full sprite (for placeholder or if no animation)
      spriteRenderer->draw(
          *playerTexture, screenX - Config::Player::SIZE / 2.0f,
          screenY - Config::Player::SIZE / 2.0f, Config::Player::SIZE,
          Config::Player::SIZE, r, g, b, 1.0f);
    }
  }
}
