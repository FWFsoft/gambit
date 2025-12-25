#pragma once

#include <functional>

#include "Camera.h"
#include "TiledMap.h"

using PlayerRenderCallback =
    std::function<void(float minDepth, float maxDepth)>;

class TileRenderer {
 public:
  TileRenderer(Camera* camera);
  void render(const TiledMap& map, PlayerRenderCallback playerCallback);

 private:
  Camera* camera;

  void renderTile(const TiledMap& map, int tileX, int tileY, uint32_t gid);
  void gridToWorld(const TiledMap& map, int tileX, int tileY, float& worldX,
                   float& worldY);
};
