#include "TextureManager.h"

#include "Logger.h"

TextureManager& TextureManager::instance() {
  static TextureManager instance;
  return instance;
}

Texture* TextureManager::get(const std::string& path) {
  // Check if already cached
  auto it = textures.find(path);
  if (it != textures.end()) {
    return it->second.get();
  }

  // Load new texture
  auto texture = std::make_unique<Texture>();
  if (!texture->loadFromFile(path)) {
    Logger::error("TextureManager: Failed to load texture: " + path);
    return nullptr;
  }

  // Cache and return
  Texture* ptr = texture.get();
  textures[path] = std::move(texture);
  return ptr;
}

void TextureManager::clear() {
  textures.clear();
  Logger::info("TextureManager: Cleared all textures");
}
