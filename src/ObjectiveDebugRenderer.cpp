#include "ObjectiveDebugRenderer.h"

#include <glad/glad.h>

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "Logger.h"
#include "OpenGLUtils.h"
#include "config/ScreenConfig.h"

// Simple line vertex shader
const char* objectiveVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;

uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
}
)";

// Simple line fragment shader
const char* objectiveFragmentShader = R"(
#version 330 core
out vec4 FragColor;

uniform vec4 lineColor;

void main() {
    FragColor = lineColor;
}
)";

ObjectiveDebugRenderer::ObjectiveDebugRenderer(
    Camera* camera, ClientPrediction* clientPrediction)
    : camera(camera), clientPrediction(clientPrediction), enabled(false) {
  // Create shader program for line rendering
  shaderProgram = OpenGLUtils::createShaderProgram(objectiveVertexShader,
                                                   objectiveFragmentShader);

  // Set up orthographic projection
  glm::mat4 projection =
      glm::ortho(Config::Screen::ORTHO_LEFT, Config::Screen::ORTHO_RIGHT,
                 Config::Screen::ORTHO_BOTTOM, Config::Screen::ORTHO_TOP,
                 Config::Screen::ORTHO_NEAR, Config::Screen::ORTHO_FAR);

  glUseProgram(shaderProgram);
  GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
  glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);

  initRenderData();

  Logger::info("ObjectiveDebugRenderer initialized");
}

ObjectiveDebugRenderer::~ObjectiveDebugRenderer() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteProgram(shaderProgram);
}

void ObjectiveDebugRenderer::initRenderData() {
  // Create VAO/VBO for dynamic line data
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);

  // Allocate buffer for circle drawing (64 segments = 128 vertices = 256
  // floats) Add extra capacity for safety
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 512, nullptr, GL_DYNAMIC_DRAW);

  // Position attribute
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void ObjectiveDebugRenderer::render() {
  if (!enabled || !clientPrediction) {
    return;
  }

  const auto& objectives = clientPrediction->getObjectives();

  if (objectives.empty()) {
    Logger::debug(
        "ObjectiveDebugRenderer: No objectives to render (map is empty)");
    return;
  }

  Logger::debug("ObjectiveDebugRenderer: Rendering " +
                std::to_string(objectives.size()) + " objectives");

  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Note: glLineWidth() is deprecated in OpenGL Core Profile (macOS)
  // Line width other than 1.0 causes GL_INVALID_VALUE on macOS
  // For thicker lines, we'd need to use geometry shaders or quad rendering

  for (const auto& [id, objective] : objectives) {
    renderObjective(objective);
  }

  OpenGLUtils::checkGLError("ObjectiveDebugRenderer::render");
}

void ObjectiveDebugRenderer::renderObjective(const ClientObjective& objective) {
  // Choose color based on state
  uint8_t r, g, b, a;
  switch (objective.state) {
    case ObjectiveState::Inactive:
      r = 255;
      g = 255;
      b = 51;  // Bright yellow
      a = 255;
      break;
    case ObjectiveState::InProgress:
      r = 51;
      g = 153;
      b = 255;  // Bright blue
      a = 255;
      break;
    case ObjectiveState::Completed:
      r = 51;
      g = 255;
      b = 51;  // Bright green
      a = 255;
      break;
    default:
      r = 128;
      g = 128;
      b = 128;  // Gray
      a = 255;
      break;
  }

  // Draw circle for objective zone
  renderCircle(objective.x, objective.y, objective.radius, r, g, b, a);

  // Draw center marker (cross)
  int centerScreenX, centerScreenY;
  camera->worldToScreen(objective.x, objective.y, centerScreenX, centerScreenY);

  // Draw cross at center
  float crossSize = 10.0f;
  std::vector<float> crossVertices = {
      // Horizontal line
      centerScreenX - crossSize, static_cast<float>(centerScreenY),
      centerScreenX + crossSize, static_cast<float>(centerScreenY),
      // Vertical line
      static_cast<float>(centerScreenX), centerScreenY - crossSize,
      static_cast<float>(centerScreenX), centerScreenY + crossSize};

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferSubData(GL_ARRAY_BUFFER, 0, crossVertices.size() * sizeof(float),
                  crossVertices.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glUseProgram(shaderProgram);
  GLint colorLoc = glGetUniformLocation(shaderProgram, "lineColor");
  glUniform4f(colorLoc, r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);

  glBindVertexArray(VAO);
  glDrawArrays(GL_LINES, 0, 4);  // 4 vertices = 2 lines (cross)
  glBindVertexArray(0);

  // Log objective info (only once per frame when debug is enabled)
  static int frameCounter = 0;
  if (frameCounter++ % 60 == 0) {  // Log once per second at 60 FPS
    Logger::debug(
        "Objective #" + std::to_string(objective.id) +
        " type=" + std::to_string(static_cast<int>(objective.type)) +
        " state=" + std::to_string(static_cast<int>(objective.state)) +
        " pos=(" + std::to_string(objective.x) + ", " +
        std::to_string(objective.y) + ")" + " screen=(" +
        std::to_string(centerScreenX) + ", " + std::to_string(centerScreenY) +
        ")");
  }
}

void ObjectiveDebugRenderer::renderCircle(float centerX, float centerY,
                                          float radius, uint8_t r, uint8_t g,
                                          uint8_t b, uint8_t a) {
  constexpr int NUM_SEGMENTS = 64;
  std::vector<float> vertices;
  vertices.reserve(NUM_SEGMENTS * 2 * 2);  // 2 floats per vertex, 2 vertices
                                           // per segment (for lines)

  for (int i = 0; i < NUM_SEGMENTS; i++) {
    float angle1 = (i / static_cast<float>(NUM_SEGMENTS)) * 2.0f * M_PI;
    float angle2 = ((i + 1) / static_cast<float>(NUM_SEGMENTS)) * 2.0f * M_PI;

    // Calculate circle points in world space
    float x1 = centerX + cos(angle1) * radius;
    float y1 = centerY + sin(angle1) * radius;
    float x2 = centerX + cos(angle2) * radius;
    float y2 = centerY + sin(angle2) * radius;

    // Convert to screen space
    int screenX1, screenY1, screenX2, screenY2;
    camera->worldToScreen(x1, y1, screenX1, screenY1);
    camera->worldToScreen(x2, y2, screenX2, screenY2);

    // Add line segment
    vertices.push_back(static_cast<float>(screenX1));
    vertices.push_back(static_cast<float>(screenY1));
    vertices.push_back(static_cast<float>(screenX2));
    vertices.push_back(static_cast<float>(screenY2));
  }

  // Safety check: ensure we don't exceed buffer size
  size_t maxFloats = 512;  // Match buffer allocation size
  if (vertices.size() > maxFloats) {
    Logger::error("ObjectiveDebugRenderer: vertices.size() (" +
                  std::to_string(vertices.size()) +
                  ") exceeds buffer capacity (" + std::to_string(maxFloats) +
                  ")");
    return;
  }

  // Upload vertices
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float),
                  vertices.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Set line color
  glUseProgram(shaderProgram);
  GLint colorLoc = glGetUniformLocation(shaderProgram, "lineColor");
  glUniform4f(colorLoc, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);

  // Draw circle
  glBindVertexArray(VAO);
  glDrawArrays(
      GL_LINES, 0,
      static_cast<GLsizei>(vertices.size() / 2));  // 2 floats per vertex
  glBindVertexArray(0);
}
