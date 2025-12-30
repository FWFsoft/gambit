#pragma once

#include <string>

struct Settings {
  // Audio settings
  float masterVolume = 1.0f;
  float musicVolume = 1.0f;
  float sfxVolume = 1.0f;
  bool muted = false;

  // Graphics settings
  int windowWidth = 1280;
  int windowHeight = 960;
  bool fullscreen = false;
  bool vsync = true;

  // Save/Load
  bool load(const std::string& filename);
  bool save(const std::string& filename) const;

  // Default filename
  static constexpr const char* DEFAULT_FILENAME = "settings.ini";
};
