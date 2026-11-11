#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "ArenaClient.h"
#include "Camera.h"

class ClientPrediction;
class EffectTracker;
class Texture;
struct ImDrawList;
struct ImVec2;

// Draws the match: everything in the world above the tile layer (ground
// markers, characters, enemies, attack effects, floating numbers) and the
// in-game HUD (ability bar, round / zone / ship status, scoreboard, the
// infusion screen).
//
// All drawing goes through Dear ImGui draw lists, so there is no extra GL
// state or shader to maintain and it works unchanged on WebGL. render()
// must be called between ImGui::NewFrame() and ImGui::Render().
class ArenaView {
 public:
  ArenaView(ArenaClient* arena, ClientPrediction* prediction,
            EffectTracker* effects, Camera* camera);

  // World layer + HUD for the Playing state.
  void render();

  // Sprite lookup shared with the character select screen.
  struct CharacterSprite {
    Texture* texture = nullptr;
    int cellW = 0, cellH = 0;
    int directions = 1;  // 8 = real turnaround, 1 = concept-art placeholder
  };
  const CharacterSprite* characterSprite(uint32_t characterId);

 private:
  ArenaClient* arena;
  ClientPrediction* prediction;
  EffectTracker* effects;
  Camera* camera;

  struct EnemySprite {
    Texture* texture = nullptr;
    int cellW = 0, cellH = 0;
    int idleFrames = 1, runFrames = 1, dieFrames = 1;
  };
  struct FxSprite {
    Texture* texture = nullptr;
    int cellW = 0, cellH = 0;
    int frames = 1;
  };

  bool manifestLoaded = false;
  std::unordered_map<std::string, CharacterSprite> characterSprites;
  std::unordered_map<std::string, EnemySprite> enemySprites;
  std::unordered_map<std::string, FxSprite> fxSprites;
  Texture* shipTexture = nullptr;

  // HUD state
  int selectedGenus = 0;        // infusion picked up in the suit screen
  bool suitScreenOpen = false;  // toggled with B outside intermission
  float timeSeconds = 0.0f;

  void loadManifest();
  const EnemySprite* enemySprite(Arena::EnemyKind kind);

  // World
  void renderWorld(ImDrawList* dl);
  void drawGround(ImDrawList* dl);
  void drawEntities(ImDrawList* dl);
  void drawAir(ImDrawList* dl);
  void drawOverhead(ImDrawList* dl);
  void drawPlayer(ImDrawList* dl, const ArenaClient::PlayerView& view,
                  float worldX, float worldY, bool isLocal);
  void drawEnemy(ImDrawList* dl, const ArenaClient::EnemyView& view);
  void drawPickup(ImDrawList* dl, uint32_t itemId, float worldX, float worldY);
  void drawShip(ImDrawList* dl);

  // HUD
  void renderHud();
  void drawAbilityBar();
  void drawStatusBar();
  void drawScoreboard(bool expanded);
  void drawBanners();
  void drawMinimap();
  void drawSuitScreen(bool intermission);
  void drawLobby();
  void drawEndScreen();
  void drawDownedOverlay();
  void drawPrompts();
  void handleHotkeys();

  // Helpers
  // World layers are laid out at 1x and then magnified as a whole
  // (see renderWorld), so toScreen ignores zoom. Overhead text and bars are
  // drawn afterwards at their final position with toZoomedScreen so they
  // stay crisp.
  ImVec2 toScreen(float worldX, float worldY) const;
  ImVec2 toZoomedScreen(float worldX, float worldY) const;
  bool localPosition(float& x, float& y) const;
  uint32_t localId() const;
};
