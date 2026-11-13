// World rendering for the arena: ground markers, characters, enemies, attack
// effects and floating text. The HUD lives in ArenaHud.cpp.

#include "ArenaView.h"

#include <imgui.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ArenaDraw.h"
#include "CharacterRegistry.h"
#include "ClientPrediction.h"
#include "Effect.h"
#include "EffectTracker.h"
#include "Logger.h"
#include "PresentationLayer.h"
#include "Texture.h"
#include "TextureManager.h"
#include "config/LookConfig.h"
#include "config/ScreenConfig.h"

using namespace ArenaDraw;

namespace {

constexpr float PROJECTILE_HEIGHT = 24.0f;  // drawn this far above the ground
constexpr float SHIP_DRAW_SIZE = 192.0f;

ImTextureID textureId(const Texture* texture) {
  return (ImTextureID)(intptr_t)texture->getID();
}

// --- The look ---------------------------------------------------------------
// While renderWorld draws into the presentation layer's scene target, the
// scene's alpha channel is an emissive mask: what is drawn "glow" (ImGui's
// normal blend) blooms, what is drawn "matte" (sprites, shadows) does not
// and hides the glow behind it. See PresentationLayer.h.
struct LookState {
  PresentationLayer* layer = nullptr;  // null: the look is off this frame
  bool shadowChannel = false;  // entity pass: shadows have a channel of their own
};
LookState look;

void glowCallback(const ImDrawList*, const ImDrawCmd*) {
  PresentationLayer::blendGlow();
}
void matteCallback(const ImDrawList*, const ImDrawCmd*) {
  PresentationLayer::blendMatte();
}
void resolveCallback(const ImDrawList*, const ImDrawCmd* cmd) {
  static_cast<PresentationLayer*>(cmd->UserCallbackData)->resolve();
}
void beginMatte(ImDrawList* dl) {
  if (look.layer) dl->AddCallback(matteCallback, nullptr);
}
void endMatte(ImDrawList* dl) {
  if (look.layer) dl->AddCallback(glowCallback, nullptr);
}

// Shadows are matte. In the entity pass they go to draw channel 0, which is
// emitted before every entity, so no shadow is ever painted over a body.
struct ShadowScope {
  ImDrawList* dl;
  explicit ShadowScope(ImDrawList* list) : dl(list) {
    if (look.shadowChannel) {
      dl->ChannelsSetCurrent(0);
    } else {
      beginMatte(dl);
    }
  }
  ~ShadowScope() {
    if (look.shadowChannel) {
      dl->ChannelsSetCurrent(1);
    } else {
      endMatte(dl);
    }
  }
  ShadowScope(const ShadowScope&) = delete;
  ShadowScope& operator=(const ShadowScope&) = delete;
};

// Draw a sprite cell standing on `feet`. The quad is scaled and rotated
// around the feet so characters can lean, squash and fall over.
void drawSprite(ImDrawList* dl, const Texture* texture, int srcX, int srcY,
                int srcW, int srcH, ImVec2 feet, float scaleX, float scaleY,
                float rotation, bool flipX, ImU32 color) {
  float w = srcW * scaleX;
  float h = srcH * scaleY;
  float cosR = std::cos(rotation);
  float sinR = std::sin(rotation);
  auto corner = [&](float lx, float ly) {
    return ImVec2(feet.x + lx * cosR - ly * sinR,
                  feet.y + lx * sinR + ly * cosR);
  };
  ImVec2 topLeft = corner(-w * 0.5f, -h);
  ImVec2 topRight = corner(w * 0.5f, -h);
  ImVec2 bottomRight = corner(w * 0.5f, 0.0f);
  ImVec2 bottomLeft = corner(-w * 0.5f, 0.0f);

  float texW = static_cast<float>(texture->getWidth());
  float texH = static_cast<float>(texture->getHeight());
  float u0 = srcX / texW;
  float u1 = (srcX + srcW) / texW;
  float v0 = srcY / texH;
  float v1 = (srcY + srcH) / texH;
  if (flipX) std::swap(u0, u1);

  beginMatte(dl);
  dl->AddImageQuad(textureId(texture), topLeft, topRight, bottomRight,
                   bottomLeft, ImVec2(u0, v0), ImVec2(u1, v0), ImVec2(u1, v1),
                   ImVec2(u0, v1), color);
  endMatte(dl);
}

// The sprite's own silhouette, laid on the ground away from the key light
// (the same light the forge bakes into the sprites). `base` is the bottom
// centre of the cell and `feetLift` how far above it the feet line is.
void drawCastShadow(ImDrawList* dl, const Texture* texture, int srcX, int srcY,
                    int srcW, int srcH, ImVec2 base, float feetLift,
                    float scaleX, float scaleY, bool flipX) {
  if (!look.layer) return;
  ShadowScope scope(dl);
  float w = srcW * scaleX;
  float h = srcH * scaleY;
  float groundY = base.y - feetLift;
  // `up` is measured from the cell's bottom edge; the shadow of a point is
  // sheared by its height above the ground.
  auto corner = [&](float lx, float up) {
    float height = up - feetLift;
    return ImVec2(base.x + lx + height * Config::Look::SHADOW_SHEAR_X,
                  groundY + height * Config::Look::SHADOW_SHEAR_Y);
  };
  float texW = static_cast<float>(texture->getWidth());
  float texH = static_cast<float>(texture->getHeight());
  float u0 = srcX / texW;
  float u1 = (srcX + srcW) / texW;
  float v0 = srcY / texH;
  float v1 = (srcY + srcH) / texH;
  if (flipX) std::swap(u0, u1);
  dl->AddImageQuad(
      textureId(texture), corner(-w * 0.5f, h), corner(w * 0.5f, h),
      corner(w * 0.5f, 0.0f), corner(-w * 0.5f, 0.0f), ImVec2(u0, v0),
      ImVec2(u1, v0), ImVec2(u1, v1), ImVec2(u0, v1),
      IM_COL32(0, 0, 0,
               static_cast<int>(255 * Config::Look::CAST_SHADOW_ALPHA)));
}

ImU32 multiplyColor(ImU32 a, ImU32 b) {
  auto ch = [](ImU32 c, int shift) { return (c >> shift) & 0xFF; };
  return IM_COL32(ch(a, IM_COL32_R_SHIFT) * ch(b, IM_COL32_R_SHIFT) / 255,
                  ch(a, IM_COL32_G_SHIFT) * ch(b, IM_COL32_G_SHIFT) / 255,
                  ch(a, IM_COL32_B_SHIFT) * ch(b, IM_COL32_B_SHIFT) / 255,
                  ch(a, IM_COL32_A_SHIFT) * ch(b, IM_COL32_A_SHIFT) / 255);
}

// Contact shadow. `alpha` is relative: 0.35 is a body standing on the ground.
void drawShadow(ImDrawList* dl, ImVec2 feet, float radius, float alpha = 0.35f) {
  if (!look.layer) {
    dl->AddEllipseFilled(feet, ImVec2(radius, radius * 0.38f),
                         IM_COL32(0, 0, 0, static_cast<int>(255 * alpha)), 0.0f,
                         20);
    return;
  }
  // The look: a soft blob that is darkest right under the feet.
  ShadowScope scope(dl);
  float w = radius * Config::Look::CONTACT_SHADOW_WIDTH;
  float h = w * Config::Look::CONTACT_SHADOW_SQUASH;
  float a = std::min(1.0f, alpha / 0.35f * Config::Look::CONTACT_SHADOW_ALPHA);
  dl->AddImage((ImTextureID)(intptr_t)look.layer->softBlobTexture(),
               ImVec2(feet.x - w, feet.y - h), ImVec2(feet.x + w, feet.y + h),
               ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
               IM_COL32(0, 0, 0, static_cast<int>(255 * a)));
}

// Short tag for an effect, used above enemies where space is tight.
const char* effectTag(EffectType type) {
  switch (type) {
    case EffectType::Slow:
      return "SLOW";
    case EffectType::Haste:
      return "HASTE";
    case EffectType::Weakened:
      return "WEAK";
    case EffectType::Empowered:
      return "EMP";
    case EffectType::Vulnerable:
      return "VULN";
    case EffectType::Fortified:
      return "FORT";
    case EffectType::Wound:
      return "WND";
    case EffectType::Mend:
      return "MEND";
    case EffectType::Expose:
      return "EXPO";
    case EffectType::Guard:
      return "GRD";
    case EffectType::Stunned:
      return "STUN";
    case EffectType::Berserk:
      return "BSRK";
    case EffectType::Snared:
      return "SNARE";
    case EffectType::Confused:
      return "CONF";
    case EffectType::Doomed:
      return "DOOM";
    case EffectType::Silenced:
      return "SIL";
    case EffectType::Marked:
      return "MARK";
    default:
      return EffectRegistry::getName(type);
  }
}

// Frame index for an 8-direction strip laid out S, SE, E, NE, N, NW, W, SW.
int directionFrame(float dirX, float dirY) {
  float degrees = std::atan2(dirY, dirX) * 180.0f / PI;  // E = 0, S = 90
  int index = static_cast<int>(std::lround((90.0f - degrees) / 45.0f));
  return ((index % 8) + 8) % 8;
}

}  // namespace

ArenaView::ArenaView(ArenaClient* arena, ClientPrediction* prediction,
                     EffectTracker* effects, Camera* camera)
    : arena(arena), prediction(prediction), effects(effects), camera(camera) {}

// ---------------------------------------------------------------------------
// Sprites
// ---------------------------------------------------------------------------

void ArenaView::loadManifest() {
  if (manifestLoaded) return;
  manifestLoaded = true;

  std::ifstream file("assets/sprites/manifest.txt");
  if (!file.is_open()) {
    Logger::error("ArenaView: assets/sprites/manifest.txt not found. Run "
                  "tools/sprite_pipeline/build_sprites.py");
    return;
  }
  std::string line;
  while (std::getline(file, line)) {
    std::istringstream in(line);
    std::string kind, name, path;
    in >> kind;
    if (kind == "character") {
      CharacterSprite sprite;
      in >> name >> path >> sprite.cellW >> sprite.cellH >> sprite.directions;
      sprite.texture = TextureManager::instance().get(path);
      if (sprite.texture) characterSprites[name] = sprite;
    } else if (kind == "enemy") {
      EnemySprite sprite;
      std::string genus;
      int tier;
      in >> name >> genus >> tier >> path >> sprite.cellW >> sprite.cellH >>
          sprite.idleFrames >> sprite.runFrames >> sprite.dieFrames;
      sprite.texture = TextureManager::instance().get(path);
      if (sprite.texture) enemySprites[name] = sprite;
    } else if (kind == "fx") {
      FxSprite sprite;
      in >> name >> path >> sprite.cellW >> sprite.cellH >> sprite.frames;
      sprite.texture = TextureManager::instance().get(path);
      if (sprite.texture) fxSprites[name] = sprite;
    }
  }
  // Fully animated actors baked by tools/sprite_forge.
  std::ifstream actors("assets/sprites/actors/manifest.txt");
  while (actors.is_open() && std::getline(actors, line)) {
    std::istringstream in(line);
    std::string kind, name;
    in >> kind >> name;
    if (kind == "actor") {
      ActorSheet sheet;
      std::string path;
      in >> path >> sheet.cellW >> sheet.cellH >> sheet.pivotX >> sheet.pivotY >>
          sheet.columns;
      if (!(in >> sheet.scale)) sheet.scale = 1.0f;
      std::string filter;
      bool smooth = (in >> filter) && filter == "smooth";
      sheet.texture = TextureManager::instance().get(path);
      // Toon-style sheets are smooth art: filter them linearly.
      if (sheet.texture && smooth) sheet.texture->setSmooth(true);
      if (sheet.texture) {
        auto anims = std::move(actorSheets[name].anims);
        actorSheets[name] = sheet;
        actorSheets[name].anims = std::move(anims);
      }
    } else if (kind == "anim") {
      std::string clip;
      ActorAnim anim;
      int loop = 1;
      in >> clip >> anim.start >> anim.frames >> anim.fps >> loop;
      anim.loop = loop != 0;
      auto old = actorSheets[name].anims.find(clip);
      if (old != actorSheets[name].anims.end()) {
        anim.sockets = std::move(old->second.sockets);
      }
      actorSheets[name].anims[clip] = anim;
    } else if (kind == "socket") {
      // socket <actor> <clip> x,y,roll,front ... (one per direction x frame)
      std::string clip, entry;
      in >> clip;
      std::vector<ActorSocket>& sockets = actorSheets[name].anims[clip].sockets;
      sockets.clear();
      while (in >> entry) {
        ActorSocket socket;
        int front = 1;
        if (std::sscanf(entry.c_str(), "%f,%f,%f,%d", &socket.x, &socket.y,
                        &socket.rollDeg, &front) == 4) {
          socket.front = front != 0;
          sockets.push_back(socket);
        }
      }
    }
  }
  for (auto it = actorSheets.begin(); it != actorSheets.end();) {
    if (!it->second.texture) {
      it = actorSheets.erase(it);
    } else {
      ++it;
    }
  }

  shipTexture = TextureManager::instance().get("assets/ship.png");
  Logger::info("ArenaView: loaded " + std::to_string(characterSprites.size()) +
               " character, " + std::to_string(enemySprites.size()) +
               " enemy and " + std::to_string(fxSprites.size()) +
               " fx sprites, " + std::to_string(actorSheets.size()) +
               " animated actors");
}

const ArenaView::CharacterSprite* ArenaView::characterSprite(
    uint32_t characterId) {
  loadManifest();
  const CharacterDefinition* definition =
      CharacterRegistry::instance().getCharacter(characterId);
  if (!definition) return nullptr;
  auto it = characterSprites.find(definition->name);
  return it == characterSprites.end() ? nullptr : &it->second;
}

const ArenaView::ActorSheet* ArenaView::actorFor(const std::string& name) const {
  auto it = actorSheets.find(name);
  return it == actorSheets.end() ? nullptr : &it->second;
}

int ArenaView::actorFrameIndex(const ActorAnim& anim, float elapsed, bool hold,
                               int forcedFrame) {
  int frame = forcedFrame >= 0 ? forcedFrame
                               : static_cast<int>(elapsed * anim.fps);
  if (anim.loop && !hold) {
    frame %= anim.frames;
  } else if (anim.loop) {
    frame = ((frame % anim.frames) + anim.frames) % anim.frames;
  } else {
    frame = std::max(0, std::min(anim.frames - 1, frame));
  }
  return frame;
}

const ArenaView::CarryPoint* ArenaView::carrierNear(float x, float y) const {
  const CarryPoint* best = nullptr;
  float bestDist = 70.0f * 70.0f;
  for (const auto& [id, point] : carryPoints) {
    if (timeSeconds - point.seenAt > 0.25f) continue;
    float dx = point.worldX - x, dy = point.worldY - y;
    float d = dx * dx + dy * dy;
    if (d < bestDist) {
      bestDist = d;
      best = &point;
    }
  }
  return best;
}

void ArenaView::drawActorFrame(ImDrawList* dl, const ActorSheet& sheet,
                               const ActorAnim& anim, int direction,
                               float elapsed, ImVec2 feet, float scale,
                               ImU32 tint, bool hold, int forcedFrame,
                               float rotation, bool castShadow) {
  int frame = actorFrameIndex(anim, elapsed, hold, forcedFrame);
  scale *= sheet.scale;
  int cell = anim.start + direction * anim.frames + frame;
  int col = cell % sheet.columns;
  int row = cell / sheet.columns;
  // drawSprite stands the quad on `feet`; the cell's feet line is
  // (cellH - pivotY) pixels above its bottom edge.
  ImVec2 base(feet.x, feet.y + (sheet.cellH - sheet.pivotY) * scale);
  if (castShadow) {
    drawCastShadow(dl, sheet.texture, col * sheet.cellW, row * sheet.cellH,
                   sheet.cellW, sheet.cellH, base,
                   (sheet.cellH - sheet.pivotY) * scale, scale, scale, false);
  }
  drawSprite(dl, sheet.texture, col * sheet.cellW, row * sheet.cellH,
             sheet.cellW, sheet.cellH, base, scale, scale, rotation, false,
             tint);
}

namespace {

// Which clip an ability plays. Characters alternate hands on repeat casts.
std::string actionClip(const std::string& character, int slot,
                       uint32_t castCount, bool alt) {
  bool odd = (castCount % 2) == 1;
  if (character == "Presidente") {
    // alt = cast while holding a foe: the Resonance forms.
    switch (slot) {
      case 0:
        return alt ? "latigo" : "grab";
      case 1:
        return alt ? "bomba" : "pesado";
      case 2:
        return alt ? "abrazo" : "cajones";
      case 3:
        return alt ? "rana_bomba" : "splash";
      default:
        return alt ? "latigo" : "cajones";
    }
  }
  if (character == "Wade") {
    switch (slot) {
      case 0:
        return odd ? "jab" : "cross";
      case 1:
        return odd ? "uppercut_R" : "uppercut_L";
      case 2:
        return "slip";
      case 3:
        return "roll";
      default:
        return "cross";
    }
  }
  if (character == "Suds") {
    switch (slot) {
      case 0:
        return "toss";     // Brew Toss
      case 1:
        return "belch";    // Frothy Cleanse
      case 2:
        return "rounds";   // Round's On Me!
      case 3:
        return "chug";     // What's In This Stuff?
      default:
        return "toss";
    }
  }
  if (character == "Gravon") {
    switch (slot) {
      case 0:
        return odd ? "slash_a" : "slash_b";  // Titan's Slash, back and forth
      case 1:
        return "ravager";  // Ravager's Edge
      case 2:
        return "fury";     // Fury of the Blade
      case 3:
        return "rush";     // Blade Rush
      default:
        return "slash_a";
    }
  }
  static const char* GENERIC[5] = {"attack", "secondary", "special",
                                   "mobility", "special"};
  return GENERIC[std::max(0, std::min(4, slot))];
}

}  // namespace

const ArenaView::EnemySprite* ArenaView::enemySprite(Arena::EnemyKind kind) {
  loadManifest();
  // The boss borrows the largest Cerebrian body until it has its own art.
  const char* name = kind == Arena::EnemyKind::Boss
                         ? "Synaptogon"
                         : Arena::enemyDef(kind).name;
  auto it = enemySprites.find(name);
  return it == enemySprites.end() ? nullptr : &it->second;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

ImVec2 ArenaView::toScreen(float worldX, float worldY) const {
  return ImVec2(worldX - camera->x + Config::Screen::WIDTH / 2.0f,
                worldY - camera->y + Config::Screen::HEIGHT / 2.0f);
}

ImVec2 ArenaView::toZoomedScreen(float worldX, float worldY) const {
  return ImVec2(
      (worldX - camera->x) * camera->zoom + Config::Screen::WIDTH / 2.0f,
      (worldY - camera->y) * camera->zoom + Config::Screen::HEIGHT / 2.0f);
}

uint32_t ArenaView::localId() const { return prediction->getLocalPlayerId(); }

bool ArenaView::localPosition(float& x, float& y) const {
  const Player& local = prediction->getLocalPlayer();
  x = local.x;
  y = local.y;
  return true;
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

void ArenaView::render() {
  loadManifest();
  timeSeconds = arena->nowMs() / 1000.0f;

  // The background draw list sits under every ImGui window: the world.
  renderWorld(ImGui::GetBackgroundDrawList());
  renderHud();
}

void ArenaView::renderWorld(ImDrawList* dl) {
  // The look is on when RenderSystem opened a scene for this frame.
  look.layer =
      presentation && presentation->sceneOpen() ? presentation : nullptr;
  look.shadowChannel = false;

  // Lay the world out at 1x, then magnify every vertex around the screen
  // centre. One place applies the zoom, and no drawing code has to care.
  int firstVertex = dl->VtxBuffer.Size;
  effectFx.beginFrame(timeSeconds);
  drawGround(dl);
  drawEntities(dl);
  drawAir(dl);
  const float zoom = camera->zoom;
  if (zoom != 1.0f) {
    const float cx = Config::Screen::WIDTH / 2.0f;
    const float cy = Config::Screen::HEIGHT / 2.0f;
    for (int i = firstVertex; i < dl->VtxBuffer.Size; i++) {
      ImDrawVert& v = dl->VtxBuffer[i];
      v.pos.x = cx + (v.pos.x - cx) * zoom;
      v.pos.y = cy + (v.pos.y - cy) * zoom;
    }
  }
  // Big effect moments (detonations, corruptions) shake the world a little.
  ImVec2 shake = effectFx.shakeOffset();
  if (shake.x != 0.0f || shake.y != 0.0f) {
    for (int i = firstVertex; i < dl->VtxBuffer.Size; i++) {
      dl->VtxBuffer[i].pos.x += shake.x;
      dl->VtxBuffer[i].pos.y += shake.y;
    }
  }
  if (look.layer) {
    // The world is complete: bloom and grade it onto the screen. Everything
    // drawn after this (health bars, names, numbers, the HUD) lands on the
    // screen itself, crisp and ungraded.
    assert(!look.layer->resolveQueued());
    dl->AddCallback(resolveCallback, look.layer);
    dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    look.layer->markResolveQueued();
    look.layer = nullptr;
  }
  drawOverhead(dl);
}

void ArenaView::setPresentation(PresentationLayer* layer) {
  presentation = layer;
  if (presentation) {
    presentation->setLightProvider(
        [this](std::vector<PresentationLayer::LightPool>& out) {
          collectLights(out);
        });
  }
}

// ---------------------------------------------------------------------------
// Light pools: what lights the ground this frame
// ---------------------------------------------------------------------------

void ArenaView::collectLights(
    std::vector<PresentationLayer::LightPool>& out) const {
  namespace L = Config::Look;
  const float halfW = Config::Screen::WIDTH * 0.5f / camera->zoom;
  const float halfH = Config::Screen::HEIGHT * 0.5f / camera->zoom;
  // Most important first: once the list is full the rest is dropped.
  auto add = [&](float x, float y, float radius, Rgb c, float strength) {
    if (strength <= 0.02f || radius < 1.0f) return;
    if (std::abs(x - camera->x) > halfW + radius ||
        std::abs(y - camera->y) > halfH + radius) {
      return;
    }
    if (out.size() >= static_cast<size_t>(L::MAX_LIGHT_POOLS)) return;
    out.push_back({x, y, radius, c.r, c.g, c.b, strength});
  };

  // A warm pool around every player: where the heroes are is where the
  // light is.
  const Rgb warm{L::PLAYER_POOL_R, L::PLAYER_POOL_G, L::PLAYER_POOL_B};
  uint32_t me = localId();
  const Player& local = prediction->getLocalPlayer();
  for (const auto& [id, view] : arena->players()) {
    const PlayerState& state = view.state;
    if (state.characterId == 0) continue;
    if ((state.flags & PlayerFlags::Dead) != 0 || state.health <= 0.0f) continue;
    bool hidden = (state.flags & PlayerFlags::Hidden) != 0;
    add(id == me ? local.x : view.x, id == me ? local.y : view.y,
        L::PLAYER_POOL_RADIUS, warm,
        L::PLAYER_POOL_STRENGTH * (hidden ? L::HIDDEN_POOL_SCALE : 1.0f));
  }

  // Telegraphs and one-shot effects.
  for (const ArenaClient::Fx& f : arena->effects()) {
    float t = std::max(0.0f, std::min(1.0f, f.ageMs / f.lifeMs));
    float fade = 1.0f - t;
    Rgb c = themeRgb(f.theme);
    float dirLen = std::sqrt(f.x2 * f.x2 + f.y2 * f.y2);
    float dirX = dirLen > 0.001f ? f.x2 / dirLen : 0.0f;
    float dirY = dirLen > 0.001f ? f.y2 / dirLen : 0.0f;
    switch (f.kind) {
      case GameEventKind::TelegraphCircle:
        // Brightens as the attack is about to land.
        add(f.x, f.y, f.value * L::TELEGRAPH_POOL_RADIUS_SCALE, c,
            L::TELEGRAPH_POOL_STRENGTH_START +
                (L::TELEGRAPH_POOL_STRENGTH_END -
                 L::TELEGRAPH_POOL_STRENGTH_START) * t);
        break;
      case GameEventKind::TelegraphCone:
        add(f.x + dirX * f.value * 0.55f, f.y + dirY * f.value * 0.55f,
            f.value * 0.8f, c,
            L::TELEGRAPH_POOL_STRENGTH_START +
                (L::TELEGRAPH_POOL_STRENGTH_END -
                 L::TELEGRAPH_POOL_STRENGTH_START) * t);
        break;
      case GameEventKind::Slash:
      case GameEventKind::Cone:
        add(f.x + dirX * f.value * 0.6f, f.y + dirY * f.value * 0.6f,
            f.value * 1.3f, c, L::FX_SWING_STRENGTH * fade);
        break;
      case GameEventKind::Burst:
        add(f.x, f.y, f.value * L::FX_BURST_RADIUS_SCALE, c,
            L::FX_BURST_STRENGTH * fade);
        break;
      case GameEventKind::Ring:
        add(f.x, f.y, f.value * (0.25f + 0.75f * ease(t)) * 1.3f, c,
            L::FX_RING_STRENGTH * fade);
        break;
      case GameEventKind::Beam: {
        float radius = std::max(L::FX_TRAIL_RADIUS, f.value * 6.0f);
        add(f.x, f.y, radius, c, L::FX_BEAM_STRENGTH * fade);
        add((f.x + f.x2) * 0.5f, (f.y + f.y2) * 0.5f, radius, c,
            L::FX_BEAM_STRENGTH * fade);
        add(f.x2, f.y2, radius, c, L::FX_BEAM_STRENGTH * fade);
        break;
      }
      case GameEventKind::Lob:
        add(f.x + (f.x2 - f.x) * t, f.y + (f.y2 - f.y) * t,
            L::FX_TRAIL_RADIUS, c, L::FX_TRAIL_STRENGTH);
        break;
      case GameEventKind::DashTrail:
        add(f.x, f.y, L::FX_TRAIL_RADIUS, c, L::FX_TRAIL_STRENGTH * fade);
        add(f.x2, f.y2, L::FX_TRAIL_RADIUS, c, L::FX_TRAIL_STRENGTH * fade);
        break;
      case GameEventKind::Tether:
        add(f.x2, f.y2, L::FX_TRAIL_RADIUS, c, L::FX_TRAIL_STRENGTH * fade);
        break;
      case GameEventKind::HitSpark:
        add(f.x, f.y, std::max(L::FX_HIT_RADIUS_MIN, f.value * 4.0f), c,
            L::FX_HIT_STRENGTH * fade);
        break;
      case GameEventKind::SpawnPuff:
        add(f.x, f.y, L::FX_TRAIL_RADIUS, c, L::FX_TRAIL_STRENGTH * fade);
        break;
      default:
        break;
    }
  }

  // Ground zones: genus colour arrives as light, not as bright tiles.
  for (const ArenaClient::Zone& zone : arena->zones()) {
    float fadeIn = std::min(1.0f, zone.ageMs / 200.0f);
    float fadeOut = std::min(1.0f, (zone.lifeMs - zone.ageMs) / 400.0f);
    float alpha = std::max(0.0f, std::min(fadeIn, fadeOut));
    add(zone.x, zone.y, zone.radius * L::ZONE_POOL_RADIUS_SCALE,
        themeRgb(zone.fromEnemy ? VfxTheme::Danger : zone.theme),
        L::ZONE_POOL_STRENGTH * alpha);
  }

  for (const ArenaClient::Projectile& shot : arena->projectiles()) {
    if (shot.ageMs > shot.lifeMs) continue;
    add(shot.x, shot.y, shot.radius * L::PROJECTILE_POOL_RADIUS_SCALE,
        themeRgb(shot.fromEnemy ? VfxTheme::Danger : shot.theme),
        L::PROJECTILE_POOL_STRENGTH);
  }

  for (const auto& [id, item] : prediction->getWorldItems()) {
    if (!Arena::PickupId::isInfusion(item.itemId)) continue;
    add(item.x, item.y, L::PICKUP_POOL_RADIUS,
        themeRgb(genusTheme(Arena::PickupId::infusionGenus(item.itemId))),
        L::PICKUP_POOL_STRENGTH);
  }

  // Enemies stand in a faint pool of their genus colour, and it turns to
  // danger red as they wind up a hit.
  for (const auto& [id, view] : arena->enemies()) {
    if (view.dying) continue;
    Arena::EnemyKind kind = static_cast<Arena::EnemyKind>(view.state.type);
    if (static_cast<int>(kind) >= static_cast<int>(Arena::EnemyKind::Count)) {
      continue;
    }
    const Arena::EnemyKindDef& def = Arena::enemyDef(kind);
    bool elite = (view.state.flags & EnemyFlags::Elite) != 0;
    bool boss = kind == Arena::EnemyKind::Boss;
    float radius = std::max(L::ENEMY_POOL_RADIUS_MIN,
                            def.radius * L::ENEMY_POOL_RADIUS_SCALE);
    if (boss) radius *= L::BOSS_POOL_RADIUS_SCALE;
    add(view.x, view.y, radius,
        themeRgb(boss ? VfxTheme::Shadow : genusTheme(def.genus)),
        boss ? L::BOSS_POOL_STRENGTH
             : elite ? L::ELITE_POOL_STRENGTH : L::ENEMY_POOL_STRENGTH);
    if (view.attackMs > 0.0f && view.attackTotalMs > 0.0f) {
      float windup = 1.0f - view.attackMs / view.attackTotalMs;
      add(view.x, view.y, radius, themeRgb(VfxTheme::Danger),
          L::ENEMY_WINDUP_POOL_STRENGTH * windup);
    }
  }
}

// ---------------------------------------------------------------------------
// Ground layer: zone, hazards, telegraphs, objectives
// ---------------------------------------------------------------------------

void ArenaView::drawGround(ImDrawList* dl) {
  const MatchStatePacket& match = arena->match();
  Arena::MatchPhase phase = arena->phase();
  bool fighting = phase == Arena::MatchPhase::Round ||
                  phase == Arena::MatchPhase::BossFight;

  if (arena->hasMatch()) {
    ImVec2 ship = toScreen(match.shipX, match.shipY);

    // Around the ship: where you need to be when the round ends.
    if (phase == Arena::MatchPhase::Round) {
      float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 2.0f);
      dl->AddCircle(ship, Arena::Tuning::SHIP_SAFE_RADIUS,
                    IM_COL32(120, 255, 170, static_cast<int>(40 + 30 * pulse)),
                    72, 2.0f);
    }

    // The Blue Zone: everything outside is tinted and deadly.
    if (fighting) {
      float radius = match.zoneRadius;
      const float band = 2400.0f;
      dl->AddCircle(ship, radius + band * 0.5f, IM_COL32(20, 40, 120, 95), 96,
                    band);
      float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 3.0f);
      dl->AddCircle(ship, radius, IM_COL32(90, 170, 255, 200), 128, 5.0f);
      dl->AddCircle(ship, radius - 7.0f,
                    IM_COL32(160, 220, 255, static_cast<int>(60 + 60 * pulse)),
                    128, 2.0f);
    }
  }

  // Objectives
  for (const auto& [id, objective] : prediction->getObjectives()) {
    ImVec2 centre = toScreen(objective.x, objective.y);
    if (centre.x < -200 || centre.x > Config::Screen::WIDTH + 200 ||
        centre.y < -200 || centre.y > Config::Screen::HEIGHT + 200) {
      continue;
    }
    ImU32 colour = IM_COL32(255, 220, 90, 150);
    const char* label = "Objective";
    if (objective.state == ObjectiveState::InProgress) {
      colour = IM_COL32(90, 190, 255, 190);
    } else if (objective.state == ObjectiveState::ReadyToDeposit) {
      colour = IM_COL32(255, 160, 60, 190);
    } else if (objective.state == ObjectiveState::Completed) {
      colour = IM_COL32(110, 230, 130, 90);
    }
    switch (objective.type) {
      case ObjectiveType::AlienScrapyard:
        label = "Scrapyard";
        break;
      case ObjectiveType::DerelictTurret:
        label = "Derelict Turret";
        break;
      case ObjectiveType::CaptureOutpost:
        label = "Outpost";
        break;
      case ObjectiveType::SalvageMedpacks:
        label = "Medpacks";
        break;
      default:
        break;
    }
    float spin = timeSeconds * 0.6f;
    dl->AddCircle(centre, objective.radius, colour, 48, 2.0f);
    // Four rotating ticks make it read as "interactive" rather than a hazard.
    for (int i = 0; i < 4; i++) {
      float a = spin + i * PI * 0.5f;
      dl->AddLine(ImVec2(centre.x + std::cos(a) * (objective.radius - 8),
                         centre.y + std::sin(a) * (objective.radius - 8)),
                  ImVec2(centre.x + std::cos(a) * (objective.radius + 8),
                         centre.y + std::sin(a) * (objective.radius + 8)),
                  colour, 3.0f);
    }
    dl->AddRectFilled(ImVec2(centre.x - 9, centre.y - 9),
                      ImVec2(centre.x + 9, centre.y + 9), colour, 2.0f);
    (void)label;  // names and progress are drawn crisp in drawOverhead
  }

  // Ground zones: clouds, pools, vortex
  for (const ArenaClient::Zone& zone : arena->zones()) {
    ImVec2 centre = toScreen(zone.x, zone.y);
    float fadeIn = std::min(1.0f, zone.ageMs / 200.0f);
    float fadeOut = std::min(1.0f, (zone.lifeMs - zone.ageMs) / 400.0f);
    float alpha = std::max(0.0f, std::min(fadeIn, fadeOut));
    // Enemy hazards are outlined in red so "don't stand here" is unmistakable.
    ImU32 edge = zone.fromEnemy ? themeColor(VfxTheme::Danger, 0.8f * alpha)
                                : themeColor(zone.theme, 0.7f * alpha);

    if (zone.variant == 0) {
      // Cloud: drifting blobs
      for (int i = 0; i < 7; i++) {
        float a = i * 0.9f + timeSeconds * (0.3f + 0.05f * i);
        float r = zone.radius * (0.25f + 0.1f * (i % 3));
        float orbit = zone.radius * (i == 0 ? 0.0f : 0.5f);
        ImVec2 p(centre.x + std::cos(a) * orbit, centre.y + std::sin(a) * orbit);
        dl->AddCircleFilled(p, r + zone.radius * 0.25f,
                            themeColor(zone.theme, 0.10f * alpha), 24);
      }
      dl->AddCircle(centre, zone.radius, edge, 48, 2.0f);
    } else if (zone.variant == 1) {
      // Pool
      dl->AddCircleFilled(centre, zone.radius,
                          themeColor(zone.theme, 0.22f * alpha), 32);
      float ripple = std::fmod(timeSeconds * 0.8f + zone.id * 0.37f, 1.0f);
      dl->AddCircle(centre, zone.radius * ripple,
                    themeHot(zone.theme, 0.35f * alpha * (1.0f - ripple)), 32,
                    2.0f);
      dl->AddCircle(centre, zone.radius, edge, 32, 2.0f);
    } else {
      // Vortex: rotating arcs pulling inward
      float shrink = 1.0f - 0.5f * ease(zone.ageMs / zone.lifeMs);
      for (int i = 0; i < 3; i++) {
        float a = timeSeconds * 9.0f + i * (2.0f * PI / 3.0f);
        float r = zone.radius * shrink * (0.45f + 0.27f * i);
        dl->PathArcTo(centre, r, a, a + 1.9f, 20);
        dl->PathStroke(themeColor(zone.theme, 0.75f * alpha), 0, 4.0f);
      }
      dl->AddCircleFilled(centre, zone.radius * shrink,
                          themeColor(zone.theme, 0.10f * alpha), 40);
    }
  }

  // Telegraphs: where an attack is about to land
  for (const ArenaClient::Fx& f : arena->effects()) {
    float t = f.ageMs / f.lifeMs;
    if (f.kind == GameEventKind::TelegraphCircle) {
      ImVec2 centre = toScreen(f.x, f.y);
      dl->AddCircleFilled(centre, f.value, themeColor(f.theme, 0.10f), 40);
      dl->AddCircleFilled(centre, f.value * t, themeColor(f.theme, 0.26f), 40);
      dl->AddCircle(centre, f.value, themeColor(f.theme, 0.95f), 40, 2.5f);
    } else if (f.kind == GameEventKind::TelegraphCone) {
      ImVec2 origin = toScreen(f.x, f.y);
      float angle = std::atan2(f.y2, f.x2);
      float half = f.param * PI / 180.0f;
      sector(dl, origin, f.value, angle, half, themeColor(f.theme, 0.10f),
             themeColor(f.theme, 0.9f), 2.0f);
      sector(dl, origin, f.value * t, angle, half, themeColor(f.theme, 0.28f),
             0);
    }
  }

  // Scorch marks left by detonations.
  effectFx.drawGround(dl, [this](float x, float y) { return toScreen(x, y); });
}

// ---------------------------------------------------------------------------
// Entities, depth sorted
// ---------------------------------------------------------------------------

void ArenaView::drawShip(ImDrawList* dl) {
  if (!arena->hasMatch()) return;
  const MatchStatePacket& match = arena->match();
  ImVec2 base = toScreen(match.shipX, match.shipY + 34.0f);
  drawShadow(dl, ImVec2(base.x, base.y - 16.0f), 62.0f, 0.35f);
  if (shipTexture) {
    float scale = SHIP_DRAW_SIZE / shipTexture->getWidth();
    // Wrecked and listing; shudders when it is being hit.
    float damage = 1.0f - match.shipHealth / std::max(1.0f, match.shipMaxHealth);
    ImU32 tint = IM_COL32(255, static_cast<int>(255 - 90 * damage),
                          static_cast<int>(255 - 110 * damage), 255);
    drawSprite(dl, shipTexture, 0, 0, shipTexture->getWidth(),
               shipTexture->getHeight(), base, scale, scale, -0.05f, false,
               tint);
  }
  if (match.turretsOnline) {
    ImVec2 top = toScreen(match.shipX, match.shipY - 30.0f);
    float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 6.0f);
    dl->AddCircleFilled(top, 5.0f + 2.0f * pulse, IM_COL32(150, 255, 190, 230));
  }
}

void ArenaView::drawPickup(ImDrawList* dl, uint32_t itemId, float worldX,
                           float worldY) {
  float bob = std::sin(timeSeconds * 4.0f + worldX * 0.05f) * 3.0f;
  ImVec2 feet = toScreen(worldX, worldY);
  ImVec2 centre(feet.x, feet.y - 14.0f + bob);
  drawShadow(dl, feet, 9.0f, 0.3f);

  if (Arena::PickupId::isInfusion(itemId)) {
    VfxTheme theme = genusTheme(Arena::PickupId::infusionGenus(itemId));
    float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 5.0f + worldY * 0.03f);
    dl->AddCircleFilled(centre, 14.0f + 3.0f * pulse, themeColor(theme, 0.22f));
    // Diamond gem
    ImVec2 gem[4] = {ImVec2(centre.x, centre.y - 10), ImVec2(centre.x + 8, centre.y),
                     ImVec2(centre.x, centre.y + 10), ImVec2(centre.x - 8, centre.y)};
    dl->AddConvexPolyFilled(gem, 4, themeColor(theme, 1.0f));
    dl->AddPolyline(gem, 4, IM_COL32(20, 14, 24, 255), ImDrawFlags_Closed, 2.0f);
    dl->AddLine(ImVec2(centre.x - 3, centre.y - 3), ImVec2(centre.x, centre.y - 7),
                themeHot(theme, 1.0f), 2.0f);
  } else if (itemId == Arena::PickupId::Scrap) {
    dl->AddRectFilled(ImVec2(centre.x - 9, centre.y - 7),
                      ImVec2(centre.x + 9, centre.y + 7),
                      IM_COL32(150, 150, 165, 255), 2.0f);
    dl->AddRect(ImVec2(centre.x - 9, centre.y - 7),
                ImVec2(centre.x + 9, centre.y + 7), IM_COL32(20, 14, 24, 255),
                2.0f, 0, 2.0f);
    dl->AddLine(ImVec2(centre.x - 5, centre.y - 3), ImVec2(centre.x + 5, centre.y + 3),
                IM_COL32(90, 90, 105, 255), 2.0f);
    text(dl, ImVec2(centre.x, centre.y - 24), IM_COL32(220, 220, 235, 255),
         11.0f, "SCRAP");
  } else if (itemId == Arena::PickupId::Medkit) {
    dl->AddRectFilled(ImVec2(centre.x - 8, centre.y - 8),
                      ImVec2(centre.x + 8, centre.y + 8),
                      IM_COL32(240, 240, 240, 255), 2.0f);
    dl->AddRectFilled(ImVec2(centre.x - 2, centre.y - 6),
                      ImVec2(centre.x + 2, centre.y + 6),
                      IM_COL32(220, 40, 50, 255));
    dl->AddRectFilled(ImVec2(centre.x - 6, centre.y - 2),
                      ImVec2(centre.x + 6, centre.y + 2),
                      IM_COL32(220, 40, 50, 255));
  } else {
    dl->AddRectFilled(ImVec2(centre.x - 6, centre.y - 6),
                      ImVec2(centre.x + 6, centre.y + 6),
                      IM_COL32(255, 215, 0, 255));
  }
}

void ArenaView::drawPlayer(ImDrawList* dl, const ArenaClient::PlayerView& view,
                           float worldX, float worldY, bool isLocal) {
  const PlayerState& state = view.state;
  if (state.characterId == 0) return;  // still choosing a character
  const CharacterSprite* sprite = characterSprite(state.characterId);
  ImVec2 feet = toScreen(worldX, worldY);

  bool dead = (state.flags & PlayerFlags::Dead) != 0 || state.health <= 0.0f;
  bool hidden = (state.flags & PlayerFlags::Hidden) != 0;
  bool charging = (state.flags & PlayerFlags::Charging) != 0;
  bool stunned = (state.flags & PlayerFlags::Stunned) != 0;

  float vx = state.vx, vy = state.vy;
  float aimX = state.aimX, aimY = state.aimY;
  if (isLocal) {
    const Player& local = prediction->getLocalPlayer();
    vx = local.vx;
    vy = local.vy;
    aimX = prediction->getAimX();
    aimY = prediction->getAimY();
  }
  float speed = std::sqrt(vx * vx + vy * vy);
  bool moving = speed > 12.0f && !dead;

  // Face the cursor; while just walking with no aim info, face the heading.
  float faceX = aimX - worldX;
  float faceY = aimY - worldY;
  if (std::abs(faceX) + std::abs(faceY) < 1.0f) {
    faceX = moving ? vx : 0.0f;
    faceY = moving ? vy : 1.0f;
  }

  // --- procedural animation -------------------------------------------------
  float scaleX = 1.0f, scaleY = 1.0f, rotation = 0.0f;
  float offsetX = 0.0f, offsetY = 0.0f;

  if (dead) {
    rotation = PI * 0.5f;  // lying down
    offsetY = -10.0f;
  } else {
    if (moving) {
      // Walk: a little hop and a lean into the direction of travel.
      float cycle = view.animTime * (7.0f + speed * 0.012f);
      offsetY -= std::abs(std::sin(cycle)) * 3.5f;
      rotation += (vx / std::max(speed, 1.0f)) * 0.07f +
                  std::sin(cycle) * 0.035f;
    } else {
      // Idle: breathe.
      float breathe = std::sin(view.animTime * 2.4f);
      scaleY += breathe * 0.018f;
      scaleX -= breathe * 0.010f;
    }
    if (charging) {
      // Winding up: crouch and tremble.
      float progress = state.charge / 255.0f;
      scaleY *= 1.0f - 0.07f * progress;
      scaleX *= 1.0f + 0.05f * progress;
      offsetX += std::sin(view.animTime * 55.0f) * 1.2f * progress;
    }
    if (view.castMs > 0.0f && view.castTotalMs > 0.0f) {
      // Attack: lunge toward the target and snap back.
      float t = 1.0f - view.castMs / view.castTotalMs;
      float punch = std::sin(std::min(1.0f, t * 1.6f) * PI);
      float reach = view.castSlot == 0 ? 9.0f : 13.0f;
      offsetX += view.castDirX * reach * punch;
      offsetY += view.castDirY * reach * punch * 0.6f;
      scaleX *= 1.0f + 0.10f * punch;
      scaleY *= 1.0f - 0.07f * punch;
      rotation += view.castDirX * 0.12f * punch;
    }
    if (stunned) {
      rotation += std::sin(view.animTime * 9.0f) * 0.08f;
    }
  }

  ImU32 tint = IM_COL32(255, 255, 255, 255);
  if (view.hitFlashMs > 0.0f) tint = IM_COL32(255, 120, 120, 255);
  if (dead) tint = IM_COL32(150, 150, 170, 200);
  if (hidden && !dead) tint = withAlpha(tint, 0.4f);

  if (!dead) drawShadow(dl, feet, 15.0f);

  static const std::vector<EffectInstance> noEffects;
  const std::vector<EffectInstance>& playerEffects =
      effects && !dead ? effects->getEffects(state.playerId, false) : noEffects;
  effectFx.drawUnder(dl, state.playerId, false, feet, 16.0f, playerEffects);
  tint = multiplyColor(tint, effectFx.bodyTint(playerEffects));

  // Ring under the local player so you never lose yourself in a brawl.
  if (isLocal && !dead) {
    dl->AddEllipse(feet, ImVec2(20.0f, 8.5f), IM_COL32(255, 255, 255, 110), 0.0f,
                   24, 1.5f);
  }

  // Charge ring
  if (charging && !dead) {
    float progress = state.charge / 255.0f;
    dl->PathArcTo(ImVec2(feet.x, feet.y), 26.0f, -PI * 0.5f,
                  -PI * 0.5f + 2.0f * PI * progress, 32);
    dl->PathStroke(progress >= 0.98f ? IM_COL32(255, 255, 255, 255)
                                     : IM_COL32(255, 210, 90, 230),
                   0, 4.0f);
  }

  ImVec2 at(feet.x + offsetX, feet.y + offsetY);
  const CharacterDefinition* definition =
      CharacterRegistry::instance().getCharacter(state.characterId);
  const ActorSheet* actor = definition ? actorFor(definition->name) : nullptr;
  if (actor) {
    // Real animation: choose a clip from what the character is doing.
    AnimState& anim = playerAnim[state.playerId];
    if (moving || view.castMs > 0.0f || aimX != worldX || aimY != worldY) {
      anim.direction = directionFrame(faceX, faceY);
    }
    if (view.castCount != anim.castCount) {
      anim.castCount = view.castCount;
      anim.clip = actionClip(definition->name, view.castSlot, view.castCount,
                             view.castAlt);
      anim.startSeconds = timeSeconds;
    }
    const ActorAnim* clip = nullptr;
    float elapsed = 0.0f;
    if (dead) {
      clip = actor->find("death");
      elapsed = view.deadMs / 1000.0f;
    }
    if (!clip && !anim.clip.empty()) {
      const ActorAnim* action = actor->find(anim.clip);
      float t = timeSeconds - anim.startSeconds;
      if (action && t < static_cast<float>(action->frames) / action->fps) {
        clip = action;
        elapsed = t;
      } else {
        anim.clip.clear();
      }
    }
    if (!clip && view.hitMs < 220.0f) {
      clip = actor->find("hit");
      elapsed = view.hitMs / 1000.0f;
    }
    if (!clip) {
      // Characters that carry things (Presidente) have holding variants.
      bool holding = state.resource >= 200 && actor->find("hold_idle");
      clip = actor->find(holding ? (moving ? "hold_run" : "hold_idle")
                                 : (moving ? "run" : "idle"));
      elapsed = view.animTime;
    }
    if (clip) {
      drawActorFrame(dl, *actor, *clip, anim.direction, elapsed, feet, 1.0f,
                     tint, true, -1, 0.0f, !dead && !hidden);
      // Carriers (Presidente) publish where their hands are this frame.
      if (!clip->sockets.empty() && state.resource >= 200 && !dead) {
        int frame = actorFrameIndex(*clip, elapsed, true, -1);
        size_t index = static_cast<size_t>(anim.direction * clip->frames + frame);
        if (index < clip->sockets.size()) {
          const ActorSocket& socket = clip->sockets[index];
          CarryPoint& point = carryPoints[state.playerId];
          point.worldX = worldX;
          point.worldY = worldY;
          point.screenX = feet.x + socket.x * actor->scale;
          point.screenY = feet.y + socket.y * actor->scale;
          point.rollDeg = socket.rollDeg;
          point.front = socket.front;
          point.faceLeft = anim.direction >= 5;
          point.seenAt = timeSeconds;
        }
      }
    }
  } else if (sprite && sprite->texture) {
    int frame = 0;
    bool flip = false;
    if (sprite->directions == 8) {
      frame = directionFrame(faceX, faceY);
    } else {
      // Placeholder art has a single pose: mirror it for left / right.
      flip = faceX < 0.0f;
    }
    if (!dead && !hidden) {
      drawCastShadow(dl, sprite->texture, frame * sprite->cellW, 0,
                     sprite->cellW, sprite->cellH, at, 0.0f, scaleX, scaleY,
                     flip);
    }
    drawSprite(dl, sprite->texture, frame * sprite->cellW, 0, sprite->cellW,
               sprite->cellH, at, scaleX, scaleY, rotation, flip, tint);
  } else {
    // No sprite at all: a labelled capsule so the game stays playable.
    dl->AddRectFilled(ImVec2(at.x - 12, at.y - 56), ImVec2(at.x + 12, at.y),
                      IM_COL32(state.r, state.g, state.b, 255), 10.0f);
  }

  effectFx.drawOver(dl, state.playerId, false, feet, 64.0f, playerEffects);

  // Shield bubble
  if (view.shieldBubbleMs > 0.0f && !dead) {
    VfxTheme theme = view.shieldTheme ? static_cast<VfxTheme>(view.shieldTheme)
                                      : VfxTheme::Wind;
    float pulse = 0.5f + 0.5f * std::sin(view.animTime * 6.0f);
    ImVec2 centre(feet.x, feet.y - 36.0f);
    dl->AddCircleFilled(centre, 44.0f, themeColor(theme, 0.10f + 0.05f * pulse));
    dl->AddCircle(centre, 44.0f, themeHot(theme, 0.75f), 40, 2.5f);
  }
}

void ArenaView::drawEnemy(ImDrawList* dl, const ArenaClient::EnemyView& view) {
  Arena::EnemyKind kind = static_cast<Arena::EnemyKind>(view.state.type);
  if (static_cast<int>(kind) >= static_cast<int>(Arena::EnemyKind::Count)) {
    return;
  }
  const Arena::EnemyKindDef& def = Arena::enemyDef(kind);
  const EnemySprite* sprite = enemySprite(kind);
  ImVec2 feet = toScreen(view.x, view.y);
  if (feet.x < -260 || feet.x > Config::Screen::WIDTH + 260 || feet.y < -120 ||
      feet.y > Config::Screen::HEIGHT + 360) {
    return;
  }

  bool elite = (view.state.flags & EnemyFlags::Elite) != 0;
  bool stunned = (view.state.flags & EnemyFlags::Stunned) != 0;
  bool confused = (view.state.flags & EnemyFlags::Confused) != 0;
  bool boss = kind == Arena::EnemyKind::Boss;
  float scale = def.drawScale * (elite ? 1.22f : 1.0f);
  float speed = std::sqrt(view.state.vx * view.state.vx +
                          view.state.vy * view.state.vy);

  float scaleX = scale, scaleY = scale, rotation = 0.0f;
  float offsetX = 0.0f, offsetY = 0.0f;
  ImU32 tint = boss ? IM_COL32(200, 130, 255, 255) : IM_COL32(255, 255, 255, 255);
  int row = 0, frame = 0;

  if (sprite) {
    if (view.dying) {
      row = 2;
      float t = std::min(0.999f, view.deathMs / 600.0f);
      frame = static_cast<int>(t * sprite->dieFrames);
      tint = withAlpha(tint, 1.0f - std::max(0.0f, (view.deathMs - 450.0f) / 250.0f));
    } else if (speed > 8.0f && !stunned) {
      row = 1;
      frame = static_cast<int>(view.animTime * 11.0f) % sprite->runFrames;
    } else {
      row = 0;
      frame = static_cast<int>(view.animTime * 6.0f) % sprite->idleFrames;
    }
  }

  if (!view.dying) {
    if (view.attackMs > 0.0f && view.attackTotalMs > 0.0f) {
      // Windup: rear back, swell and glow toward the hit.
      float t = 1.0f - view.attackMs / view.attackTotalMs;
      float dir = view.faceLeft ? 1.0f : -1.0f;
      offsetX += dir * 5.0f * t;
      scaleX *= 1.0f + 0.10f * t;
      scaleY *= 1.0f + 0.12f * t;
      int glow = static_cast<int>(120 * t);
      tint = IM_COL32(255, 255 - glow, 255 - glow, 255);
    } else if (view.strikeMs > 0.0f) {
      // The hit: snap forward.
      float t = view.strikeMs / 160.0f;
      float dir = view.faceLeft ? -1.0f : 1.0f;
      offsetX += dir * 12.0f * t;
      scaleX *= 1.0f + 0.08f * t;
    }
    if (stunned) rotation = std::sin(view.animTime * 10.0f) * 0.10f;
    if (confused) offsetX += std::sin(view.animTime * 14.0f) * 2.0f;
    if (view.hitFlashMs > 0.0f) {
      tint = IM_COL32(255, 110, 110, 255);
      scaleX *= 1.06f;
    }
  }

  if (!view.dying) drawShadow(dl, feet, def.radius * scale / def.drawScale * 1.05f);

  static const std::vector<EffectInstance> noEnemyEffects;
  const std::vector<EffectInstance>& enemyEffects =
      effects && !view.dying ? effects->getEffects(view.state.id, true)
                             : noEnemyEffects;
  float bodyRadius = def.radius * scale / def.drawScale;
  effectFx.drawUnder(dl, view.state.id, true, feet, bodyRadius, enemyEffects);
  tint = multiplyColor(tint, effectFx.bodyTint(enemyEffects));
  if (elite && !view.dying) {
    dl->AddCircle(feet, def.radius * 1.5f, IM_COL32(255, 210, 80, 200), 28, 2.5f);
  }

  ImVec2 at(feet.x + offsetX, feet.y + offsetY);
  const ActorSheet* actor = boss ? nullptr : actorFor(def.name);
  // Grappled: in El Presidente's hands. Pinned to the carry socket his
  // animation exports, so the foe rises with his leaps, whirls with the
  // Látigo and is driven down by the powerbomb.
  bool lifted = (view.state.flags & EnemyFlags::Grappled) != 0;
  float liftRotation = 0.0f;
  const CarryPoint* carrier = lifted ? carrierNear(view.x, view.y) : nullptr;
  if (carrier) {
    // Rolls are authored for an upright body (90 = lying across his hands).
    // The aliens are long, low creatures, so a quarter turn more puts them
    // on their backs, legs up.
    liftRotation = (carrier->rollDeg + 90.0f) * (PI / 180.0f) *
                   (carrier->faceLeft ? -1.0f : 1.0f);
    // Put the foe's middle on his hands. Sprites rotate about the bottom
    // of their cell, which sits `drop` pixels below their feet.
    float drop = actor ? (actor->cellH - actor->pivotY) * scale * actor->scale : 0.0f;
    float half = actor ? actor->pivotY * 0.3f * scale * actor->scale
                       : (sprite ? sprite->cellH * 0.3f * scale : 16.0f);
    float reach = drop + half;
    ImVec2 base(carrier->screenX - reach * std::sin(liftRotation),
                carrier->screenY + reach * std::cos(liftRotation));
    feet = ImVec2(base.x, base.y - drop);
    at = ImVec2(feet.x + offsetX, feet.y + offsetY);
  } else if (lifted && (view.state.flags & EnemyFlags::HeldLow) != 0) {
    // Crushed against his chest in the bear hug.
    at.y -= 34.0f;
    feet.y -= 34.0f;
  } else if (lifted) {
    at.y -= 100.0f;
    feet.y -= 100.0f;
    liftRotation = view.faceLeft ? -1.45f : 1.45f;
    at.x += view.faceLeft ? 18.0f : -18.0f;
    feet.x += view.faceLeft ? 18.0f : -18.0f;
  }
  if (actor) {
    AnimState& anim = enemyAnim[view.state.id];
    float faceX = 0.0f, faceY = 0.0f;
    if (view.attackMs > 0.0f || view.strikeMs > 0.0f) {
      faceX = view.aimX - view.x;
      faceY = view.aimY - view.y;
    } else if (speed > 8.0f) {
      faceX = view.state.vx;
      faceY = view.state.vy;
    }
    if (std::abs(faceX) + std::abs(faceY) > 0.5f) {
      anim.direction = directionFrame(faceX, faceY);
    }
    const ActorAnim* clip = nullptr;
    float elapsed = view.animTime;
    int forced = -1;
    if (view.dying) {
      clip = actor->find("death");
      elapsed = view.deathMs / 1000.0f;
    } else if (view.attackMs > 0.0f || view.strikeMs > 0.0f) {
      // Hold the coil for the whole telegraph, then strike when it lands.
      clip = actor->find("attack");
      if (clip) {
        float windup = static_cast<float>(clip->frames) * 0.45f;
        if (view.attackMs > 0.0f && view.attackTotalMs > 0.0f) {
          float p = 1.0f - view.attackMs / view.attackTotalMs;
          forced = static_cast<int>(p * windup);
        } else {
          float p = 1.0f - view.strikeMs / 160.0f;
          forced = static_cast<int>(windup + p * (clip->frames - windup));
        }
      }
    } else if (view.hitMs < 180.0f) {
      clip = actor->find("hit");
      elapsed = view.hitMs / 1000.0f;
    }
    if (!clip) clip = actor->find(speed > 8.0f && !stunned ? "run" : "idle");
    if (clip) {
      ImU32 actorTint = tint;
      if (view.hitFlashMs > 0.0f) actorTint = IM_COL32(255, 150, 150, 255);
      if (lifted) {
        clip = actor->find("hit");
        forced = 1;
      }
      if (clip) {
        drawActorFrame(dl, *actor, *clip, anim.direction, elapsed, feet, scale,
                       actorTint, true, forced, liftRotation,
                       !view.dying && !lifted);
      }
    }
  } else if (sprite && sprite->texture) {
    if (!view.dying && !lifted) {
      drawCastShadow(dl, sprite->texture, frame * sprite->cellW,
                     row * sprite->cellH, sprite->cellW, sprite->cellH, at, 0.0f,
                     scaleX, scaleY, view.faceLeft);
    }
    drawSprite(dl, sprite->texture, frame * sprite->cellW, row * sprite->cellH,
               sprite->cellW, sprite->cellH, at, scaleX, scaleY,
               rotation + liftRotation, view.faceLeft, tint);
  } else {
    dl->AddCircleFilled(ImVec2(at.x, at.y - def.radius), def.radius,
                        themeColor(genusTheme(def.genus), 1.0f));
  }
  effectFx.drawOver(dl, view.state.id, true, feet,
                    std::max(26.0f, bodyRadius * 3.2f), enemyEffects);
}

void ArenaView::drawEntities(ImDrawList* dl) {
  struct Item {
    float depth;
    int kind;  // 0 player, 1 enemy, 2 pickup, 3 ship
    const void* ptr;
    uint32_t id;
    float x, y;
  };
  std::vector<Item> items;
  items.reserve(128);

  uint32_t me = localId();
  const Player& local = prediction->getLocalPlayer();
  for (const auto& [id, view] : arena->players()) {
    float x = view.x, y = view.y;
    if (id == me) {
      // Draw the local player where prediction puts them: no input lag.
      x = local.x;
      y = local.y;
    }
    items.push_back({y, 0, &view, id, x, y});
  }
  for (const auto& [id, view] : arena->enemies()) {
    // Grappled foes are in their captor's arms: draw them just in front of
    // or behind him, as his animation says.
    const CarryPoint* carrier = (view.state.flags & EnemyFlags::Grappled)
                                    ? carrierNear(view.x, view.y)
                                    : nullptr;
    float depth = carrier ? carrier->worldY + (carrier->front ? 0.5f : -0.5f)
                  : (view.state.flags & EnemyFlags::Grappled) ? view.y + 80.0f
                                                             : view.y;
    items.push_back({depth, 1, &view, id, view.x, view.y});
  }
  for (const auto& [id, item] : prediction->getWorldItems()) {
    items.push_back({item.y - 1.0f, 2, nullptr, item.itemId, item.x, item.y});
  }
  if (arena->hasMatch()) {
    items.push_back({arena->match().shipY + 30.0f, 3, nullptr, 0, 0, 0});
  }

  std::sort(items.begin(), items.end(),
            [](const Item& a, const Item& b) { return a.depth < b.depth; });

  // The look: every entity's shadow lies under every entity. Shadows go to
  // draw channel 0 (matte), bodies and their effects to channel 1.
  const bool shadowPass = look.layer != nullptr;
  if (shadowPass) {
    dl->AddCallback(matteCallback, nullptr);
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    dl->AddCallback(glowCallback, nullptr);
    look.shadowChannel = true;
  }

  for (const Item& item : items) {
    switch (item.kind) {
      case 0:
        drawPlayer(dl, *static_cast<const ArenaClient::PlayerView*>(item.ptr),
                   item.x, item.y, item.id == me);
        break;
      case 1:
        drawEnemy(dl, *static_cast<const ArenaClient::EnemyView*>(item.ptr));
        break;
      case 2:
        drawPickup(dl, item.id, item.x, item.y);
        break;
      case 3:
        drawShip(dl);
        break;
    }
  }

  if (shadowPass) {
    look.shadowChannel = false;
    dl->ChannelsMerge();
  }
}

// ---------------------------------------------------------------------------
// Air layer: attacks
// ---------------------------------------------------------------------------

void ArenaView::drawAir(ImDrawList* dl) {
  effectFx.drawVerbs(
      dl, [this](float x, float y) { return toScreen(x, y); },
      [this](uint32_t id, bool isEnemy, float& x, float& y) {
        if (isEnemy) {
          auto it = arena->enemies().find(id);
          if (it == arena->enemies().end()) return false;
          x = it->second.x;
          y = it->second.y;
          return true;
        }
        if (id == localId()) return localPosition(x, y);
        auto it = arena->players().find(id);
        if (it == arena->players().end()) return false;
        x = it->second.x;
        y = it->second.y;
        return true;
      });
  for (const ArenaClient::Fx& f : arena->effects()) {
    float t = std::min(1.0f, f.ageMs / f.lifeMs);
    float fade = 1.0f - t;

    switch (f.kind) {
      case GameEventKind::Slash: {
        // A blade trail: a bright leading edge sweeping across the arc with
        // softer echoes trailing behind it.
        ImVec2 origin = toScreen(f.x, f.y - PROJECTILE_HEIGHT);
        float angle = std::atan2(f.y2, f.x2);
        float half = f.value2 * 0.5f * PI / 180.0f;
        float sweep = ease(t * 1.7f);
        float head = angle - half + 2.0f * half * sweep;
        float start = angle - half;
        float radius = f.value * (0.86f + 0.14f * sweep);
        int segments = std::max(8, static_cast<int>(half * radius / 8.0f));
        // Echoes: wider, fainter, slightly smaller.
        for (int i = 3; i >= 1; i--) {
          float r = radius * (1.0f - 0.09f * i);
          float tail = std::max(start, head - half * (0.5f + 0.35f * i));
          dl->PathArcTo(origin, r, tail, head, segments);
          dl->PathStroke(themeColor(f.theme, (0.55f - 0.13f * i) * fade), 0,
                         radius * 0.11f * (1.0f + 0.5f * i));
        }
        float tail = std::max(start, head - half * 0.9f);
        dl->PathArcTo(origin, radius, tail, head, segments);
        dl->PathStroke(themeHot(f.theme, 0.85f * fade), 0, radius * 0.13f);
        dl->PathArcTo(origin, radius, std::max(start, head - half * 0.45f), head,
                      segments);
        dl->PathStroke(IM_COL32(255, 255, 255, static_cast<int>(255 * fade)), 0,
                       radius * 0.05f);
        // Spark at the tip of the swing.
        ImVec2 tip(origin.x + std::cos(head) * radius,
                   origin.y + std::sin(head) * radius);
        dl->AddCircleFilled(tip, radius * 0.07f * fade + 1.0f,
                            themeHot(f.theme, fade));
        break;
      }
      case GameEventKind::Cone: {
        ImVec2 origin = toScreen(f.x, f.y - PROJECTILE_HEIGHT * 0.6f);
        float angle = std::atan2(f.y2, f.x2);
        float half = f.value2 * 0.5f * PI / 180.0f;
        float reach = f.value * (0.35f + 0.65f * ease(t * 2.2f));
        sector(dl, origin, reach, angle, half, themeColor(f.theme, 0.38f * fade),
               themeHot(f.theme, 0.85f * fade), 2.5f);
        // Streaks radiating outward sell the direction.
        for (int i = -2; i <= 2; i++) {
          float a = angle + half * 0.38f * i;
          dl->AddLine(ImVec2(origin.x + std::cos(a) * reach * 0.35f,
                             origin.y + std::sin(a) * reach * 0.35f),
                      ImVec2(origin.x + std::cos(a) * reach * 0.95f,
                             origin.y + std::sin(a) * reach * 0.95f),
                      themeHot(f.theme, 0.5f * fade), 2.0f);
        }
        break;
      }
      case GameEventKind::Burst: {
        float lift = f.param == 1 ? PROJECTILE_HEIGHT : 0.0f;
        ImVec2 centre = toScreen(f.x, f.y - lift);
        float radius = f.value * (0.55f + 0.45f * ease(t * 1.6f));
        // Fire bursts use the explosion sheet from the Unity project.
        auto sheet = fxSprites.find("explosion");
        if (f.theme == VfxTheme::Fire && sheet != fxSprites.end() &&
            f.value >= 40.0f) {
          const FxSprite& sprite = sheet->second;
          int frame = std::min(sprite.frames - 1,
                               static_cast<int>(t * sprite.frames));
          float size = f.value * 2.5f;
          float texW = static_cast<float>(sprite.texture->getWidth());
          dl->AddImage(textureId(sprite.texture),
                       ImVec2(centre.x - size * 0.5f, centre.y - size * 0.62f),
                       ImVec2(centre.x + size * 0.5f, centre.y + size * 0.38f),
                       ImVec2(frame * sprite.cellW / texW, 0.0f),
                       ImVec2((frame + 1) * sprite.cellW / texW, 1.0f),
                       IM_COL32(255, 255, 255, static_cast<int>(255 * (1.0f - t * t))));
        }
        dl->AddCircleFilled(centre, radius, themeColor(f.theme, 0.34f * fade), 36);
        dl->AddCircleFilled(centre, radius * 0.55f,
                            themeHot(f.theme, 0.40f * fade * fade), 28);
        dl->AddCircle(centre, radius, themeHot(f.theme, 0.9f * fade), 36, 3.0f);
        break;
      }
      case GameEventKind::Ring: {
        ImVec2 centre = toScreen(f.x, f.y);
        float radius = f.value * (0.25f + 0.75f * ease(t));
        dl->AddCircle(centre, radius, themeColor(f.theme, 0.9f * fade), 48,
                      2.0f + 5.0f * fade);
        dl->AddCircle(centre, radius * 0.8f, themeHot(f.theme, 0.4f * fade), 40,
                      2.0f);
        break;
      }
      case GameEventKind::Beam: {
        ImVec2 a = toScreen(f.x, f.y - PROJECTILE_HEIGHT);
        ImVec2 b = toScreen(f.x2, f.y2 - PROJECTILE_HEIGHT);
        dl->AddLine(a, b, themeColor(f.theme, 0.5f * fade), f.value * 1.8f);
        dl->AddLine(a, b, themeHot(f.theme, 0.95f * fade),
                    std::max(1.5f, f.value * 0.6f));
        break;
      }
      case GameEventKind::Lob: {
        // Thrown object on an arc, with its shadow sliding along the ground.
        float gx = f.x + (f.x2 - f.x) * t;
        float gy = f.y + (f.y2 - f.y) * t;
        float dx = f.x2 - f.x, dy = f.y2 - f.y;
        float height = (70.0f + std::sqrt(dx * dx + dy * dy) * 0.18f) * 4.0f *
                       t * (1.0f - t);
        ImVec2 ground = toScreen(gx, gy);
        ImVec2 pos(ground.x, ground.y - PROJECTILE_HEIGHT - height);
        drawShadow(dl, ground, 7.0f, 0.3f);
        auto can = fxSprites.find("brew_can");
        if (f.param == 2 && can != fxSprites.end()) {
          const FxSprite& sprite = can->second;
          int frame = static_cast<int>(f.ageMs / 45.0f) % sprite.frames;
          float texW = static_cast<float>(sprite.texture->getWidth());
          dl->AddImage(textureId(sprite.texture),
                       ImVec2(pos.x - 20, pos.y - 20), ImVec2(pos.x + 20, pos.y + 20),
                       ImVec2(frame * sprite.cellW / texW, 0.0f),
                       ImVec2((frame + 1) * sprite.cellW / texW, 1.0f));
        } else {
          float r = f.param == 3 ? 9.0f : 7.0f;
          dl->AddCircleFilled(pos, r + 4.0f, themeColor(f.theme, 0.35f));
          dl->AddCircleFilled(pos, r, IM_COL32(40, 36, 46, 255));
          dl->AddCircle(pos, r, themeHot(f.theme, 1.0f), 16, 2.0f);
        }
        break;
      }
      case GameEventKind::DashTrail: {
        ImVec2 a = toScreen(f.x, f.y - PROJECTILE_HEIGHT);
        ImVec2 b = toScreen(f.x2, f.y2 - PROJECTILE_HEIGHT);
        // Three stacked streaks that thin out as they fade.
        for (int i = -1; i <= 1; i++) {
          ImVec2 off(0.0f, i * 12.0f);
          dl->AddLine(ImVec2(a.x, a.y + off.y), ImVec2(b.x, b.y + off.y),
                      themeColor(f.theme, (i == 0 ? 0.75f : 0.4f) * fade),
                      (i == 0 ? 10.0f : 5.0f) * fade + 1.0f);
        }
        dl->AddCircleFilled(a, 16.0f * fade, themeColor(f.theme, 0.3f * fade));
        break;
      }
      case GameEventKind::Tether: {
        ImVec2 a = toScreen(f.x, f.y - PROJECTILE_HEIGHT);
        ImVec2 b = toScreen(f.x2, f.y2 - PROJECTILE_HEIGHT);
        float alpha = std::min(1.0f, fade * 2.5f);
        // A gently waving strand of light.
        const int steps = 14;
        float nx = -(b.y - a.y), ny = b.x - a.x;
        float len = std::sqrt(nx * nx + ny * ny);
        if (len > 0.001f) {
          nx /= len;
          ny /= len;
        }
        for (int i = 0; i <= steps; i++) {
          float u = static_cast<float>(i) / steps;
          float wave = std::sin(u * PI * 3.0f + timeSeconds * 12.0f) * 6.0f *
                       std::sin(u * PI);
          dl->PathLineTo(ImVec2(a.x + (b.x - a.x) * u + nx * wave,
                                a.y + (b.y - a.y) * u + ny * wave));
        }
        dl->PathStroke(themeHot(f.theme, 0.9f * alpha), 0, 3.5f);
        dl->AddLine(a, b, themeColor(f.theme, 0.3f * alpha), 8.0f);
        dl->AddCircleFilled(b, 9.0f, themeColor(f.theme, 0.5f * alpha));
        break;
      }
      case GameEventKind::HitSpark: {
        // Four-point star that snaps open and vanishes.
        ImVec2 centre = toScreen(f.x, f.y - 26.0f);
        float size = f.value * (0.5f + 0.5f * ease(t * 2.0f));
        float spin = f.x2;
        for (int i = 0; i < 4; i++) {
          float a = spin + i * PI * 0.5f;
          float len = size * (i % 2 == 0 ? 1.0f : 0.6f);
          ImVec2 tip(centre.x + std::cos(a) * len, centre.y + std::sin(a) * len);
          ImVec2 back(centre.x - std::cos(a) * len, centre.y - std::sin(a) * len);
          dl->AddLine(back, tip, themeHot(f.theme, fade), 3.0f * fade + 1.0f);
        }
        dl->AddCircleFilled(centre, size * 0.32f, themeHot(f.theme, 0.9f * fade));
        break;
      }
      case GameEventKind::SpawnPuff: {
        ImVec2 centre = toScreen(f.x, f.y);
        float radius = 10.0f + 26.0f * ease(t);
        dl->AddCircle(centre, radius, themeColor(f.theme, 0.8f * fade), 20, 3.0f);
        for (int i = 0; i < 5; i++) {
          float a = i * 1.26f + f.x * 0.01f;
          dl->AddCircleFilled(
              ImVec2(centre.x + std::cos(a) * radius,
                     centre.y + std::sin(a) * radius * 0.6f - 20.0f * t),
              4.0f * fade + 1.0f, themeColor(f.theme, 0.7f * fade));
        }
        break;
      }
      default:
        break;
    }
  }

  // Projectiles
  for (const ArenaClient::Projectile& shot : arena->projectiles()) {
    if (shot.ageMs > shot.lifeMs) continue;
    ImVec2 ground = toScreen(shot.x, shot.y);
    ImVec2 pos(ground.x, ground.y - PROJECTILE_HEIGHT);
    float angle = std::atan2(shot.vy, shot.vx);
    drawShadow(dl, ground, shot.radius * 0.7f, 0.25f);

    // Enemy shots get a red rim: threat colour beats genus colour.
    ImU32 rim = shot.fromEnemy ? themeColor(VfxTheme::Danger, 1.0f)
                               : themeHot(shot.theme, 1.0f);
    if (shot.sprite == 1) {
      // Crescent of wind, bowed toward its heading.
      float r = shot.radius * 1.6f;
      ImVec2 centre(pos.x - std::cos(angle) * r * 0.6f,
                    pos.y - std::sin(angle) * r * 0.6f);
      dl->PathArcTo(centre, r, angle - 1.0f, angle + 1.0f, 14);
      dl->PathStroke(themeColor(shot.theme, 0.45f), 0, 11.0f);
      dl->PathArcTo(centre, r, angle - 0.9f, angle + 0.9f, 14);
      dl->PathStroke(themeHot(shot.theme, 0.95f), 0, 4.0f);
    } else if (shot.sprite == 3) {
      // Bomb ball with a lit fuse.
      dl->AddCircleFilled(pos, shot.radius + 5.0f, themeColor(shot.theme, 0.3f));
      dl->AddCircleFilled(pos, shot.radius, IM_COL32(44, 40, 52, 255));
      dl->AddCircle(pos, shot.radius, IM_COL32(230, 230, 240, 255), 16, 2.0f);
      float flicker = 0.5f + 0.5f * std::sin(timeSeconds * 40.0f);
      dl->AddCircleFilled(ImVec2(pos.x + 4, pos.y - shot.radius),
                          2.5f + 2.0f * flicker, themeHot(shot.theme, 1.0f));
    } else {
      // Orb with a short tail.
      ImVec2 tail(pos.x - std::cos(angle) * shot.radius * 2.6f,
                  pos.y - std::sin(angle) * shot.radius * 2.6f);
      dl->AddLine(tail, pos, themeColor(shot.theme, 0.5f), shot.radius * 1.2f);
      dl->AddCircleFilled(pos, shot.radius + 5.0f, themeColor(shot.theme, 0.28f));
      dl->AddCircleFilled(pos, shot.radius, themeColor(shot.theme, 1.0f));
      dl->AddCircleFilled(pos, shot.radius * 0.5f, themeHot(shot.theme, 1.0f));
      dl->AddCircle(pos, shot.radius, rim, 16, 2.0f);
    }
  }
}

// ---------------------------------------------------------------------------
// Overhead: health, status, numbers
// ---------------------------------------------------------------------------

void ArenaView::drawOverhead(ImDrawList* dl) {
  uint32_t me = localId();
  const Player& local = prediction->getLocalPlayer();


  // Objectives: name and progress
  for (const auto& [id, objective] : prediction->getObjectives()) {
    ImVec2 centre = toZoomedScreen(objective.x, objective.y);
    if (centre.x < -100 || centre.x > Config::Screen::WIDTH + 100 ||
        centre.y < -100 || centre.y > Config::Screen::HEIGHT + 100) {
      continue;
    }
    const char* label = "Objective";
    switch (objective.type) {
      case ObjectiveType::AlienScrapyard:
        label = "Scrapyard";
        break;
      case ObjectiveType::DerelictTurret:
        label = "Derelict Turret";
        break;
      case ObjectiveType::CaptureOutpost:
        label = "Outpost";
        break;
      case ObjectiveType::SalvageMedpacks:
        label = "Medpacks";
        break;
      default:
        break;
    }
    bool done = objective.state == ObjectiveState::Completed;
    text(dl, ImVec2(centre.x, centre.y + 20.0f),
         done ? IM_COL32(130, 220, 150, 170) : IM_COL32(255, 228, 130, 255),
         13.0f, done ? "done" : label);
    if (objective.state == ObjectiveState::InProgress &&
        objective.progress > 0.0f) {
      bar(dl, ImVec2(centre.x - 34, centre.y + 38), 68, 7, objective.progress,
          IM_COL32(90, 220, 255, 255));
    }
  }

  // Enemies: health bar once hurt (always for elites and the boss)
  for (const auto& [id, view] : arena->enemies()) {
    if (view.dying) continue;
    Arena::EnemyKind kind = static_cast<Arena::EnemyKind>(view.state.type);
    if (static_cast<int>(kind) >= static_cast<int>(Arena::EnemyKind::Count)) {
      continue;
    }
    const Arena::EnemyKindDef& def = Arena::enemyDef(kind);
    const EnemySprite* sprite = enemySprite(kind);
    ImVec2 feet = toZoomedScreen(view.x, view.y);
    if (feet.x < -100 || feet.x > Config::Screen::WIDTH + 100 || feet.y < -50 ||
        feet.y > Config::Screen::HEIGHT + 250) {
      continue;
    }
    bool elite = (view.state.flags & EnemyFlags::Elite) != 0;
    const ActorSheet* enemyActor =
        kind == Arena::EnemyKind::Boss ? nullptr : actorFor(def.name);
    float spriteHeight = enemyActor ? enemyActor->pivotY * enemyActor->scale * 0.72f
                                    : (sprite ? sprite->cellH : 40);
    float height = spriteHeight * def.drawScale * (elite ? 1.22f : 1.0f) *
                   camera->zoom;
    float top = feet.y - height - 8.0f;
    bool hurt = view.state.health < view.state.maxHealth - 0.5f;
    if (hurt || elite) {
      float width = std::max(26.0f, def.radius * 2.2f);
      bar(dl, ImVec2(feet.x - width * 0.5f, top), width, 4.0f,
          view.state.health / std::max(1.0f, view.state.maxHealth),
          elite ? IM_COL32(255, 200, 70, 255) : IM_COL32(230, 60, 60, 255));
    }
    // Effects: full badges on elites and the boss, small pips on the rest
    // (auras and tints already say a lot; a pack of badges says nothing).
    if (effects) {
      bool notable = elite || kind == Arena::EnemyKind::Boss;
      if (notable) {
        effectFx.drawBadges(dl, id, true, ImVec2(feet.x, top - 11.0f),
                            effects->getEffects(id, true), 3);
      } else {
        effectFx.drawPips(dl, ImVec2(feet.x, top - 8.0f),
                          effects->getEffects(id, true), 3);
      }
    }
  }

  // Players: name, health, shield, effects
  for (const auto& [id, view] : arena->players()) {
    if (view.state.characterId == 0) continue;
    float x = view.x, y = view.y;
    if (id == me) {
      x = local.x;
      y = local.y;
    }
    ImVec2 feet = toZoomedScreen(x, y);
    const CharacterSprite* sprite = characterSprite(view.state.characterId);
    const CharacterDefinition* heightDef =
        CharacterRegistry::instance().getCharacter(view.state.characterId);
    const ActorSheet* heightActor =
        heightDef ? actorFor(heightDef->name) : nullptr;
    float height = (heightActor ? heightActor->pivotY * 0.82f
                                : (sprite ? sprite->cellH : 60.0f)) *
                   camera->zoom;
    bool dead = view.state.health <= 0.0f;
    float top = feet.y - height - 12.0f;

    const CharacterDefinition* def =
        CharacterRegistry::instance().getCharacter(view.state.characterId);
    const char* name = def ? def->name.c_str() : "?";

    if (dead) {
      text(dl, ImVec2(feet.x, feet.y - 44.0f), IM_COL32(255, 110, 110, 255),
           13.0f, "DOWN");
      const MatchPlayerState* mp = arena->matchPlayer(id);
      if (mp && mp->reviveProgress > 0.0f) {
        bar(dl, ImVec2(feet.x - 24, feet.y - 28), 48, 6, mp->reviveProgress,
            IM_COL32(120, 255, 170, 255));
      }
      continue;
    }

    const float width = 46.0f;
    float fraction = view.state.health / std::max(1.0f, view.state.maxHealth);
    ImU32 fill = id == me ? IM_COL32(90, 230, 110, 255)
                          : IM_COL32(90, 190, 255, 255);
    bar(dl, ImVec2(feet.x - width * 0.5f, top), width, 5.0f, fraction, fill);
    if (view.state.shield > 0.0f) {
      float shield =
          std::min(1.0f, view.state.shield / std::max(1.0f, view.state.maxHealth));
      dl->AddRectFilled(ImVec2(feet.x - width * 0.5f, top - 3.0f),
                        ImVec2(feet.x - width * 0.5f + width * shield, top - 1.0f),
                        IM_COL32(200, 235, 255, 255));
    }
    if (id != me) {
      text(dl, ImVec2(feet.x, top - 15.0f), IM_COL32(210, 230, 255, 255), 12.0f,
           name);
    }
    if (effects) {
      effectFx.drawBadges(dl, id, false,
                          ImVec2(feet.x, top - (id != me ? 25.0f : 11.0f)),
                          effects->getEffects(id, false), 4);
    }
    const MatchPlayerState* mp = arena->matchPlayer(id);
    if (mp && mp->carryingScrap) {
      text(dl, ImVec2(feet.x, top - (id == me ? 15.0f : 41.0f)),
           IM_COL32(255, 200, 110, 255), 11.0f, "carrying scrap");
    }
  }

  // Floating numbers and status words
  for (const ArenaClient::FloatText& t : arena->floatTexts()) {
    float u = t.ageMs / t.lifeMs;
    ImVec2 pos = toZoomedScreen(t.x + t.driftX * u, t.y - 34.0f * ease(u));
    float alpha = u < 0.7f ? 1.0f : (1.0f - u) / 0.3f;
    // Pop in slightly larger, then settle.
    float size = t.size * (1.0f + 0.35f * std::max(0.0f, 1.0f - u * 6.0f));
    text(dl, pos, withAlpha(t.color, alpha), size, t.text.c_str());
  }

  // Effect callouts ("SPREAD x23", "CORRUPTED") over everything else.
  effectFx.drawCallouts(
      dl, [this](float x, float y) { return toZoomedScreen(x, y); });
}
