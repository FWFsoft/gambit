#include "RenderSystem.h"

#include <SDL2/SDL_image.h>
#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "GameStateManager.h"
#include "Logger.h"
#include "OpenGLUtils.h"
#include "Texture.h"
#include "TextureManager.h"
#include "config/PlayerConfig.h"
#include "config/ScreenConfig.h"

RenderSystem::RenderSystem(Window* window, ClientPrediction* clientPrediction,
                           RemotePlayerInterpolation* remoteInterpolation,
                           EnemyInterpolation* enemyInterpolation,
                           Camera* camera, TiledMap* tiledMap,
                           CollisionDebugRenderer* collisionDebugRenderer,
                           MusicZoneDebugRenderer* musicZoneDebugRenderer,
                           ObjectiveDebugRenderer* objectiveDebugRenderer)
    : window(window),
      clientPrediction(clientPrediction),
      remoteInterpolation(remoteInterpolation),
      enemyInterpolation(enemyInterpolation),
      camera(camera),
      tiledMap(tiledMap),
      collisionDebugRenderer(collisionDebugRenderer),
      musicZoneDebugRenderer(musicZoneDebugRenderer),
      objectiveDebugRenderer(objectiveDebugRenderer) {
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
  // Skip game world rendering during title screen and character select
  GameState currentState = GameStateManager::instance().getCurrentState();
  if (currentState == GameState::TitleScreen ||
      currentState == GameState::CharacterSelect) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return;
  }

  SDL_Window* sdlWindow = window->getWindow();

  const Player& localPlayer = clientPrediction->getLocalPlayer();
  camera->follow(localPlayer.x, localPlayer.y);

  // Clear the screen with OpenGL
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // Black background
  glClear(GL_COLOR_BUFFER_BIT);

  // Gather all players
  std::vector<Player> allPlayers;

  // Add local player if alive
  if (localPlayer.isAlive()) {
    allPlayers.push_back(localPlayer);
  }

  auto remotePlayerIds = remoteInterpolation->getRemotePlayerIds();
  for (uint32_t playerId : remotePlayerIds) {
    Player remotePlayer;
    if (remoteInterpolation->getInterpolatedState(playerId, e.interpolation,
                                                  remotePlayer)) {
      // Skip dead players
      if (remotePlayer.isDead()) {
        continue;
      }
      allPlayers.push_back(remotePlayer);
    }
  }

  // Gather all enemies (with frustum culling for performance)
  std::vector<Enemy> allEnemies;
  if (enemyInterpolation) {
    auto enemyIds = enemyInterpolation->getEnemyIds();

    for (uint32_t enemyId : enemyIds) {
      Enemy enemy;
      if (enemyInterpolation->getInterpolatedState(enemyId, e.interpolation,
                                                   enemy)) {
        // Skip dead enemies
        if (enemy.state == ::EnemyState::Dead) {
          continue;
        }

        // Frustum culling: Check if enemy is potentially visible on screen
        // Use generous bounds to account for isometric projection
        int screenX, screenY;
        camera->worldToScreen(enemy.x, enemy.y, screenX, screenY);

        // Cull enemies far off-screen (beyond 200px margin)
        if (screenX < -200 || screenX > Config::Screen::WIDTH + 200 ||
            screenY < -200 || screenY > Config::Screen::HEIGHT + 200) {
          continue;
        }

        allEnemies.push_back(enemy);
      }
    }
  }

  // Collect all entities with their depths for sorting
  struct EntityToRender {
    float depth;
    enum Type { Player, Enemy, WorldItem } type;
    union {
      const ::Player* player;
      const ::Enemy* enemy;
      const ::WorldItem* worldItem;
    };
  };

  // Get world items from client prediction
  const auto& worldItems = clientPrediction->getWorldItems();

  std::vector<EntityToRender> entitiesToRender;
  entitiesToRender.reserve(allPlayers.size() + allEnemies.size() +
                           worldItems.size());

  for (const Player& player : allPlayers) {
    EntityToRender entity;
    entity.depth = player.x + player.y;
    entity.type = EntityToRender::Player;
    entity.player = &player;
    entitiesToRender.push_back(entity);
  }

  for (const Enemy& enemy : allEnemies) {
    EntityToRender entity;
    entity.depth = enemy.x + enemy.y;
    entity.type = EntityToRender::Enemy;
    entity.enemy = &enemy;
    entitiesToRender.push_back(entity);
  }

  for (const auto& [id, worldItem] : worldItems) {
    EntityToRender entity;
    entity.depth = worldItem.x + worldItem.y;
    entity.type = EntityToRender::WorldItem;
    entity.worldItem = &worldItem;
    entitiesToRender.push_back(entity);
  }

  // Sort by depth (back to front for painter's algorithm)
  std::sort(entitiesToRender.begin(), entitiesToRender.end(),
            [](const EntityToRender& a, const EntityToRender& b) {
              return a.depth < b.depth;
            });

  // Render tiles first, then all entities in depth order (single callback)
  tileRenderer->render(*tiledMap, [&](float minDepth, float maxDepth) {
    // Draw objective zones first (as ground markers)
    const auto& objectives = clientPrediction->getObjectives();
    for (const auto& [id, objective] : objectives) {
      drawObjective(objective);
    }

    // Render all entities in sorted order
    for (const auto& entity : entitiesToRender) {
      if (entity.type == EntityToRender::Player) {
        drawPlayer(*entity.player);
      } else if (entity.type == EntityToRender::Enemy) {
        drawEnemy(*entity.enemy);
      } else if (entity.type == EntityToRender::WorldItem) {
        drawWorldItem(*entity.worldItem);
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
    // Also render map bounds in debug mode
    if (tiledMap) {
      collisionDebugRenderer->renderMapBounds(tiledMap->getWorldWidth(),
                                              tiledMap->getWorldHeight());
    }
  }

  // Render objective debug overlay (if enabled)
  if (objectiveDebugRenderer) {
    objectiveDebugRenderer->render();
  }

  // NOTE: Buffer swap moved to GameLoop to ensure UI renders after game world
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

  // Render health bar above player
  drawHealthBar(screenX, screenY - Config::Player::SIZE / 2.0f - 10,
                player.health, Config::Player::MAX_HEALTH);
}

void RenderSystem::drawEnemy(const Enemy& enemy) {
  int screenX, screenY;
  camera->worldToScreen(enemy.x, enemy.y, screenX, screenY);

  // Render enemy sprite (POC: use player sprite with red tint)
  const AnimationController* controller = enemy.getAnimationController();
  if (controller && playerTexture) {
    int srcX, srcY, srcW, srcH;
    controller->getCurrentFrame(srcX, srcY, srcW, srcH);

    // Red tint for enemies
    spriteRenderer->drawRegion(
        *playerTexture, screenX - Config::Player::SIZE / 2.0f,
        screenY - Config::Player::SIZE / 2.0f, Config::Player::SIZE,
        Config::Player::SIZE, srcX, srcY, srcW, srcH, 1.0f, 0.3f, 0.3f,
        1.0f  // Red tint (R=1.0, G=0.3, B=0.3)
    );
  }

  // Render health bar
  drawHealthBar(screenX, screenY - 20, enemy.health, enemy.maxHealth);
}

void RenderSystem::drawWorldItem(const WorldItem& worldItem) {
  int screenX, screenY;
  camera->worldToScreen(worldItem.x, worldItem.y, screenX, screenY);

  // Render as 16x16 gold square (placeholder for now)
  constexpr float ITEM_SIZE = 16.0f;
  glm::vec4 color(1.0f, 0.84f, 0.0f, 1.0f);  // Gold color

  spriteRenderer->draw(*whitePixelTexture,
                       static_cast<float>(screenX - ITEM_SIZE / 2),
                       static_cast<float>(screenY - ITEM_SIZE / 2), ITEM_SIZE,
                       ITEM_SIZE, color.r, color.g, color.b, color.a);
}

void RenderSystem::drawHealthBar(int x, int y, float health, float maxHealth) {
  constexpr int BAR_WIDTH = 32;
  constexpr int BAR_HEIGHT = 4;

  float healthPercent = health / maxHealth;
  int filledWidth = static_cast<int>(BAR_WIDTH * healthPercent);

  // Background (black)
  spriteRenderer->draw(*whitePixelTexture, x - BAR_WIDTH / 2, y, BAR_WIDTH,
                       BAR_HEIGHT, 0.0f, 0.0f, 0.0f, 0.8f);

  // Foreground (green to red gradient based on health)
  float r = 1.0f - healthPercent;  // More red when low health
  float g = healthPercent;         // More green when high health

  spriteRenderer->draw(*whitePixelTexture, x - BAR_WIDTH / 2, y, filledWidth,
                       BAR_HEIGHT, r, g, 0.0f, 1.0f);
}

void RenderSystem::drawObjective(const ClientObjective& objective) {
  int screenX, screenY;
  camera->worldToScreen(objective.x, objective.y, screenX, screenY);

  // Choose color based on state
  float r, g, b, alpha;
  switch (objective.state) {
    case ObjectiveState::Inactive:
      r = 1.0f;
      g = 1.0f;
      b = 0.2f;  // Yellow
      alpha = 0.3f;
      break;
    case ObjectiveState::InProgress:
      r = 0.2f;
      g = 0.6f;
      b = 1.0f;  // Blue
      alpha = 0.4f;
      break;
    case ObjectiveState::Completed:
      r = 0.2f;
      g = 1.0f;
      b = 0.2f;  // Green
      alpha = 0.2f;
      break;
    default:
      r = 0.5f;
      g = 0.5f;
      b = 0.5f;  // Gray
      alpha = 0.3f;
      break;
  }

  // Draw a diamond shape for the objective zone
  // We approximate by drawing a rotated square (4 triangles from center)
  // For simplicity, draw as a filled circle approximation using multiple
  // rectangles

  // Draw as a series of horizontal bars to approximate a circle/diamond
  float radius = objective.radius;
  constexpr int NUM_SEGMENTS = 16;

  for (int i = 0; i < NUM_SEGMENTS; i++) {
    // Calculate Y offset for this segment
    float t =
        (static_cast<float>(i) / (NUM_SEGMENTS - 1)) * 2.0f - 1.0f;  // -1 to 1
    float yOffset = t * radius;

    // Width at this Y (diamond shape: width = radius - |yOffset|)
    float width = radius - std::abs(yOffset);
    if (width <= 0) continue;

    // Convert to screen coordinates
    // In isometric view, we need to account for the projection
    int segScreenX, segScreenY;
    camera->worldToScreen(objective.x, objective.y + yOffset, segScreenX,
                          segScreenY);

    // Draw horizontal bar at this Y level
    spriteRenderer->draw(
        *whitePixelTexture, static_cast<float>(segScreenX - width),
        static_cast<float>(segScreenY - 2), width * 2.0f, 4.0f, r, g, b, alpha);
  }

  // Draw a small icon in the center based on objective type
  float iconR, iconG, iconB;
  if (objective.type == ObjectiveType::AlienScrapyard) {
    iconR = 0.8f;
    iconG = 0.6f;
    iconB = 0.2f;  // Orange/brown for scrapyard
  } else {
    iconR = 1.0f;
    iconG = 0.2f;
    iconB = 0.2f;  // Red for outpost
  }

  // Draw center marker (small square)
  constexpr float ICON_SIZE = 12.0f;
  spriteRenderer->draw(*whitePixelTexture,
                       static_cast<float>(screenX - ICON_SIZE / 2),
                       static_cast<float>(screenY - ICON_SIZE / 2), ICON_SIZE,
                       ICON_SIZE, iconR, iconG, iconB, 0.9f);

  // Draw progress ring for in-progress objectives
  if (objective.state == ObjectiveState::InProgress &&
      objective.progress > 0.0f) {
    // Draw a progress indicator arc (simplified as a bar below the icon)
    float progressWidth = ICON_SIZE * 2.0f * objective.progress;
    spriteRenderer->draw(
        *whitePixelTexture, static_cast<float>(screenX - ICON_SIZE),
        static_cast<float>(screenY + ICON_SIZE / 2 + 4), progressWidth, 4.0f,
        0.2f, 1.0f, 0.2f, 0.9f);  // Green progress bar
  }
}

void RenderSystem::captureScreenshot(const std::string& path) {
  // Ensure output directory exists
  std::filesystem::path filePath(path);
  if (filePath.has_parent_path()) {
    std::filesystem::create_directories(filePath.parent_path());
  }

  // Read pixels from OpenGL framebuffer
  int width = Config::Screen::WIDTH;
  int height = Config::Screen::HEIGHT;

  std::vector<uint8_t> pixels(width * height * 4);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

  // Create SDL surface from pixel data
  SDL_Surface* surface =
      SDL_CreateRGBSurfaceFrom(pixels.data(), width, height, 32, width * 4,
                               0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

  if (!surface) {
    Logger::error("Failed to create surface for screenshot: " +
                  std::string(SDL_GetError()));
    return;
  }

  // Flip image vertically (OpenGL has origin at bottom-left)
  SDL_Surface* flipped = SDL_CreateRGBSurface(
      0, width, height, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

  if (!flipped) {
    SDL_FreeSurface(surface);
    Logger::error("Failed to create flipped surface: " +
                  std::string(SDL_GetError()));
    return;
  }

  SDL_LockSurface(surface);
  SDL_LockSurface(flipped);

  uint8_t* srcPixels = static_cast<uint8_t*>(surface->pixels);
  uint8_t* dstPixels = static_cast<uint8_t*>(flipped->pixels);

  for (int y = 0; y < height; y++) {
    memcpy(dstPixels + y * flipped->pitch,
           srcPixels + (height - 1 - y) * surface->pitch, width * 4);
  }

  SDL_UnlockSurface(flipped);
  SDL_UnlockSurface(surface);

  // Save to PNG using atomic write (write to temp file, then rename)
  // This prevents race conditions where readers get partial/corrupt files
  std::string tempPath = path + ".tmp";
  if (IMG_SavePNG(flipped, tempPath.c_str()) != 0) {
    Logger::error("Failed to save screenshot to " + path + ": " +
                  std::string(IMG_GetError()));
  } else {
    // Atomic rename - ensures readers never see partial file
    std::error_code ec;
    std::filesystem::rename(tempPath, path, ec);
    if (ec) {
      Logger::error("Failed to rename screenshot temp file: " + ec.message());
    } else {
      Logger::info("Screenshot saved to " + path);
    }
  }

  SDL_FreeSurface(flipped);
  SDL_FreeSurface(surface);
}
