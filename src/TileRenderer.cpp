#include "TileRenderer.h"

TileRenderer::TileRenderer(Camera* camera) : camera(camera) {}

void TileRenderer::render(const TiledMap& map,
                          PlayerRenderCallback playerCallback) {
  const auto& tileLayers = map.getTileLayers();
  if (tileLayers.empty()) {
    return;
  }

  const tmx::TileLayer* layer = tileLayers[0];
  const auto& tiles = layer->getTiles();

  int tileHeight = map.getTileHeight();

  for (int tileY = 0; tileY < map.getHeight(); ++tileY) {
    for (int tileX = 0; tileX < map.getWidth(); ++tileX) {
      int tileIndex = tileY * map.getWidth() + tileX;
      uint32_t gid = tiles[tileIndex].ID;

      if (gid > 0) {
        renderTile(map, tileX, tileY, gid);
      }
    }

    float unit = map.getTileWidth() / 2.0f;
    for (int tileX = 0; tileX < map.getWidth(); ++tileX) {
      float tileWorldX = tileX * unit;
      float tileWorldY = tileY * unit;
      float tileDepth = tileWorldX + tileWorldY;
      float nextDepth = tileDepth + unit;
      playerCallback(tileDepth, nextDepth);
    }
  }

  // Render remaining players that are beyond all tiles
  float unit = map.getTileWidth() / 2.0f;
  float maxTileDepth = (map.getWidth() + map.getHeight()) * unit;
  playerCallback(maxTileDepth, maxTileDepth + 10000);
}

void TileRenderer::renderTile(const TiledMap& map, int tileX, int tileY,
                              uint32_t gid) {
  // TODO: Implement OpenGL sprite rendering in Phase 3
  // For now, just suppress unused parameter warnings
  (void)map;
  (void)tileX;
  (void)tileY;
  (void)gid;
}

void TileRenderer::gridToWorld(const TiledMap& map, int tileX, int tileY,
                               float& worldX, float& worldY) {
  float unit = map.getTileWidth() / 2.0f;
  worldX = tileX * unit;
  worldY = tileY * unit;
}
