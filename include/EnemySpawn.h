#pragma once

#include <cstdint>
#include <string>

#include "Enemy.h"

struct EnemySpawn {
  EnemyType type;    // Type of enemy to spawn
  float x, y;        // Spawn position
  std::string name;  // Spawn point name (for debugging)
  bool elite = false;  // spawns as the kind's elite (map property "elite")
  // The landing sites it spawns for, one bit per site (map property
  // "sites", e.g. "0,2": in reach of sites 0 and 2); 0 = every match. See
  // TiledMap::getCrashSites.
  uint32_t sites = 0;
  bool activeFor(int site) const {
    return sites == 0 || site < 0 || ((sites >> site) & 1u) != 0;
  }

  // Future: spawn rate, max count, respawn timer
};
