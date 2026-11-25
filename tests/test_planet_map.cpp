// The generated planet as the engine loads it (assets/maps/planet.tmx, from
// tools/maps/generate_planet.py): every data layer decodes to the values the
// generator wrote. Guards the loader's tileset-row decoding: regions once all
// read as region 15 (one genus for the whole planet) because the loader
// ignored which row of terrain_data a layer paints with.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include "Biome.h"
#include "CollisionSystem.h"
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

  // Landing sites: each match lands on one, bringing its own spawns, hull
  // and turrets. Objectives and camps are fixed places on the planet, in
  // play for every site within reach (ServerGameState::chooseLandingSite).
  const auto& sites = map.getCrashSites();
  assert(sites.size() >= 3);
  CollisionSystem solid(map.getCollisionShapes(), &grid);
  for (int i = 0; i < static_cast<int>(sites.size()); i++) {
    const auto& site = sites[i];
    assert(grid.biomeAt(site.x, site.y) != 0);
    assert(!grid.blockedAt(site.x, site.y + 200.0f));
    int spawns = 0, objectives = 0, camps = 0;
    for (const PlayerSpawn& p : map.getPlayerSpawns()) spawns += p.site == i;
    for (const Objective& o : map.getObjectives()) {
      if (o.sites == 0 || !o.activeFor(i)) continue;
      objectives++;
      // In play for a site means within its reach (the far ring), or a
      // coverage fill objective, in play wherever the ship lands.
      bool everywhere = __builtin_popcount(o.sites) ==
                        static_cast<int>(sites.size());
      assert(everywhere || std::hypot(o.x - site.x, o.y - site.y) < 9100.0f);
    }
    for (const EnemySpawn& e : map.getEnemySpawns()) {
      camps += e.sites != 0 && e.activeFor(i);
    }
    assert(spawns >= 4 && objectives >= 20 && camps >= 20);
    // Only this site's hull is solid while the ship stands here.
    solid.placeHullNear(site.x, site.y);
    assert(solid.getHullSite() == i);
    int here = 0, elsewhere = 0;
    for (const CollisionShape& shape : map.getCollisionShapes()) {
      if (CollisionSystem::hullTierOf(shape) != 0) continue;
      int at = CollisionSystem::hullSiteOf(shape);
      assert(at >= 0);  // every hull belongs to a site
      if (solid.isSolid(shape)) {
        assert(at == i);
        here++;
      } else {
        elsewhere++;
      }
    }
    assert(here > 0 && elsewhere > 0);
    std::cout << "site " << i << ": " << objectives << " objectives, " << camps
              << " camp guards, " << spawns << " spawns, "
              << biomeName(biomeFromId(grid.biomeAt(site.x, site.y))) << "\n";
  }

  // Issue #49: wherever the ship lands, no walkable spot is far from an
  // objective in play (straight line; tools/maps/generate_planet.py,
  // COVER_GAP). Sampled every 8 tiles.
  for (int i = 0; i < static_cast<int>(sites.size()); i++) {
    std::vector<const Objective*> live;
    for (const Objective& o : map.getObjectives()) {
      if (o.activeFor(i)) live.push_back(&o);
    }
    float worst = 0.0f;
    for (int ty = 0; ty < grid.height(); ty += 8) {
      for (int tx = 0; tx < grid.width(); tx += 8) {
        if (grid.biomeOf(tx, ty) == 0 || grid.blockedTile(tx, ty)) continue;
        float wx, wy;
        grid.cornerToWorld(tx + 0.5f, ty + 0.5f, wx, wy);
        float best = 1e9f;
        for (const Objective* o : live) {
          best = std::min(best, std::hypot(o->x - wx, o->y - wy));
        }
        worst = std::max(worst, best);
      }
    }
    std::cout << "site " << i << ": farthest walkable spot is " << worst
              << " px from an objective\n";
    assert(worst < 3200.0f);
  }

  // Issue #50: the land ends in a coast, never at the edge of the grid.
  // Nothing walkable within 600 px (square to the edge) of it.
  {
    const float step = 1.0f / std::hypot(1.0f / 64.0f, 1.0f / 32.0f);
    float nearest = 1e9f;
    for (int ty = 0; ty < grid.height(); ty++) {
      for (int tx = 0; tx < grid.width(); tx++) {
        if (grid.blockedTile(tx, ty)) continue;
        int inner = std::min(std::min(tx, ty), std::min(grid.width() - 1 - tx,
                                                        grid.height() - 1 - ty));
        nearest = std::min(nearest, (inner + 0.5f) * step);
      }
    }
    std::cout << "nearest walkable tile to the map's edge: " << nearest
              << " px\n";
    assert(nearest >= 600.0f);
  }

  // Fixed places: some objectives are in play from more than one site.
  int shared = 0;
  for (const Objective& o : map.getObjectives()) {
    shared += __builtin_popcount(o.sites) > 1;
  }
  assert(shared > 0);
  std::cout << shared << " objectives are in reach of more than one site\n";

  std::cout << "Planet map: " << regions.size() << " territories in "
            << (biomeCells.size() - 1) << " biomes, " << sites.size()
            << " landing sites. OK\n";
  return 0;
}
