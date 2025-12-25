#pragma once

#include <glad/glad.h>

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

  // Draw a colored rectangle (no texture)
  void drawRect(float x, float y, float width, float height, float r = 1.0f,
                float g = 1.0f, float b = 1.0f, float a = 1.0f);

 private:
  GLuint shaderProgram;
  GLuint VAO;
  GLuint VBO;

  void initRenderData();
};
