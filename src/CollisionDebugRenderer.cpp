#include "CollisionDebugRenderer.h"

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "OpenGLUtils.h"
#include "config/ScreenConfig.h"

// Simple line vertex shader
const char* lineVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;

uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
}
)";

// Simple line fragment shader
const char* lineFragmentShader = R"(
#version 330 core
out vec4 FragColor;

uniform vec4 lineColor;

void main() {
    FragColor = lineColor;
}
)";

CollisionDebugRenderer::CollisionDebugRenderer(
    Camera* camera, const CollisionSystem* collisionSystem)
    : camera(camera), collisionSystem(collisionSystem), enabled(false) {
  // Create shader program for line rendering
  shaderProgram =
      OpenGLUtils::createShaderProgram(lineVertexShader, lineFragmentShader);

  // Set up orthographic projection
  glm::mat4 projection =
      glm::ortho(Config::Screen::ORTHO_LEFT, Config::Screen::ORTHO_RIGHT,
                 Config::Screen::ORTHO_BOTTOM, Config::Screen::ORTHO_TOP,
                 Config::Screen::ORTHO_NEAR, Config::Screen::ORTHO_FAR);

  glUseProgram(shaderProgram);
  GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
  glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);

  initRenderData();
}

CollisionDebugRenderer::~CollisionDebugRenderer() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteProgram(shaderProgram);
}

void CollisionDebugRenderer::initRenderData() {
  // Create VAO/VBO for dynamic line data
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);

  // We'll update buffer data each frame with actual line positions
  glBufferData(
      GL_ARRAY_BUFFER, sizeof(float) * 2 * 8, nullptr,
      GL_DYNAMIC_DRAW);  // 8 vertices max per AABB (4 lines * 2 points)

  // Position attribute
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void CollisionDebugRenderer::render() {
  if (!enabled || !collisionSystem) {
    return;
  }

  const auto& shapes = collisionSystem->getShapes();
  for (const auto& shape : shapes) {
    renderShape(shape);
  }
}

void CollisionDebugRenderer::renderMapBounds(float worldWidth,
                                             float worldHeight) {
  if (!enabled) {
    return;
  }

  // Draw diamond-shaped map bounds
  // Diamond corners: top (0, -halfH), right (halfW, 0), bottom (0, halfH), left
  // (-halfW, 0)
  float halfW = worldWidth / 2.0f;
  float halfH = worldHeight / 2.0f;

  int screenX1, screenY1, screenX2, screenY2, screenX3, screenY3, screenX4,
      screenY4;

  // Top corner
  camera->worldToScreen(0, -halfH, screenX1, screenY1);
  // Right corner
  camera->worldToScreen(halfW, 0, screenX2, screenY2);
  // Bottom corner
  camera->worldToScreen(0, halfH, screenX3, screenY3);
  // Left corner
  camera->worldToScreen(-halfW, 0, screenX4, screenY4);

  // Build line vertices for diamond outline (4 lines)
  float vertices[] = {
      // Line 1: top to right
      (float)screenX1, (float)screenY1, (float)screenX2, (float)screenY2,
      // Line 2: right to bottom
      (float)screenX2, (float)screenY2, (float)screenX3, (float)screenY3,
      // Line 3: bottom to left
      (float)screenX3, (float)screenY3, (float)screenX4, (float)screenY4,
      // Line 4: left to top
      (float)screenX4, (float)screenY4, (float)screenX1, (float)screenY1};

  // Upload line data
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Set line color (yellow for map bounds)
  glUseProgram(shaderProgram);
  GLint colorLoc = glGetUniformLocation(shaderProgram, "lineColor");
  glUniform4f(colorLoc, 1.0f, 1.0f, 0.0f, 1.0f);  // Yellow

  // Draw lines
  glBindVertexArray(VAO);
  glDrawArrays(GL_LINES, 0, 8);  // 8 vertices = 4 lines
  glBindVertexArray(0);

  OpenGLUtils::checkGLError("CollisionDebugRenderer::renderMapBounds");
}

void CollisionDebugRenderer::renderShape(const CollisionShape& shape) {
  if (shape.type == CollisionShape::Type::Rectangle) {
    renderAABB(shape.aabb, 255, 0, 0);  // Red for rectangles
  } else if (shape.type == CollisionShape::Type::Polygon) {
    // Render AABB in yellow for polygons
    renderAABB(shape.aabb, 255, 255, 0);
  }
}

void CollisionDebugRenderer::renderAABB(const AABB& aabb, uint8_t r, uint8_t g,
                                        uint8_t b) {
  // Convert world corners to screen coordinates
  int screenX1, screenY1, screenX2, screenY2, screenX3, screenY3, screenX4,
      screenY4;

  // Top-left corner
  camera->worldToScreen(aabb.x, aabb.y, screenX1, screenY1);

  // Top-right corner
  camera->worldToScreen(aabb.x + aabb.width, aabb.y, screenX2, screenY2);

  // Bottom-right corner
  camera->worldToScreen(aabb.x + aabb.width, aabb.y + aabb.height, screenX3,
                        screenY3);

  // Bottom-left corner
  camera->worldToScreen(aabb.x, aabb.y + aabb.height, screenX4, screenY4);

  // Build line vertices (4 lines forming rectangle outline)
  float vertices[] = {
      // Line 1: top-left to top-right
      (float)screenX1, (float)screenY1, (float)screenX2, (float)screenY2,
      // Line 2: top-right to bottom-right
      (float)screenX2, (float)screenY2, (float)screenX3, (float)screenY3,
      // Line 3: bottom-right to bottom-left
      (float)screenX3, (float)screenY3, (float)screenX4, (float)screenY4,
      // Line 4: bottom-left to top-left
      (float)screenX4, (float)screenY4, (float)screenX1, (float)screenY1};

  // Upload line data
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Set line color
  glUseProgram(shaderProgram);
  GLint colorLoc = glGetUniformLocation(shaderProgram, "lineColor");
  glUniform4f(colorLoc, r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);

  // Draw lines
  glBindVertexArray(VAO);
  glDrawArrays(GL_LINES, 0, 8);  // 8 vertices = 4 lines
  glBindVertexArray(0);

  OpenGLUtils::checkGLError("CollisionDebugRenderer::renderAABB");
}
