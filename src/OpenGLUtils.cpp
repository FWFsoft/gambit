#include "OpenGLUtils.h"

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif

#include <stdexcept>
#include <vector>

#include "Logger.h"

namespace OpenGLUtils {

GLuint compileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  // Check for compilation errors
  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    GLint logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(logLength);
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error("Shader compilation failed: " +
                             std::string(log.data()));
  }

  return shader;
}

GLuint createShaderProgram(const char* vertexSource,
                           const char* fragmentSource) {
  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
  GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);

  // Check for linking errors
  GLint success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    GLint logLength;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(logLength);
    glGetProgramInfoLog(program, logLength, nullptr, log.data());
    glDeleteProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    throw std::runtime_error("Shader program linking failed: " +
                             std::string(log.data()));
  }

  // Clean up shaders (they're linked into the program now)
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return program;
}

void checkGLError(const char* operation) {
#if defined(__EMSCRIPTEN__) && !defined(GAMBIT_WEBGL_ERROR_CHECKS)
  // In WebGL glGetError() is a synchronous round trip to the GPU process
  // (measured ~0.3 ms each, ~1.3 ms a frame). Build with
  // -DGAMBIT_WEBGL_ERROR_CHECKS to turn the checks back on when debugging.
  (void)operation;
  return;
#endif
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    Logger::error("OpenGL error after " + std::string(operation) + ": " +
                  std::to_string(error));
  }
}

}  // namespace OpenGLUtils
