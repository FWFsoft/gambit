#pragma once

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif

#include "Camera.h"
#include "TiledMap.h"

class MusicZoneDebugRenderer {
 public:
  MusicZoneDebugRenderer(Camera* camera, const TiledMap* tiledMap);
  ~MusicZoneDebugRenderer();

  void render();
  void setEnabled(bool enabled) { this->enabled = enabled; }
  bool isEnabled() const { return enabled; }
  void toggle() { enabled = !enabled; }

 private:
  Camera* camera;
  const TiledMap* tiledMap;
  bool enabled;

  // OpenGL rendering state
  GLuint shaderProgram;
  GLuint VAO;
  GLuint VBO;

  void initRenderData();
  void renderZone(const MusicZone& zone, size_t index);
  void renderFilledRect(float x, float y, float w, float h, uint8_t r,
                        uint8_t g, uint8_t b, uint8_t a);
};
