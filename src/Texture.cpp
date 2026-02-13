#include "Texture.h"

#include <SDL2/SDL_image.h>
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif

#include "Logger.h"

Texture::Texture() : textureID(0), width(0), height(0) {}

Texture::~Texture() {
  if (textureID != 0) {
    glDeleteTextures(1, &textureID);
  }
}

bool Texture::loadFromFile(const std::string& path) {
  // Load image using SDL2_image
  SDL_Surface* surface = IMG_Load(path.c_str());
  if (!surface) {
    Logger::error("Failed to load texture: " + path + " - " +
                  std::string(IMG_GetError()));
    return false;
  }

  // Determine format
  GLenum format = GL_RGB;
  if (surface->format->BytesPerPixel == 4) {
    format = GL_RGBA;
  }

  width = surface->w;
  height = surface->h;

  // Generate OpenGL texture
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);

  // Upload pixel data to GPU
  glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
               GL_UNSIGNED_BYTE, surface->pixels);

  // Set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Clean up
  SDL_FreeSurface(surface);
  glBindTexture(GL_TEXTURE_2D, 0);

  Logger::info("Loaded texture: " + path + " (" + std::to_string(width) + "x" +
               std::to_string(height) + ")");
  return true;
}

bool Texture::createWhitePixel() {
  width = 1;
  height = 1;

  // Create a 1x1 white pixel
  unsigned char whitePixel[] = {255, 255, 255, 255};

  // Generate OpenGL texture
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);

  // Upload pixel data
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               whitePixel);

  // Set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_2D, 0);

  Logger::info("Created 1x1 white pixel texture");
  return true;
}

void Texture::bind() const { glBindTexture(GL_TEXTURE_2D, textureID); }

void Texture::unbind() { glBindTexture(GL_TEXTURE_2D, 0); }
