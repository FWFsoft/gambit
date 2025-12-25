#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "Texture.h"

class TextureManager {
 public:
  static TextureManager& instance();

  // Get texture by path, loading it if not already cached
  Texture* get(const std::string& path);

  // Clear all cached textures
  void clear();

 private:
  TextureManager() = default;
  ~TextureManager() = default;
  TextureManager(const TextureManager&) = delete;
  TextureManager& operator=(const TextureManager&) = delete;

  std::unordered_map<std::string, std::unique_ptr<Texture>> textures;
};
