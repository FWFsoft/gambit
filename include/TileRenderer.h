#pragma once

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif

#include <functional>
#include <glm/glm.hpp>
#include <vector>

#include "Camera.h"
#include "SpriteRenderer.h"
#include "Texture.h"
#include "TiledMap.h"

class PresentationLayer;

using PlayerRenderCallback =
    std::function<void(float minDepth, float maxDepth)>;

class TileRenderer {
 public:
  TileRenderer(Camera* camera, SpriteRenderer* spriteRenderer,
               Texture* whitePixel);
  ~TileRenderer();
  void render(const TiledMap& map, PlayerRenderCallback playerCallback);

  // With a presentation layer the floor takes the look's world values and
  // is lit by its light map whenever a scene is open.
  void setPresentation(const PresentationLayer* layer) { presentation = layer; }

 private:
  Camera* camera;
  SpriteRenderer* spriteRenderer;
  Texture* whitePixel;
  Texture* tilesetTexture;  // Managed by TextureManager, not owned
  const PresentationLayer* presentation = nullptr;  // not owned

  // Batched rendering
  GLuint shaderProgram;
  GLuint VAO, VBO;
  GLint uniformProjection;
  GLint uniformCameraPos;
  GLint uniformScreenCenter;
  GLint uniformZoom;
  GLint uniformLook;
  GLint uniformViewportSize;
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
