#include "TileRenderer.h"

TileRenderer::TileRenderer(SDL_Renderer* renderer, Camera* camera)
    : renderer(renderer), camera(camera) {}

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
    float rowMinDepth = tileY * unit;
    float rowMaxDepth = (tileY + 1) * unit;
    playerCallback(rowMinDepth, rowMaxDepth);
  }
}

void TileRenderer::renderTile(const TiledMap& map, int tileX, int tileY,
                              uint32_t gid) {
  float worldX, worldY;
  gridToWorld(map, tileX, tileY, worldX, worldY);

  int screenX, screenY;
  camera->worldToScreen(worldX, worldY, screenX, screenY);

  int tileWidth = map.getTileWidth();
  int tileHeight = map.getTileHeight();

  SDL_Rect rect;
  rect.x = screenX - tileWidth / 2;
  rect.y = screenY - tileHeight / 2;
  rect.w = tileWidth;
  rect.h = tileHeight;

  uint8_t colorValue = static_cast<uint8_t>((gid - 1) * 64);
  SDL_SetRenderDrawColor(renderer, colorValue, colorValue, colorValue, 255);
  SDL_RenderFillRect(renderer, &rect);
}

void TileRenderer::gridToWorld(const TiledMap& map, int tileX, int tileY,
                               float& worldX, float& worldY) {
  float unit = map.getTileWidth() / 2.0f;
  worldX = tileX * unit;
  worldY = tileY * unit;
}
