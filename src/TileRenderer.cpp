#include "TileRenderer.h"

TileRenderer::TileRenderer(Camera* camera, SpriteRenderer* spriteRenderer,
                           Texture* whitePixel)
    : camera(camera), spriteRenderer(spriteRenderer), whitePixel(whitePixel) {}

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
  float worldX, worldY;
  gridToWorld(map, tileX, tileY, worldX, worldY);

  int screenX, screenY;
  camera->worldToScreen(worldX, worldY, screenX, screenY);

  int tileWidth = map.getTileWidth();
  int tileHeight = map.getTileHeight();

  // Calculate color based on gid (grayscale for now)
  float colorValue = static_cast<float>((gid - 1) % 4) / 4.0f;

  // Draw tile as colored rectangle using white pixel texture
  if (whitePixel) {
    spriteRenderer->draw(*whitePixel, screenX - tileWidth / 2.0f,
                         screenY - tileHeight / 2.0f, tileWidth, tileHeight,
                         colorValue, colorValue, colorValue, 1.0f);
  }
}

void TileRenderer::gridToWorld(const TiledMap& map, int tileX, int tileY,
                               float& worldX, float& worldY) {
  float unit = map.getTileWidth() / 2.0f;
  worldX = tileX * unit;
  worldY = tileY * unit;
}
