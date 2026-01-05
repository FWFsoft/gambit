#include "TileRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "Logger.h"
#include "OpenGLUtils.h"
#include "TextureManager.h"
#include "config/ScreenConfig.h"
#include "config/TileConfig.h"

// Batch tile vertex shader
const char* batchTileVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 projection;
uniform vec2 cameraPos;
uniform vec2 screenCenter;

void main() {
    TexCoord = aTexCoord;

    // Apply isometric projection
    float isoX = aPos.x - aPos.y;
    float isoY = (aPos.x + aPos.y) * 0.5;

    // Apply camera transform
    float camIsoX = cameraPos.x - cameraPos.y;
    float camIsoY = (cameraPos.x + cameraPos.y) * 0.5;

    float screenX = isoX - camIsoX + screenCenter.x;
    float screenY = isoY - camIsoY + screenCenter.y;

    gl_Position = projection * vec4(screenX, screenY, 0.0, 1.0);
}
)";

// Batch tile fragment shader
const char* batchTileFragmentShader = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D image;

void main() {
    FragColor = texture(image, TexCoord);
}
)";

TileRenderer::TileRenderer(Camera* camera, SpriteRenderer* spriteRenderer,
                           Texture* whitePixel)
    : camera(camera),
      spriteRenderer(spriteRenderer),
      whitePixel(whitePixel),
      tilesetTexture(nullptr),
      batchBuilt(false),
      verticesUploaded(false) {
  initBatchRenderer();
}

TileRenderer::~TileRenderer() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteProgram(shaderProgram);
}

void TileRenderer::initBatchRenderer() {
  // Create shader program for batched tile rendering
  shaderProgram = OpenGLUtils::createShaderProgram(batchTileVertexShader,
                                                   batchTileFragmentShader);

  // Cache uniform locations
  uniformProjection = glGetUniformLocation(shaderProgram, "projection");
  uniformCameraPos = glGetUniformLocation(shaderProgram, "cameraPos");
  uniformScreenCenter = glGetUniformLocation(shaderProgram, "screenCenter");

  // Create VAO/VBO for dynamic batched rendering
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  // Allocate enough space for max tiles
  glBufferData(GL_ARRAY_BUFFER, Config::Tile::BATCH_BUFFER_SIZE * sizeof(float),
               nullptr, GL_DYNAMIC_DRAW);

  glBindVertexArray(VAO);

  // Position attribute
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

  // Texture coord attribute
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void*)(2 * sizeof(float)));

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  // Reserve space for vertices
  batchVertices.reserve(Config::Tile::BATCH_BUFFER_SIZE);
}

void TileRenderer::render(const TiledMap& map,
                          PlayerRenderCallback playerCallback) {
  const auto& tileLayers = map.getTileLayers();
  if (tileLayers.empty()) {
    return;
  }

  // Load tileset if not already loaded
  if (!tilesetTexture) {
    std::string tilesetPath = map.getTilesetImagePath();
    if (!tilesetPath.empty()) {
      tilesetTexture = TextureManager::instance().get(tilesetPath);
      if (!tilesetTexture) {
        Logger::info("Failed to load tileset: " + tilesetPath +
                     ", using placeholder");
        tilesetTexture = whitePixel;
      } else {
        Logger::info("Loaded tileset: " + tilesetPath);
      }
    } else {
      Logger::info("No tileset found in map, using placeholder");
      tilesetTexture = whitePixel;
    }
  }

  // Build all tile vertices into a single batch (only once - tiles are static)
  if (!batchBuilt) {
    buildTileBatch(map);
    batchBuilt = true;
  }

  // Render all tiles in one draw call
  renderBatch();

  // Render all entities on top of tiles
  // Entities are already depth-sorted by RenderSystem before drawing
  // Single callback - no need for multiple depth bands since tiles are drawn
  // first
  playerCallback(0.0f, 1000000.0f);
}

void TileRenderer::buildTileBatch(const TiledMap& map) {
  batchVertices.clear();

  const auto& tileLayers = map.getTileLayers();
  if (tileLayers.empty()) {
    return;
  }

  const tmx::TileLayer* layer = tileLayers[0];
  const auto& tiles = layer->getTiles();

  int tileWidth = map.getTileWidth();
  int tileHeight = map.getTileHeight();
  int columns = map.getTilesetColumns();
  int spacing = map.getTilesetSpacing();

  bool isPlaceholder = (tilesetTexture == whitePixel);

  for (int tileY = 0; tileY < map.getHeight(); ++tileY) {
    for (int tileX = 0; tileX < map.getWidth(); ++tileX) {
      int tileIndex = tileY * map.getWidth() + tileX;
      uint32_t gid = tiles[tileIndex].ID;

      if (gid == 0) continue;  // Skip empty tiles

      float worldX, worldY;
      gridToWorld(map, tileX, tileY, worldX, worldY);

      // Use world coordinates instead of screen coordinates
      // Camera transformation will be applied in shader
      float x = worldX - tileWidth / 2.0f;
      float y = worldY - tileHeight / 2.0f;

      // Calculate UV coordinates for texture atlas
      float u1, v1, u2, v2;
      if (isPlaceholder) {
        u1 = 0.0f;
        v1 = 0.0f;
        u2 = 1.0f;
        v2 = 1.0f;
      } else {
        int tileTextureIndex = gid - 1;
        // FIX: Use actual column count from tileset, not hardcoded 2
        int column = tileTextureIndex % columns;
        int row = tileTextureIndex / columns;
        // Account for spacing between tiles
        int srcX = column * (tileWidth + spacing);
        int srcY = row * (tileHeight + spacing);

        u1 = srcX / (float)tilesetTexture->getWidth();
        v1 = srcY / (float)tilesetTexture->getHeight();
        u2 = (srcX + tileWidth) / (float)tilesetTexture->getWidth();
        v2 = (srcY + tileHeight) / (float)tilesetTexture->getHeight();
      }

      // Build 6 vertices (2 triangles) for this tile
      // Triangle 1: top-left, bottom-right, bottom-left
      batchVertices.push_back(x);
      batchVertices.push_back(y + tileHeight);
      batchVertices.push_back(u1);
      batchVertices.push_back(v2);

      batchVertices.push_back(x + tileWidth);
      batchVertices.push_back(y);
      batchVertices.push_back(u2);
      batchVertices.push_back(v1);

      batchVertices.push_back(x);
      batchVertices.push_back(y);
      batchVertices.push_back(u1);
      batchVertices.push_back(v1);

      // Triangle 2: top-left, top-right, bottom-right
      batchVertices.push_back(x);
      batchVertices.push_back(y + tileHeight);
      batchVertices.push_back(u1);
      batchVertices.push_back(v2);

      batchVertices.push_back(x + tileWidth);
      batchVertices.push_back(y + tileHeight);
      batchVertices.push_back(u2);
      batchVertices.push_back(v2);

      batchVertices.push_back(x + tileWidth);
      batchVertices.push_back(y);
      batchVertices.push_back(u2);
      batchVertices.push_back(v1);
    }
  }
}

void TileRenderer::renderBatch() {
  if (batchVertices.empty()) {
    return;
  }

  glUseProgram(shaderProgram);

  // Set projection matrix
  glm::mat4 projection =
      glm::ortho(Config::Screen::ORTHO_LEFT, Config::Screen::ORTHO_RIGHT,
                 Config::Screen::ORTHO_BOTTOM, Config::Screen::ORTHO_TOP,
                 Config::Screen::ORTHO_NEAR, Config::Screen::ORTHO_FAR);
  glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, &projection[0][0]);

  // Update camera uniforms every frame
  glUniform2f(uniformCameraPos, camera->x, camera->y);
  glUniform2f(uniformScreenCenter, camera->screenWidth / 2.0f,
              camera->screenHeight / 2.0f);

  // Bind texture
  glActiveTexture(GL_TEXTURE0);
  tilesetTexture->bind();

  // Upload vertex data only once (tiles are static in world space)
  if (!verticesUploaded) {
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, batchVertices.size() * sizeof(float),
                    batchVertices.data());
    verticesUploaded = true;
  }

  // Draw all tiles in one call
  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLES, 0, batchVertices.size() / 4);
  glBindVertexArray(0);

  OpenGLUtils::checkGLError("TileRenderer::renderBatch");
}

void TileRenderer::gridToWorld(const TiledMap& map, int tileX, int tileY,
                               float& worldX, float& worldY) {
  float unit = map.getTileWidth() / 2.0f;
  worldX = tileX * unit;
  worldY = tileY * unit;
}
