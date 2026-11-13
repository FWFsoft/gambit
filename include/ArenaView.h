#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ArenaClient.h"
#include "Camera.h"
#include "EffectVisuals.h"
#include "PresentationLayer.h"

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

  // The look (docs/design/ART_DIRECTION.md): with a presentation layer the
  // world is drawn into its scene target, actors cast shadows, effects
  // light the ground and bloom, and the HUD is drawn after the resolve.
  void setPresentation(PresentationLayer* layer);

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
  EffectVisuals effectFx;  // how effects look (EffectVisuals.h)
  Camera* camera;
  PresentationLayer* presentation = nullptr;  // not owned; may be null

  // Pools of light on the ground for this frame: players, enemies, effects,
  // telegraphs. Called by the presentation layer before the floor is drawn.
  void collectLights(std::vector<PresentationLayer::LightPool>& out) const;

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

  // Baked, fully animated sheets from tools/sprite_forge. When a character
  // or enemy has one it replaces the placeholder sprite.
  // Where a carried thing sits in one frame: pixels from the feet, its
  // roll, and whether it is in front of the carrier (see sprite_forge).
  struct ActorSocket {
    float x = 0.0f, y = 0.0f, rollDeg = 0.0f;
    bool front = true;
  };
  struct ActorAnim {
    int start = 0, frames = 1, fps = 10;
    bool loop = true;
    std::vector<ActorSocket> sockets;  // direction * frames + frame, or empty
  };
  struct ActorSheet {
    Texture* texture = nullptr;
    int cellW = 0, cellH = 0, pivotX = 0, pivotY = 0, columns = 1;
    float scale = 1.0f;  // drawn this much larger (chunkier art pixels)
    std::unordered_map<std::string, ActorAnim> anims;
    const ActorAnim* find(const std::string& name) const {
      auto it = anims.find(name);
      return it == anims.end() ? nullptr : &it->second;
    }
  };
  std::unordered_map<std::string, ActorSheet> actorSheets;
  const ActorSheet* actorFor(const std::string& name) const;
  // Remembered per entity so a clip plays from its start and stopped
  // characters keep facing where they last looked.
  struct AnimState {
    std::string clip;
    float startSeconds = 0.0f;
    uint32_t castCount = 0;
    int direction = 0;
  };
  std::unordered_map<uint32_t, AnimState> playerAnim;
  std::unordered_map<uint32_t, AnimState> enemyAnim;
  // Where each carrier's hands were last drawn, so a grappled foe can be
  // pinned to them (and drawn in front of or behind the carrier).
  struct CarryPoint {
    float worldX = 0.0f, worldY = 0.0f;
    float screenX = 0.0f, screenY = 0.0f;
    float rollDeg = 0.0f;
    bool front = true, faceLeft = false;
    float seenAt = -1.0f;
  };
  std::unordered_map<uint32_t, CarryPoint> carryPoints;
  const CarryPoint* carrierNear(float x, float y) const;
  static int actorFrameIndex(const ActorAnim& anim, float elapsed, bool hold,
                             int forcedFrame);
  void drawActorFrame(ImDrawList* dl, const ActorSheet& sheet,
                      const ActorAnim& anim, int direction, float elapsed,
                      ImVec2 feet, float scale, unsigned int tint, bool hold = true,
                      int forcedFrame = -1, float rotation = 0.0f,
                      bool castShadow = false);

  bool manifestLoaded = false;
  std::unordered_map<std::string, CharacterSprite> characterSprites;
  std::unordered_map<std::string, EnemySprite> enemySprites;
  std::unordered_map<std::string, FxSprite> fxSprites;
  Texture* shipTexture = nullptr;

  // HUD state
  int selectedGenus = 0;        // infusion picked up in the suit screen
  bool suitScreenOpen = false;  // toggled with B outside intermission
  bool effectLab = false;       // F12: number keys drive effect verbs
  void drawEffectLabPanel();
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
