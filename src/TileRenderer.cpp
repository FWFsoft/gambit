#include "TileRenderer.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

#include "Logger.h"
#include "OpenGLUtils.h"
#include "ShaderCompat.h"
#include "PresentationLayer.h"
#include "TextureManager.h"
#include "config/LookConfig.h"
#include "config/ScreenConfig.h"
#include "config/TileConfig.h"

// Batch tile vertex shader
const char* batchTileVertexShader =
    GAMBIT_GLSL_VERSION
    R"(
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in float aShade;

out vec2 TexCoord;
out float Shade;

uniform mat4 projection;
uniform vec2 cameraPos;
uniform vec2 screenCenter;
uniform float zoom;

void main() {
    TexCoord = aTexCoord;
    Shade = aShade;

    // gridToWorld already converted to isometric, so just apply the camera
    // offset and magnify around the screen centre
    float screenX = (aPos.x - cameraPos.x) * zoom + screenCenter.x;
    float screenY = (aPos.y - cameraPos.y) * zoom + screenCenter.y;

    gl_Position = projection * vec4(screenX, screenY, 0.0, 1.0);
}
)";

// Batch tile fragment shader.
// World values (docs/design/ART_DIRECTION.md, "The look"): the floor is
// pushed dark, cool and low in contrast here, so every tileset is covered,
// then lit by the light map. Strengths: include/config/LookConfig.h.
const char* batchTileFragmentShader =
    GAMBIT_GLSL_VERSION
    GAMBIT_GLSL_PRECISION_HIGH
    R"(
in vec2 TexCoord;
in float Shade;  // the terrain's slope light: < 1 on faces turned away
out vec4 FragColor;

uniform sampler2D image;
uniform sampler2D lightMap;
uniform float look;         // 0 = tiles as painted, 1 = the look
uniform vec2 viewportSize;  // framebuffer pixels
uniform vec3 floorTint;
uniform vec3 floorValue;    // min, range, saturation
uniform vec3 floorLight;    // range, knee, add
uniform vec3 floorVignette; // amount, start, power

void main() {
    vec4 tile = texture(image, TexCoord);
    float lum = dot(tile.rgb, vec3(0.299, 0.587, 0.114));
    vec3 base = mix(vec3(lum), tile.rgb, floorValue.z);
    base = (floorValue.x + base * floorValue.y) * floorTint * Shade;

    vec2 screenUv = gl_FragCoord.xy / viewportSize;
    vec3 light = texture(lightMap, screenUv).rgb * floorLight.x;
    light = light / (1.0 + light * floorLight.y);
    vec3 lit = base * (1.0 + light) + light * floorLight.z;

    float edge = length((screenUv - 0.5) * 2.0);
    lit *= 1.0 - floorVignette.x *
           pow(clamp(edge - floorVignette.y, 0.0, 1.0), floorVignette.z);

    FragColor = vec4(mix(tile.rgb * Shade, lit, look), tile.a);
}
)";

TileRenderer::TileRenderer(Camera* camera, SpriteRenderer* spriteRenderer,
                           Texture* whitePixel)
    : camera(camera),
      spriteRenderer(spriteRenderer),
      whitePixel(whitePixel),
      tilesetTexture(nullptr),
      batchBuilt(false),
      verticesUploaded(false),
      vboCapacity(0) {
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
  uniformZoom = glGetUniformLocation(shaderProgram, "zoom");
  uniformLook = glGetUniformLocation(shaderProgram, "look");
  uniformViewportSize = glGetUniformLocation(shaderProgram, "viewportSize");

  // The look's strengths are compile-time constants: set them once.
  namespace Look = Config::Look;
  glUseProgram(shaderProgram);
  glUniform1i(glGetUniformLocation(shaderProgram, "image"), 0);
  glUniform1i(glGetUniformLocation(shaderProgram, "lightMap"), 1);
  glUniform3f(glGetUniformLocation(shaderProgram, "floorTint"),
              Look::FLOOR_TINT_R, Look::FLOOR_TINT_G, Look::FLOOR_TINT_B);
  glUniform3f(glGetUniformLocation(shaderProgram, "floorValue"),
              Look::FLOOR_VALUE_MIN, Look::FLOOR_VALUE_RANGE,
              Look::FLOOR_SATURATION);
  glUniform3f(glGetUniformLocation(shaderProgram, "floorLight"),
              Look::LIGHT_RANGE, Look::LIGHT_KNEE, Look::LIGHT_ADD);
  glUniform3f(glGetUniformLocation(shaderProgram, "floorVignette"),
              Look::FLOOR_VIGNETTE, Look::FLOOR_VIGNETTE_START,
              Look::FLOOR_VIGNETTE_POWER);
  glUseProgram(0);

  // Create VAO/VBO for dynamic batched rendering
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  // Start with zero capacity - will allocate on first render based on actual
  // map size
  vboCapacity = 0;

  glBindVertexArray(VAO);

  // Position attribute
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float),
                        (void*)0);

  // Texture coord attribute
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                        FLOATS_PER_VERTEX * sizeof(float),
                        (void*)(2 * sizeof(float)));

  // Shade attribute (slope light)
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE,
                        FLOATS_PER_VERTEX * sizeof(float),
                        (void*)(4 * sizeof(float)));

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  // Vector will auto-grow in buildTileBatch() based on actual map size
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
  const TerrainGrid& terrain = map.terrain();
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
      // For isometric tiles, center the quad at the world position
      float x = worldX - tileWidth / 2.0f;
      float y = worldY - tileHeight / 2.0f;  // Center anchor for proper overlap

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

      if (terrain.active()) {
        addTerrainTile(terrain, tileX, tileY, worldX, worldY, tileWidth,
                       tileHeight, u1, v1, u2, v2);
        continue;
      }

      // Build 6 vertices (2 triangles) for this tile
      auto push = [&](float px, float py, float pu, float pv) {
        batchVertices.insert(batchVertices.end(), {px, py, pu, pv, 1.0f});
      };
      // Triangle 1: top-left, bottom-right, bottom-left
      push(x, y + tileHeight, u1, v2);
      push(x + tileWidth, y, u2, v1);
      push(x, y, u1, v1);
      // Triangle 2: top-left, top-right, bottom-right
      push(x, y + tileHeight, u1, v2);
      push(x + tileWidth, y + tileHeight, u2, v2);
      push(x + tileWidth, y, u2, v1);
    }
  }
}

void TileRenderer::addTerrainTile(const TerrainGrid& terrain, int tileX,
                                  int tileY, float worldX, float worldY,
                                  int tileWidth, int tileHeight, float u1,
                                  float v1, float u2, float v2) {
  // The tile is drawn as its diamond, each corner lifted to the ground's
  // height there: one smooth surface, so slopes and cliffs are the mesh
  // itself. The diamond sits in the middle half of its 64x64 cell.
  const float hw = tileWidth / 2.0f, qh = tileHeight / 4.0f;
  const float um = (u1 + u2) / 2.0f, vm = (v1 + v2) / 2.0f;
  const float vq = (v2 - v1) / 4.0f;
  struct Corner {
    float x, y, u, v;
    int cx, cy;
  };
  const Corner corners[4] = {
      {worldX, worldY - qh, um, vm - vq, tileX, tileY},              // top
      {worldX + hw, worldY, u2, vm, tileX + 1, tileY},               // right
      {worldX, worldY + qh, um, vm + vq, tileX + 1, tileY + 1},      // bottom
      {worldX - hw, worldY, u1, vm, tileX, tileY + 1},               // left
  };
  auto push = [&](const Corner& c) {
    float lift = terrain.cornerLift(c.cx, c.cy);
    batchVertices.insert(batchVertices.end(),
                         {c.x, c.y - lift, c.u, c.v, slopeShade(terrain, c.cx, c.cy)});
  };
  push(corners[0]);
  push(corners[1]);
  push(corners[2]);
  push(corners[0]);
  push(corners[2]);
  push(corners[3]);
}

float TileRenderer::slopeShade(const TerrainGrid& terrain, int cx, int cy) {
  // Slope light at a corner. Down the screen is +cx and +cy together; to
  // the right is +cx, -cy. A face that drops toward the camera is a cliff
  // seen head on: dark. A face rising to the right is turned to the light
  // (from the upper left): a little brighter.
  auto lift = [&](int x, int y) {
    x = std::clamp(x, 0, terrain.width());
    y = std::clamp(y, 0, terrain.height());
    return terrain.cornerLift(x, y);
  };
  float dcx = (lift(cx + 1, cy) - lift(cx - 1, cy)) * 0.5f;
  float dcy = (lift(cx, cy + 1) - lift(cx, cy - 1)) * 0.5f;
  float down = (dcx + dcy) * 0.5f;   // lift change per step down the screen
  float right = (dcx - dcy) * 0.5f;  // ... per step to the right
  using namespace Config::Look;
  // High ground is a little lighter, low ground darker: plateaus separate
  // into value bands even under the look's dark floor.
  float height = TERRAIN_HEIGHT_SHADE * lift(cx, cy);
  float shade = 1.0f + height + TERRAIN_FACE_SHADE * std::min(0.0f, down) +
                TERRAIN_BACK_SHADE * std::max(0.0f, down) +
                TERRAIN_SIDE_SHADE * right;
  return std::clamp(shade, TERRAIN_SHADE_MIN, TERRAIN_SHADE_MAX);
}

void TileRenderer::renderBatch() {
  if (batchVertices.empty()) {
    return;
  }

  // Ensure proper OpenGL state for tile rendering
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(shaderProgram);

  // Set projection matrix
  glm::mat4 projection =
      glm::ortho(Config::Screen::ORTHO_LEFT, Config::Screen::ORTHO_RIGHT,
                 Config::Screen::ORTHO_BOTTOM, Config::Screen::ORTHO_TOP,
                 Config::Screen::ORTHO_NEAR, Config::Screen::ORTHO_FAR);
  glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, &projection[0][0]);

  // Update camera uniforms every frame
  glUniform2f(uniformCameraPos, camera->x, camera->y);
  glUniform1f(uniformZoom, camera->zoom);
  glUniform2f(uniformScreenCenter, camera->screenWidth / 2.0f,
              camera->screenHeight / 2.0f);

  // The look: dark floor lit by the light map, when a scene is open.
  bool lookOn = presentation && presentation->sceneOpen();
  glUniform1f(uniformLook, lookOn ? 1.0f : 0.0f);
  if (presentation) {
    glUniform2f(uniformViewportSize, presentation->targetWidth(),
                presentation->targetHeight());
    presentation->bindLightMap(1);
  } else {
    glUniform2f(uniformViewportSize, static_cast<float>(camera->screenWidth),
                static_cast<float>(camera->screenHeight));
  }

  // Bind texture
  glActiveTexture(GL_TEXTURE0);
  tilesetTexture->bind();

  // Upload vertex data only once (tiles are static in world space)
  if (!verticesUploaded) {
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    size_t requiredCapacity = batchVertices.size();

    // Check if we need to resize the buffer
    if (requiredCapacity > vboCapacity) {
      // Allocate new buffer with actual size needed (not fixed size)
      glBufferData(GL_ARRAY_BUFFER, requiredCapacity * sizeof(float),
                   batchVertices.data(), GL_STATIC_DRAW);
      vboCapacity = requiredCapacity;

      Logger::info(
          "TileRenderer: Allocated GPU buffer for " +
          std::to_string(requiredCapacity / (6 * FLOATS_PER_VERTEX)) +
          " tiles (" +
          std::to_string(requiredCapacity * sizeof(float) / 1024 / 1024) +
          " MB)");
    } else {
      // Buffer is large enough, just update data
      glBufferSubData(GL_ARRAY_BUFFER, 0, requiredCapacity * sizeof(float),
                      batchVertices.data());
    }

    verticesUploaded = true;
  }

  // Draw all tiles in one call
  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLES, 0,
               static_cast<GLsizei>(batchVertices.size() / FLOATS_PER_VERTEX));
  glBindVertexArray(0);

  OpenGLUtils::checkGLError("TileRenderer::renderBatch");
}

void TileRenderer::gridToWorld(const TiledMap& map, int tileX, int tileY,
                               float& worldX, float& worldY) {
  // Tiled's isometric grid-to-world formula
  int tileWidth = map.getTileWidth();
  int tileHeight = map.getTileHeight();

  // Calculate position using Tiled's formula
  // Using tileHeight/4 for Y to increase vertical overlap for thick isometric
  // tiles
  worldX = (tileX - tileY) * tileWidth / 2.0f;
  worldY = (tileX + tileY) * tileHeight / 4.0f;

  // Center the map at origin
  // The map's bounding box center needs to be at (0, 0)
  int mapWidth = map.getWidth();
  int mapHeight = map.getHeight();

  // Map center in grid space
  float centerTileX = (mapWidth - 1) / 2.0f;
  float centerTileY = (mapHeight - 1) / 2.0f;

  // Map center in world space (before offset)
  float centerWorldX = (centerTileX - centerTileY) * tileWidth / 2.0f;
  float centerWorldY = (centerTileX + centerTileY) * tileHeight / 4.0f;

  // Offset to center the map at origin
  worldX -= centerWorldX;
  worldY -= centerWorldY;
}
