#pragma once

#include <cstdint>
#include <vector>

// Fog of war: what the crew can see right now, and what it has seen before.
//
// Sight is shared: every source (a hero, the ship) reveals a circle around
// itself, and the whole crew sees the union. A place is in one of three
// states:
//   seen now   clear; enemies and pickups there are drawn.
//   explored   seen earlier: the ground stays known but dimmed, and whatever
//              walks there is hidden.
//   unexplored never seen: hidden.
//
// This class is the model only (no rendering): ArenaView asks it what to
// draw and turns texels() into a texture. Sight does not stop at walls yet.
//
// The fog is a presentation of state every client already has (the server
// still sends all enemies). It decides what a player is shown, it is not an
// anti-cheat boundary.
class FogOfWar {
 public:
  struct Source {
    float x, y;
    float radius;  // full sight reaches this far, minus the soft edge
  };

  // The world is centred on the origin (as TiledMap lays it out).
  FogOfWar(float worldWidth, float worldHeight);

  // Forget everything explored (a new match).
  void reset();

  // Once per frame: who gives sight this frame. Marks what they see as
  // explored and rebuilds texels().
  void update(const std::vector<Source>& sources);

  // 0 = not in sight, 1 = fully seen, in between on the soft edge.
  // `reach` is the size of the thing asked about: a telegraph of radius 80
  // is seen as soon as its rim enters sight, not only its centre.
  float visibility(float x, float y, float reach = 0.0f) const;
  // Whether something standing here should be drawn.
  bool reveals(float x, float y, float reach = 0.0f) const;
  // 0 = never seen .. 1 = has been fully seen.
  float explored(float x, float y) const;

  // The fog as an RGBA8 image, gridWidth() x gridHeight(), row-major from
  // the top-left of the world. Alpha is how strongly fog covers that cell.
  const std::vector<uint8_t>& texels() const { return pixels; }
  int gridWidth() const { return columns; }
  int gridHeight() const { return rows; }
  // World rectangle the image covers (a little larger than the map).
  float originX() const { return left; }
  float originY() const { return top; }
  float spanX() const { return columns * cell; }
  float spanY() const { return rows * cell; }

 private:
  float cell;
  float left, top;
  int columns, rows;
  std::vector<Source> current;
  std::vector<float> seen;      // per cell, highest visibility ever
  std::vector<uint8_t> pixels;  // RGBA8

  static float sight(const Source& source, float x, float y, float reach);
};
