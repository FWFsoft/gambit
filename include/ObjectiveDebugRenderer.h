#pragma once

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif

#include "Camera.h"
#include "ClientPrediction.h"

class ObjectiveDebugRenderer {
 public:
  ObjectiveDebugRenderer(Camera* camera, ClientPrediction* clientPrediction);
  ~ObjectiveDebugRenderer();

  void render();
  void setEnabled(bool enabled) { this->enabled = enabled; }
  bool isEnabled() const { return enabled; }
  void toggle() { enabled = !enabled; }

 private:
  Camera* camera;
  ClientPrediction* clientPrediction;
  bool enabled;

  // OpenGL rendering state
  GLuint shaderProgram;
  GLuint VAO;
  GLuint VBO;

  void initRenderData();
  void renderObjective(const ClientObjective& objective);
  void renderCircle(float centerX, float centerY, float radius, uint8_t r,
                    uint8_t g, uint8_t b, uint8_t a);
};
