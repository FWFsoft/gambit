#pragma once

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif

#include <glm/glm.hpp>

#include "Texture.h"

class SpriteRenderer {
 public:
  SpriteRenderer();
  ~SpriteRenderer();

  // Set the projection matrix (call once per frame or when window resizes)
  void setProjection(const glm::mat4& projection);

  // Draw a textured sprite
  void draw(const Texture& texture, float x, float y, float width, float height,
            float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);

  // Draw a region of a texture (for tile atlases)
  // srcX, srcY, srcW, srcH are in pixel coordinates
  void drawRegion(const Texture& texture, float x, float y, float width,
                  float height, int srcX, int srcY, int srcW, int srcH,
                  float r = 1.0f, float g = 1.0f, float b = 1.0f,
                  float a = 1.0f);

  // Draw a colored rectangle (no texture)
  void drawRect(float x, float y, float width, float height, float r = 1.0f,
                float g = 1.0f, float b = 1.0f, float a = 1.0f);

 private:
  GLuint shaderProgram;
  GLuint VAO;
  GLuint VBO;
  GLuint regionVAO;  // Separate VAO for dynamic region rendering
  GLuint regionVBO;  // Separate VBO to avoid modifying main VBO

  // Cached uniform locations (avoid expensive glGetUniformLocation calls)
  GLint uniformModel;
  GLint uniformColor;
  GLint uniformProjection;

  void initRenderData();
};
