#include "Settings.h"

#include <fstream>
#include <sstream>

#include "Logger.h"

bool Settings::load(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    Logger::info("Settings file not found, using defaults: " + filename);
    return false;
  }

  std::string line;
  while (std::getline(file, line)) {
    // Skip comments and empty lines
    if (line.empty() || line[0] == '#' || line[0] == ';') {
      continue;
    }

    // Parse key=value
    size_t pos = line.find('=');
    if (pos == std::string::npos) {
      continue;
    }

    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);

    // Trim whitespace
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);

    // Audio settings
    if (key == "masterVolume") {
      masterVolume = std::stof(value);
    } else if (key == "musicVolume") {
      musicVolume = std::stof(value);
    } else if (key == "sfxVolume") {
      sfxVolume = std::stof(value);
    } else if (key == "muted") {
      muted = (value == "true" || value == "1");
    }
    // Graphics settings
    else if (key == "windowWidth") {
      windowWidth = std::stoi(value);
    } else if (key == "windowHeight") {
      windowHeight = std::stoi(value);
    } else if (key == "fullscreen") {
      fullscreen = (value == "true" || value == "1");
    } else if (key == "vsync") {
      vsync = (value == "true" || value == "1");
    }
  }

  Logger::info("Settings loaded from: " + filename);
  return true;
}

bool Settings::save(const std::string& filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    Logger::error("Failed to save settings: " + filename);
    return false;
  }

  file << "# Gambit Settings\n\n";

  file << "# Audio\n";
  file << "masterVolume=" << masterVolume << "\n";
  file << "musicVolume=" << musicVolume << "\n";
  file << "sfxVolume=" << sfxVolume << "\n";
  file << "muted=" << (muted ? "true" : "false") << "\n\n";

  file << "# Graphics\n";
  file << "windowWidth=" << windowWidth << "\n";
  file << "windowHeight=" << windowHeight << "\n";
  file << "fullscreen=" << (fullscreen ? "true" : "false") << "\n";
  file << "vsync=" << (vsync ? "true" : "false") << "\n";

  Logger::info("Settings saved to: " + filename);
  return true;
}
