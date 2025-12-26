#include "SpriteRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "OpenGLUtils.h"

// Simple sprite vertex shader
const char* spriteVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 projection;
uniform mat4 model;

void main() {
    TexCoord = aTexCoord;
    gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
}
)";

// Simple sprite fragment shader
const char* spriteFragmentShader = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D image;
uniform vec4 spriteColor;

void main() {
    FragColor = spriteColor * texture(image, TexCoord);
}
)";

SpriteRenderer::SpriteRenderer() {
  // Create shader program
  shaderProgram = OpenGLUtils::createShaderProgram(spriteVertexShader,
                                                   spriteFragmentShader);

  // Cache uniform locations (avoid repeated lookups)
  uniformModel = glGetUniformLocation(shaderProgram, "model");
  uniformColor = glGetUniformLocation(shaderProgram, "spriteColor");
  uniformProjection = glGetUniformLocation(shaderProgram, "projection");

  initRenderData();
}

SpriteRenderer::~SpriteRenderer() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteVertexArrays(1, &regionVAO);
  glDeleteBuffers(1, &regionVBO);
  glDeleteProgram(shaderProgram);
}

void SpriteRenderer::initRenderData() {
  // Configure VAO/VBO for a quad (two triangles)
  float vertices[] = {
      // pos      // tex
      0.0f, 1.0f, 0.0f, 1.0f,  // top-left
      1.0f, 0.0f, 1.0f, 0.0f,  // bottom-right
      0.0f, 0.0f, 0.0f, 0.0f,  // bottom-left

      0.0f, 1.0f, 0.0f, 1.0f,  // top-left
      1.0f, 1.0f, 1.0f, 1.0f,  // top-right
      1.0f, 0.0f, 1.0f, 0.0f   // bottom-right
  };

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

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

  // Create separate VAO/VBO for dynamic region rendering
  glGenVertexArrays(1, &regionVAO);
  glGenBuffers(1, &regionVBO);

  glBindBuffer(GL_ARRAY_BUFFER, regionVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), nullptr, GL_DYNAMIC_DRAW);

  glBindVertexArray(regionVAO);

  // Position attribute
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

  // Texture coord attribute
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void*)(2 * sizeof(float)));

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void SpriteRenderer::setProjection(const glm::mat4& projection) {
  glUseProgram(shaderProgram);
  glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, &projection[0][0]);
}

void SpriteRenderer::draw(const Texture& texture, float x, float y, float width,
                          float height, float r, float g, float b, float a) {
  glUseProgram(shaderProgram);

  // Create model matrix (translate + scale)
  glm::mat4 model = glm::mat4(1.0f);
  model = glm::translate(model, glm::vec3(x, y, 0.0f));
  model = glm::scale(model, glm::vec3(width, height, 1.0f));

  // Set uniforms (use cached locations)
  glUniformMatrix4fv(uniformModel, 1, GL_FALSE, &model[0][0]);
  glUniform4f(uniformColor, r, g, b, a);

  // Bind texture
  glActiveTexture(GL_TEXTURE0);
  texture.bind();

  // Draw quad
  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);

  OpenGLUtils::checkGLError("SpriteRenderer::draw");
}

void SpriteRenderer::drawRegion(const Texture& texture, float x, float y,
                                float width, float height, int srcX, int srcY,
                                int srcW, int srcH, float r, float g, float b,
                                float a) {
  // Convert pixel coordinates to UV coordinates (0.0-1.0)
  float u1 = srcX / (float)texture.getWidth();
  float v1 = srcY / (float)texture.getHeight();
  float u2 = (srcX + srcW) / (float)texture.getWidth();
  float v2 = (srcY + srcH) / (float)texture.getHeight();

  // Create vertices with custom UVs
  float vertices[] = {
      // pos      // tex
      0.0f, 1.0f, u1, v2,  // top-left
      1.0f, 0.0f, u2, v1,  // bottom-right
      0.0f, 0.0f, u1, v1,  // bottom-left

      0.0f, 1.0f, u1, v2,  // top-left
      1.0f, 1.0f, u2, v2,  // top-right
      1.0f, 0.0f, u2, v1   // bottom-right
  };

  // Update separate region VBO (doesn't corrupt main sprite VBO)
  glBindBuffer(GL_ARRAY_BUFFER, regionVBO);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glUseProgram(shaderProgram);

  // Create model matrix (translate + scale)
  glm::mat4 model = glm::mat4(1.0f);
  model = glm::translate(model, glm::vec3(x, y, 0.0f));
  model = glm::scale(model, glm::vec3(width, height, 1.0f));

  // Set uniforms (use cached locations)
  glUniformMatrix4fv(uniformModel, 1, GL_FALSE, &model[0][0]);
  glUniform4f(uniformColor, r, g, b, a);

  // Bind texture
  glActiveTexture(GL_TEXTURE0);
  texture.bind();

  // Draw quad using region VAO
  glBindVertexArray(regionVAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);

  OpenGLUtils::checkGLError("SpriteRenderer::drawRegion");
}

void SpriteRenderer::drawRect(float x, float y, float width, float height,
                              float r, float g, float b, float a) {
  // For colored rectangles, we can use a 1x1 white texture
  // For now, just skip - we'll implement this in Phase 3 if needed
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  (void)r;
  (void)g;
  (void)b;
  (void)a;
}
