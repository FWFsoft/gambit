// The generated planet as the engine loads it (assets/maps/planet.tmx, from
// tools/maps/generate_planet.py): every data layer decodes to the values the
// generator wrote. Guards the loader's tileset-row decoding: regions once all
// read as region 15 (one genus for the whole planet) because the loader
// ignored which row of terrain_data a layer paints with.

#include <cassert>
#include <iostream>
#include <map>
#include <set>

#include "Biome.h"
#include "Logger.h"
#include "TiledMap.h"

int main() {
  Logger::init();
  TiledMap map;
  bool loaded = map.load("assets/maps/planet.tmx");
  assert(loaded);
  const TerrainGrid& grid = map.terrain();
  assert(grid.active());

  std::set<int> regions;
  std::map<int, int> biomeCells;
  // Region -> the biomes it covers. Territories nest inside biomes.
  std::map<int, std::set<int>> regionBiomes;
  for (int ty = 0; ty < grid.height(); ty++) {
    for (int tx = 0; tx < grid.width(); tx++) {
      int region = grid.regionOf(tx, ty);
      int biome = grid.biomeOf(tx, ty);
      assert(region >= 0 && region < TerrainGrid::REGION_COUNT);
      assert(biome >= 0 && biome < BIOME_KINDS);
      biomeCells[biome]++;
      if (region > 0) {
        regions.insert(region);
        regionBiomes[region].insert(biome);
        // Land in a territory always has a biome, and the other way round.
        assert(biome != 0);
      }
      if (biome != 0) assert(region != 0);
    }
  }

  // Many territories, not one (the old decoding bug).
  assert(regions.size() >= 10);
  for (const auto& [region, biomes] : regionBiomes) {
    if (biomes.size() != 1) {
      std::cerr << "region " << region << " crosses " << biomes.size()
                << " biomes\n";
    }
    assert(biomes.size() == 1);
  }

  // All eight biomes, each a real place: at least 3% of the land.
  int land = 0;
  for (const auto& [biome, cells] : biomeCells) {
    if (biome != 0) land += cells;
  }
  for (int b = 1; b < BIOME_KINDS; b++) {
    int cells = biomeCells[b];
    std::cout << biomeName(biomeFromId(b)) << ": " << (100.0f * cells / land)
              << "% of land\n";
    assert(cells > land * 3 / 100);
  }

  // The ship landed on land with a biome, not in the sea.
  assert(grid.biomeAt(map.getShipX(), map.getShipY()) != 0);
  assert(!grid.blockedAt(map.getShipX(), map.getShipY() + 200.0f));

  std::cout << "Planet map: " << regions.size() << " territories in "
            << (biomeCells.size() - 1) << " biomes. OK\n";
  return 0;
}
