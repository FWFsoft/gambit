#include "MusicZoneDebugRenderer.h"

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "OpenGLUtils.h"
#include "config/ScreenConfig.h"

// Reuse simple shader from CollisionDebugRenderer
const char* zoneVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;

uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
}
)";

const char* zoneFragmentShader = R"(
#version 330 core
out vec4 FragColor;

uniform vec4 fillColor;

void main() {
    FragColor = fillColor;
}
)";

MusicZoneDebugRenderer::MusicZoneDebugRenderer(Camera* camera,
                                               const TiledMap* tiledMap)
    : camera(camera), tiledMap(tiledMap), enabled(false) {
  // Create shader program for filled rect rendering
  shaderProgram =
      OpenGLUtils::createShaderProgram(zoneVertexShader, zoneFragmentShader);

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

MusicZoneDebugRenderer::~MusicZoneDebugRenderer() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteProgram(shaderProgram);
}

void MusicZoneDebugRenderer::initRenderData() {
  // Create VAO/VBO for dynamic rect data
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);

  // Buffer for 4 vertices (quad)
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 2 * 4, nullptr,
               GL_DYNAMIC_DRAW);

  // Position attribute
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void MusicZoneDebugRenderer::render() {
  if (!enabled || !tiledMap) {
    return;
  }

  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  const auto& zones = tiledMap->getMusicZones();
  for (size_t i = 0; i < zones.size(); ++i) {
    renderZone(zones[i], i);
  }

  glDisable(GL_BLEND);
}

void MusicZoneDebugRenderer::renderZone(const MusicZone& zone, size_t index) {
  // Assign different colors to each zone
  uint8_t r, g, b;
  switch (index % 6) {
    case 0:
      r = 200;
      g = 100;
      b = 255;
      break;  // Purple
    case 1:
      r = 100;
      g = 200;
      b = 255;
      break;  // Cyan
    case 2:
      r = 255;
      g = 200;
      b = 100;
      break;  // Orange
    case 3:
      r = 100;
      g = 255;
      b = 150;
      break;  // Green
    case 4:
      r = 255;
      g = 100;
      b = 150;
      break;  // Pink
    case 5:
      r = 200;
      g = 255;
      b = 100;
      break;  // Yellow-green
    default:
      r = 200;
      g = 200;
      b = 200;
      break;  // Gray fallback
  }

  renderFilledRect(zone.x, zone.y, zone.width, zone.height, r, g, b, 80);
}

void MusicZoneDebugRenderer::renderFilledRect(float x, float y, float w,
                                              float h, uint8_t r, uint8_t g,
                                              uint8_t b, uint8_t a) {
  // Convert world corners to screen coordinates
  int screenX1, screenY1, screenX2, screenY2, screenX3, screenY3, screenX4,
      screenY4;

  // Top-left corner
  camera->worldToScreen(x, y, screenX1, screenY1);

  // Top-right corner
  camera->worldToScreen(x + w, y, screenX2, screenY2);

  // Bottom-right corner
  camera->worldToScreen(x + w, y + h, screenX3, screenY3);

  // Bottom-left corner
  camera->worldToScreen(x, y + h, screenX4, screenY4);

  // Build quad vertices (2 triangles)
  float vertices[] = {
      (float)screenX1, (float)screenY1,  // Top-left
      (float)screenX2, (float)screenY2,  // Top-right
      (float)screenX3, (float)screenY3,  // Bottom-right
      (float)screenX4, (float)screenY4   // Bottom-left
  };

  // Upload vertex data
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Set fill color (with alpha for transparency)
  glUseProgram(shaderProgram);
  GLint colorLoc = glGetUniformLocation(shaderProgram, "fillColor");
  glUniform4f(colorLoc, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);

  // Draw filled quad
  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);  // 4 vertices = 1 quad
  glBindVertexArray(0);

  OpenGLUtils::checkGLError("MusicZoneDebugRenderer::renderFilledRect");
}
