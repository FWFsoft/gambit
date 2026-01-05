#include "RenderSystem.h"

#include <glad/glad.h>

#include <algorithm>
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
                           MusicZoneDebugRenderer* musicZoneDebugRenderer)
    : window(window),
      clientPrediction(clientPrediction),
      remoteInterpolation(remoteInterpolation),
      enemyInterpolation(enemyInterpolation),
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
