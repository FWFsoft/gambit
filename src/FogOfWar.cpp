#include "FogOfWar.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "config/CameraConfig.h"

namespace {
namespace Tune = Config::CameraView;

float smoothstep(float t) {
  t = std::max(0.0f, std::min(1.0f, t));
  return t * t * (3.0f - 2.0f * t);
}
}  // namespace

FogOfWar::FogOfWar(float worldWidth, float worldHeight) : cell(Tune::FOG_CELL) {
  assert(worldWidth > 0.0f && worldHeight > 0.0f);
  assert(cell > 0.0f);
  // Two cells of margin, so the map edge is not the texture edge.
  columns = static_cast<int>(std::ceil(worldWidth / cell)) + 4;
  rows = static_cast<int>(std::ceil(worldHeight / cell)) + 4;
  assert(columns > 0 && rows > 0);
  assert(columns <= 4096 && rows <= 4096 &&
         "fog grid too large: raise FOG_CELL");
  left = -columns * cell * 0.5f;
  top = -rows * cell * 0.5f;
  seen.assign(static_cast<size_t>(columns) * rows, 0.0f);
  pixels.assign(seen.size() * 4, 0);
  update({});
}

void FogOfWar::reset() {
  std::fill(seen.begin(), seen.end(), 0.0f);
  update({});
}

float FogOfWar::sight(const Source& source, float x, float y, float reach) {
  if (source.radius <= 0.0f) return 0.0f;
  assert(reach >= 0.0f);
  const float dx = x - source.x;
  const float dy = y - source.y;
  const float distance = std::max(0.0f, std::sqrt(dx * dx + dy * dy) - reach);
  const float soft = std::min(Tune::SIGHT_SOFT_EDGE, source.radius);
  return smoothstep((source.radius - distance) / soft);
}

float FogOfWar::visibility(float x, float y, float reach) const {
  float best = 0.0f;
  for (const Source& source : current) {
    best = std::max(best, sight(source, x, y, reach));
  }
  return best;
}

bool FogOfWar::reveals(float x, float y, float reach) const {
  return visibility(x, y, reach) >= Tune::REVEAL_THRESHOLD;
}

float FogOfWar::explored(float x, float y) const {
  const int column = static_cast<int>(std::floor((x - left) / cell));
  const int row = static_cast<int>(std::floor((y - top) / cell));
  if (column < 0 || column >= columns || row < 0 || row >= rows) return 0.0f;
  return seen[static_cast<size_t>(row) * columns + column];
}

void FogOfWar::update(const std::vector<Source>& sources) {
  current = sources;
  assert(seen.size() == static_cast<size_t>(columns) * rows);

  for (int row = 0; row < rows; row++) {
    const float y = top + (row + 0.5f) * cell;
    for (int column = 0; column < columns; column++) {
      const float x = left + (column + 0.5f) * cell;
      const size_t index = static_cast<size_t>(row) * columns + column;
      const float now = visibility(x, y);
      seen[index] = std::max(seen[index], now);
      const float cover =
          Tune::FOG_UNEXPLORED +
          (Tune::FOG_EXPLORED - Tune::FOG_UNEXPLORED) * seen[index];
      const float alpha = (1.0f - now) * cover;
      uint8_t* pixel = &pixels[index * 4];
      pixel[0] = static_cast<uint8_t>(Tune::FOG_R);
      pixel[1] = static_cast<uint8_t>(Tune::FOG_G);
      pixel[2] = static_cast<uint8_t>(Tune::FOG_B);
      pixel[3] = static_cast<uint8_t>(alpha * 255.0f + 0.5f);
    }
  }
}
