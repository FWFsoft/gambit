#pragma once

#include <string>

#include "Enemy.h"

struct EnemySpawn {
  EnemyType type;    // Type of enemy to spawn
  float x, y;        // Spawn position
  std::string name;  // Spawn point name (for debugging)
  bool elite = false;  // spawns as the kind's elite (map property "elite")

  // Future: spawn rate, max count, respawn timer
};
