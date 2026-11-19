#pragma once

#include <string>

struct PlayerSpawn {
  float x, y;        // Spawn position
  std::string name;  // Spawn point name (for debugging)
  int site = -1;     // landing site (map property "site"); -1 = any
};
