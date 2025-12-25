#pragma once

#include <glad/glad.h>

#include <string>

namespace OpenGLUtils {

// Compile a shader from source code
GLuint compileShader(GLenum type, const char* source);

// Link vertex and fragment shaders into a program
GLuint createShaderProgram(const char* vertexSource,
                           const char* fragmentSource);

// Check for OpenGL errors and log them
void checkGLError(const char* operation);

}  // namespace OpenGLUtils
