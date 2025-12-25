#pragma once

#include <glad/glad.h>

#include <string>

class Texture {
 public:
  Texture();
  ~Texture();

  // Load texture from file (PNG, JPG, BMP, etc.)
  bool loadFromFile(const std::string& path);

  // Create a simple 1x1 white texture (useful for colored rectangles)
  bool createWhitePixel();

  // Bind this texture for rendering
  void bind() const;

  // Unbind texture
  static void unbind();

  // Getters
  GLuint getID() const { return textureID; }
  int getWidth() const { return width; }
  int getHeight() const { return height; }
  bool isLoaded() const { return textureID != 0; }

 private:
  GLuint textureID;
  int width;
  int height;
};
