#pragma once

#include <glad/glad.h>

#include <functional>
#include <glm/glm.hpp>
#include <vector>

#include "Camera.h"
#include "SpriteRenderer.h"
#include "Texture.h"
#include "TiledMap.h"

using PlayerRenderCallback =
    std::function<void(float minDepth, float maxDepth)>;

class TileRenderer {
 public:
  TileRenderer(Camera* camera, SpriteRenderer* spriteRenderer,
               Texture* whitePixel);
  ~TileRenderer();
  void render(const TiledMap& map, PlayerRenderCallback playerCallback);

 private:
  Camera* camera;
  SpriteRenderer* spriteRenderer;
  Texture* whitePixel;
  Texture* tilesetTexture;  // Managed by TextureManager, not owned

  // Batched rendering
  GLuint shaderProgram;
  GLuint VAO, VBO;
  GLint uniformProjection;
  GLint uniformCameraPos;
  GLint uniformScreenCenter;
  std::vector<float> batchVertices;
  bool batchBuilt;        // Cache flag - tiles are static
  bool verticesUploaded;  // Cache flag - vertices uploaded to GPU
  size_t vboCapacity;     // Current VBO buffer capacity in floats

  void initBatchRenderer();
  void buildTileBatch(const TiledMap& map);
  void renderBatch();
  void gridToWorld(const TiledMap& map, int tileX, int tileY, float& worldX,
                   float& worldY);
};
