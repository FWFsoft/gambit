#pragma once

#include <cassert>
#include <cstdint>

// The planet's biomes: earth-like places, the Biome layer of the map
// (TerrainGrid::biomeAt). Geography, not genus: the genera live across
// them. Ids match tools/maps/generate_planet.py (BIOMES) and the map's
// "biomes" property. See docs/design/MAP_PIPELINE.md, "Biomes".
enum class Biome : uint8_t {
  None = 0,  // sea, void, off the map, maps without biomes
  Plains = 1,
  PacificNorthwest = 2,
  Arctic = 3,
  Desert = 4,
  Tidal = 5,
  City = 6,
  Jungle = 7,
  Swamp = 8,
};

constexpr int BIOME_KINDS = 9;  // None plus the eight biomes

inline Biome biomeFromId(int id) {
  return (id > 0 && id < BIOME_KINDS) ? static_cast<Biome>(id) : Biome::None;
}

inline const char* biomeName(Biome b) {
  switch (b) {
    case Biome::Plains:
      return "Plains";
    case Biome::PacificNorthwest:
      return "Pacific Northwest";
    case Biome::Arctic:
      return "Arctic";
    case Biome::Desert:
      return "Desert";
    case Biome::Tidal:
      return "Tidal coast";
    case Biome::City:
      return "City";
    case Biome::Jungle:
      return "Jungle";
    case Biome::Swamp:
      return "Swamp";
    case Biome::None:
      return "None";
  }
  assert(false && "unknown biome");
  return "None";
}
