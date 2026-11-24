// World rendering for the arena: ground markers, characters, enemies, attack
// effects and floating text. The HUD lives in ArenaHud.cpp.

#include "ArenaView.h"

#include <imgui.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ArenaDraw.h"
#include "CharacterRegistry.h"
#include "CollisionSystem.h"
#include "ClientPrediction.h"
#include "Effect.h"
#include "EventBus.h"
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
constexpr float SHIP_DRAW_SIZE = 192.0f;  // the old flat sprite (fallback)
// How each ship tier (Arena::Tuning::SHIP_TIER_*) is drawn: its forge sheet,
// the sheet column (direction) that matches the crash (cab lower left,
// rockets upper right), and where its ground centre stands relative to the
// ship's centre, so the dug-out ship sits on the crash site.
struct ShipTierLook {
  const char* actor;
  int direction;
  float offsetX, offsetY;
  float shadowRadius;
};
constexpr ShipTierLook SHIP_TIER_LOOK[Arena::Tuning::SHIP_TIER_COUNT] = {
    {"Ship", 0, 0.0f, 0.0f, 220.0f},
    {"ShipPatched", 1, -20.0f, -90.0f, 230.0f},
    {"ShipFort", 1, -20.0f, -90.0f, 270.0f},
};

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
const char* objectiveLabel(ObjectiveType type) {
  switch (type) {
    case ObjectiveType::AlienScrapyard:
      return "Scrapyard";
    case ObjectiveType::DerelictTurret:
      return "Derelict Turret";
    case ObjectiveType::CaptureOutpost:
      return "Outpost";
    case ObjectiveType::SalvageMedpacks:
      return "Med Pod";
    case ObjectiveType::RecoverProbe:
      return "Crashed Probe";
    case ObjectiveType::SaveTheFrogs:
      return "Frogs";
    case ObjectiveType::NoxiousGas:
      return "Toxic Spores";
    case ObjectiveType::LittleJohn:
      return "Guarded Gate";
    case ObjectiveType::DigPitTrap:
      return "Pit Trap";
    case ObjectiveType::StringTripwire:
      return "Tripwire";
  }
  return "Objective";
}

int directionFrame(float dirX, float dirY) {
  float degrees = std::atan2(dirY, dirX) * 180.0f / PI;  // E = 0, S = 90
  int index = static_cast<int>(std::lround((90.0f - degrees) / 45.0f));
  return ((index % 8) + 8) % 8;
}

}  // namespace

ArenaView::ArenaView(ArenaClient* arena, ClientPrediction* prediction,
                     EffectTracker* effects, Camera* camera)
    : arena(arena), prediction(prediction), effects(effects), camera(camera) {
  // Picking up an infusion says what it is and what to do with it.
  EventBus::instance().subscribe<ItemPickedUpEvent>(
      [this](const ItemPickedUpEvent& e) { onLocalPickup(e.itemId); });
}

void ArenaView::onLocalPickup(uint32_t itemId) {
  if (!Arena::PickupId::isInfusion(itemId)) return;
  Arena::Genus genus = Arena::PickupId::infusionGenus(itemId);
  const Arena::InfusionPartInfo& part =
      Arena::partInfo(Arena::PickupId::infusionPart(itemId));
  std::string title = std::string("+ ") + Arena::genusName(genus) + " " +
                      part.name;
  for (char& ch : title) ch = static_cast<char>(std::toupper(ch));
  // A handful at once is one banner, not a stack of them; each still
  // flashes.
  if (timeSeconds - lastPickupBanner > 2.0f) {
    arena->pushBanner(title,
                      "Infusion! Slot it into your suit at the ship  [B]",
                      themeHot(genusTheme(genus), 1.0f), 2600.0f);
    lastPickupBanner = timeSeconds;
  }
  pickupFlashGenus = genus;
  pickupFlashUntil = timeSeconds + 0.7f;
}

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
                       uint32_t castCount, bool alt, uint8_t variant) {
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
  if (character == "Nyx") {
    // Become the Swarm plays from her form (drop, lift), not from the cast.
    static const char* CLIPS[4] = {"swarm", "plague", "cloud", ""};
    return CLIPS[std::max(0, std::min(3, slot))];
  }
  if (character == "Nolan") {
    static const char* CLIPS[4] = {"sweep", "lotus", "blackout", "blink"};
    return CLIPS[std::max(0, std::min(3, slot))];
  }
  if (character == "Kade") {
    // Focused Blast's hold plays "beam_charge" then "beam_hold"; the cast is
    // the release. Full Release plays from his mode ("release_hold", then
    // "spent"), not from the cast.
    static const char* CLIPS[4] = {"slap", "beam", "", "step"};
    return CLIPS[std::max(0, std::min(3, slot))];
  }
  if (character == "Jeff") {
    // Acquire Biomass's hold plays "harvest_charge"; the cast is the
    // release. Each sheet (legs, treads) has its own "transform": the one
    // on the sheet he is changing into.
    static const char* CLIPS[4] = {"fire", "harvest", "discharge", "transform"};
    return CLIPS[std::max(0, std::min(3, slot))];
  }
  if (character == "Isaac") {
    // Lifeline's hold plays "lifeline_charge"; the cast is the release.
    static const char* CLIPS[4] = {"ray", "lifeline", "blessing", "flash"};
    return CLIPS[std::max(0, std::min(3, slot))];
  }
  if (character == "Wade") {
    // The server picks the punch (Arena::WadeForm); play what it threw.
    switch (slot) {
      case 0: {
        static const char* PUNCHES[Arena::WadeForm::PunchCount] = {
            "jab", "cross", "hook_L", "hook_R", "cross_body", "hook_body"};
        return PUNCHES[variant < Arena::WadeForm::PunchCount ? variant : 0];
      }
      case 1:
        return variant == Arena::WadeForm::RearUppercut ? "uppercut_R"
                                                        : "uppercut_L";
      case 2:
        return variant == Arena::WadeForm::Haymaker ? "haymaker" : "slip";
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
    // The server picks the form (Arena::GravonForm); play what it threw.
    switch (slot) {
      case 0:
        switch (variant) {
          case Arena::GravonForm::SlashHeave:
            return "heave";  // off his back
          case Arena::GravonForm::SlashHeaveRun:
            return "heave_run";  // up from the drag
          case Arena::GravonForm::SlashBackhand:
            return "slash_b";
          default:
            return "slash_f";
        }
      case 1:
        return "ravager";  // the leap, the landing, the haul out
      case 2:
        return "fury";
      case 3:
        return variant == Arena::GravonForm::RushStab ? "rush_stab" : "rush";
      default:
        return "slash_f";
    }
  }
  static const char* GENERIC[5] = {"attack", "secondary", "special",
                                   "mobility", "special"};
  return GENERIC[std::max(0, std::min(4, slot))];
}

// ---- Nyx's nanites ---------------------------------------------------------
// Square pixels in her palette, stateless: each one is a hash of its index
// and the clock, so nothing is stored per particle.

float nanoHash(uint32_t n) {
  n = (n << 13u) ^ n;
  n = n * (n * n * 15731u + 789221u) + 1376312589u;
  return static_cast<float>(n & 0x7fffffffu) / 2147483647.0f;
}

float fract(float v) { return v - std::floor(v); }

// The coat's magentas and pinks, the screen's cyan and blue, her skin's
// orange, the trousers' cream. `bias` 0 = mostly coat, 1 = mostly screen.
ImU32 naniteColour(float pick, float bias, float alpha) {
  static const Rgb COAT[3] = {
      {0.78f, 0.12f, 0.50f}, {0.95f, 0.36f, 0.62f}, {0.62f, 0.10f, 0.42f}};
  static const Rgb SCREEN[2] = {{0.36f, 0.88f, 1.00f}, {0.26f, 0.42f, 0.95f}};
  static const Rgb WARM[2] = {{0.98f, 0.68f, 0.26f}, {0.95f, 0.92f, 0.82f}};
  float coatShare = 0.72f - 0.55f * bias;
  float screenShare = 0.18f + 0.55f * bias;
  Rgb c;
  if (pick < coatShare) {
    c = COAT[static_cast<int>(pick / coatShare * 2.99f)];
  } else if (pick < coatShare + screenShare) {
    c = SCREEN[static_cast<int>((pick - coatShare) / screenShare * 1.99f)];
  } else {
    c = WARM[pick > 0.97f ? 1 : 0];
  }
  return rgba(c, alpha);
}

void naniteSquare(ImDrawList* dl, float x, float y, float size, ImU32 col) {
  float half = std::max(1.0f, std::round(size)) * 0.5f;
  x = std::round(x);
  y = std::round(y);
  dl->AddRectFilled(ImVec2(x - half, y - half), ImVec2(x + half, y + half), col);
}

// Her body fraying into pixels: they leave the coat's edges and hem and
// drift off, outward and up. `amount` 1 is her steady state; more while she
// dissolves.
void drawNaniteDissolve(ImDrawList* dl, ImVec2 feet, float t, float amount,
                        float alpha) {
  int count = static_cast<int>(22.0f * amount);
  for (int i = 0; i < count; i++) {
    uint32_t seed = static_cast<uint32_t>(i) * 7919u + 17u;
    float period = 0.8f + 0.9f * nanoHash(seed);
    float phase = fract(t / period + nanoHash(seed + 1));
    float side = nanoHash(seed + 2) < 0.5f ? -1.0f : 1.0f;
    // Most leave the coat (the lower two thirds), a few the shoulders.
    float h = nanoHash(seed + 3);
    float up = h < 0.75f ? 8.0f + 52.0f * (h / 0.75f) : 62.0f + 30.0f * nanoHash(seed + 4);
    float edge = (up < 60.0f ? 10.0f + 0.12f * (60.0f - up) : 9.0f) +
                 5.0f * nanoHash(seed + 5);
    float drift = (6.0f + 18.0f * nanoHash(seed + 6)) * phase;
    float x = feet.x + side * (edge + drift);
    float y = feet.y - up - (3.0f + 12.0f * nanoHash(seed + 7)) * phase;
    float size = (2.0f + 2.5f * nanoHash(seed + 8)) * (1.0f - 0.45f * phase);
    float a = alpha * (1.0f - phase) * (fract(t * 9.0f + nanoHash(seed + 9)) > 0.12f ? 1.0f : 0.25f);
    naniteSquare(dl, x, y, size, naniteColour(nanoHash(seed + 10), 0.2f, a));
  }
}

// A swarm: pixels orbiting a centre `lift` above (x, y) on flattened rings,
// glitching in and out. `onEnemy`/`onAlly` tint it toward the coat's
// magenta (infection) or the screen's cyan (help).
// Kade's visor sits about this far above his feet on screen; his charge-up
// crouch lowers it.
constexpr float KADE_VISOR_HEIGHT = 64.0f;
constexpr float KADE_CROUCH_DROP = 12.0f;

// Focused Blast's charge (KADE RMB reference sheet): the visor brightens, a
// dashed ring builds around his head, and energy arcs crackle on his body
// from a third of the charge. At full charge the ring closes and flashes.
void drawKadeCharge(ImDrawList* dl, ImVec2 feet, float progress, float t) {
  const float p = std::max(0.0f, std::min(1.0f, progress));
  const bool full = p >= 0.995f;
  ImVec2 visor(feet.x, feet.y - KADE_VISOR_HEIGHT + KADE_CROUCH_DROP * p);

  // Visor glow: a soft halo that grows with the charge.
  for (int i = 3; i >= 1; --i) {
    float r = (6.0f + 10.0f * p) * static_cast<float>(i) * 0.6f;
    dl->AddCircleFilled(visor, r, themeColor(VfxTheme::Optic, 0.10f + 0.12f * p), 20);
  }
  dl->AddCircleFilled(visor, 3.0f + 3.0f * p, themeHot(VfxTheme::Optic, 0.5f + 0.5f * p), 12);

  // Dashed ring around the head, closing as the charge builds.
  const int dashes = 12;
  float radius = 18.0f + 10.0f * p;
  float spin = t * (1.5f + 3.0f * p);
  float gap = 0.55f * (1.0f - p) + 0.12f;          // dashes join up at full charge
  for (int i = 0; i < dashes; ++i) {
    if (static_cast<float>(i) > p * dashes + 0.5f && !full) break;   // builds round
    float a0 = spin + (2.0f * PI / dashes) * i;
    float a1 = a0 + (2.0f * PI / dashes) * (1.0f - gap);
    dl->PathArcTo(visor, radius, a0, a1, 6);
    dl->PathStroke(full ? themeHot(VfxTheme::Optic, 0.6f + 0.4f * std::sin(t * 30.0f))
                        : themeColor(VfxTheme::Optic, 0.45f + 0.5f * p),
                   0, 2.0f + 1.5f * p);
  }

  // Energy arcs on the body: short jagged sparks from a third of the charge.
  if (p > 0.33f) {
    int arcs = 1 + static_cast<int>((p - 0.33f) * 6.0f);
    for (int k = 0; k < arcs; ++k) {
      float seed = std::floor(t * 14.0f) * 1.37f + k * 2.11f;
      float ox = std::sin(seed * 3.1f) * 16.0f;
      float oy = -KADE_VISOR_HEIGHT * (0.25f + 0.5f * (0.5f + 0.5f * std::sin(seed * 1.7f)));
      ImVec2 a(feet.x + ox, feet.y + oy);
      ImVec2 pts[4];
      pts[0] = a;
      for (int s = 1; s < 4; ++s) {
        pts[s] = ImVec2(pts[s - 1].x + std::sin(seed * (5.0f + s)) * 6.0f,
                        pts[s - 1].y + std::cos(seed * (3.0f + s)) * 6.0f);
      }
      dl->AddPolyline(pts, 4, themeHot(VfxTheme::Optic, 0.8f), 0, 1.5f);
    }
  }
}

// Focused Blast's beam: from the visor, a tapered blue wedge with a white
// core and magenta streaks (KADE RMB reference: "Optic Beam, core +
// volumetric"). `halfWidth` is in world units.
// `lift0` / `lift1`: how high above the floor each end is on screen (the
// visor for a beam from Kade, a mirror's centre for one off a mirror).
constexpr float KADE_BEAM_LIFT = KADE_VISOR_HEIGHT - KADE_CROUCH_DROP;
// Kade's mirrors stand as tall as he does; beams cross at their middle.
constexpr float KADE_MIRROR_BEAM_LIFT = 36.0f;
template <typename ToScreen>
void drawOpticBeam(ImDrawList* dl, ToScreen toScreen, float x0, float y0,
                   float x1, float y1, float halfWidth, float t, float fade,
                   float lift0 = KADE_BEAM_LIFT, float lift1 = KADE_BEAM_LIFT) {
  float dx = x1 - x0, dy = y1 - y0;
  float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1.0f) return;
  float nx = -dy / len, ny = dx / len;
  auto at = [&](float u, float side) {
    ImVec2 g = toScreen(x0 + dx * u + nx * side, y0 + dy * u + ny * side);
    return ImVec2(g.x, g.y - (lift0 + (lift1 - lift0) * u));
  };
  // The beam snaps out fast, then thins as it fades.
  float grow = std::min(1.0f, t * 6.0f);
  float w = halfWidth * (0.55f + 0.45f * fade);
  auto wedge = [&](float scale, ImU32 colour) {
    ImVec2 q[4] = {at(0.0f, -w * scale * 0.25f), at(grow, -w * scale),
                   at(grow, w * scale), at(0.0f, w * scale * 0.25f)};
    dl->AddConvexPolyFilled(q, 4, colour);
  };
  wedge(1.25f, themeColor(VfxTheme::Optic, 0.25f * fade));
  wedge(0.85f, themeColor(VfxTheme::Optic, 0.60f * fade));
  wedge(0.40f, themeHot(VfxTheme::Optic, 0.90f * fade));
  wedge(0.14f, IM_COL32(255, 255, 255, static_cast<int>(240 * fade)));
  // Magenta streaks racing along the beam.
  for (int k = 0; k < 4; ++k) {
    float side = (k % 2 ? 1.0f : -1.0f) * w * (0.35f + 0.15f * k);
    float u0 = std::fmod(t * 3.0f + k * 0.23f, 1.0f) * grow;
    float u1 = std::min(grow, u0 + 0.18f);
    dl->AddLine(at(u0, side), at(u1, side),
                IM_COL32(255, 90, 210, static_cast<int>(210 * fade)), 2.5f);
  }
}

// Jeff (G3F-0427): Acquire Biomass's reach while RMB is held, drawn with
// biomass streaming inward; and the beam's heat as a gauge at his side,
// which blinks red and steams while he is venting.
void drawJeffGauges(ImDrawList* dl, const PlayerState& state, ImVec2 feet,
                    float t) {
  if (state.chargeSlot == 1) {
    float radius = Arena::JeffMode::harvestRadius(state.charge / 255.0f);
    dl->AddCircleFilled(feet, radius, themeColor(VfxTheme::Biomass, 0.07f), 48);
    dl->AddCircle(feet, radius, themeColor(VfxTheme::Biomass, 0.85f), 48, 2.5f);
    for (int i = 0; i < 12; i++) {
      float a = i * 0.5236f + t * 0.8f;
      float u = fract(t * 1.4f + i * 0.37f);  // 0 at the rim .. 1 at him
      float r = radius * (1.0f - u);
      ImVec2 p(feet.x + std::cos(a) * r, feet.y + std::sin(a) * r);
      dl->AddCircleFilled(p, 2.5f, themeHot(VfxTheme::Biomass, 0.8f * (1.0f - u * 0.5f)), 6);
    }
  }
  const float heat = state.resource / 255.0f;
  const bool venting = Arena::JeffMode::overheated(state.kitMode);
  if (heat <= 0.01f && !venting) return;
  const ImVec2 a(feet.x + 24.0f, feet.y - 58.0f), b(feet.x + 30.0f, feet.y - 10.0f);
  dl->AddRectFilled(a, b, IM_COL32(12, 12, 20, 200), 2.0f);
  float top = b.y - (b.y - a.y) * heat;
  ImU32 fill = venting ? (std::sin(t * 20.0f) > 0.0f ? IM_COL32(255, 60, 40, 255)
                                                     : IM_COL32(150, 30, 20, 255))
                       : rgba({1.0f, 0.75f - 0.45f * heat, 0.2f - 0.15f * heat}, 0.95f);
  dl->AddRectFilled(ImVec2(a.x + 1.0f, top), ImVec2(b.x - 1.0f, b.y - 1.0f), fill, 1.5f);
  dl->AddRect(a, b, IM_COL32(220, 214, 200, 200), 2.0f, 0, 1.0f);
  if (venting) {
    // Steam off the vents.
    for (int i = 0; i < 8; i++) {
      float h0 = nanoHash(state.playerId * 17u + i), h1 = nanoHash(state.playerId * 17u + i + 99u);
      float u = fract(t * (1.2f + h0) + h1);
      ImVec2 p(feet.x + (h0 - 0.5f) * 30.0f, feet.y - 50.0f - 40.0f * u);
      dl->AddCircleFilled(p, 4.0f + 9.0f * u,
                          IM_COL32(220, 225, 230, static_cast<int>(110 * (1.0f - u))), 12);
    }
  }
}

// Spent after Full Release: smoke curls off his shoulders and head, purple
// going grey as it rises (the human's reference for the end of his Q).
void drawKadeSmoke(ImDrawList* dl, ImVec2 feet, float t, uint32_t salt) {
  for (int i = 0; i < 22; i++) {
    uint32_t seed = static_cast<uint32_t>(i) * 7919u + salt * 131u;
    float h0 = nanoHash(seed), h1 = nanoHash(seed + 1), h2 = nanoHash(seed + 2);
    float u = fract(t * (0.40f + 0.30f * h0) + h1);  // 0 born .. 1 gone
    // Born on the hunched back and head, curling up and drifting.
    float baseX = feet.x + (h2 - 0.5f) * 34.0f;
    float baseY = feet.y - 38.0f - 16.0f * nanoHash(seed + 3);
    float curl = std::sin(t * 2.2f + h0 * 6.0f + u * 4.0f) * (4.0f + 14.0f * u);
    ImVec2 p(baseX + curl, baseY - 64.0f * u);
    float r = 4.0f + 11.0f * u;
    float a = (u < 0.12f ? u / 0.12f : 1.0f - (u - 0.12f) / 0.88f);
    // Violet near him, ash grey as it rises; a lighter core so it reads on
    // the dark floor.
    int red = static_cast<int>(175 - 25 * u), green = static_cast<int>(130 + 30 * u);
    int blue = static_cast<int>(220 - 50 * u);
    dl->AddCircleFilled(p, r, IM_COL32(red, green, blue, static_cast<int>(120 * a)),
                        14);
    dl->AddCircleFilled(ImVec2(p.x - r * 0.2f, p.y - r * 0.2f), r * 0.5f,
                        IM_COL32(230, 215, 245, static_cast<int>(90 * a)), 10);
  }
}

void drawNaniteCloud(ImDrawList* dl, ImVec2 at, float t, int count,
                     float radius, float lift, float bias, uint32_t salt) {
  ImVec2 centre(at.x, at.y - lift);
  dl->AddCircleFilled(centre, radius * 1.15f,
                      themeColor(VfxTheme::Nanite, 0.10f), 24);
  for (int i = 0; i < count; i++) {
    uint32_t seed = static_cast<uint32_t>(i) * 104729u + salt * 31u;
    float speed = (0.9f + 1.6f * nanoHash(seed)) * (nanoHash(seed + 1) < 0.5f ? -1.0f : 1.0f);
    float angle = t * speed + nanoHash(seed + 2) * 6.2832f;
    float r = radius * (0.25f + 0.75f * nanoHash(seed + 3));
    float bob = std::sin(t * 3.0f + nanoHash(seed + 4) * 6.0f) * radius * 0.25f;
    float x = centre.x + std::cos(angle) * r;
    float y = centre.y + std::sin(angle) * r * 0.6f + bob;
    bool lit = fract(t * 7.0f + nanoHash(seed + 5)) > 0.1f;
    float size = 2.0f + 2.5f * nanoHash(seed + 6);
    naniteSquare(dl, x, y, size,
                 naniteColour(nanoHash(seed + 7), bias, lit ? 0.95f : 0.3f));
  }
}

}  // namespace

const ArenaView::EnemySprite* ArenaView::enemySprite(Arena::EnemyKind kind) {
  loadManifest();
  // The boss borrows the largest Cerebrian body until it has its own art.
  // (Kinds drawn by a forge actor have no cut-out here and return nullptr.)
  const char* name = kind == Arena::EnemyKind::Boss
                         ? "Synaptogon"
                         : Arena::enemyDef(kind).name;
  auto it = enemySprites.find(name);
  return it == enemySprites.end() ? nullptr : &it->second;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

float ArenaView::liftAt(float worldX, float worldY) const {
  const TerrainGrid* grid = terrain();
  return grid ? grid->liftAt(worldX, worldY) : 0.0f;
}

const TerrainGrid* ArenaView::terrain() const {
  const CollisionSystem* solid = prediction->getCollisionSystem();
  return solid ? solid->getTerrain() : nullptr;
}

// Everything in the world stands on the ground: lifted by its height there
// (TerrainGrid). One place, so every sprite, ring and effect follows the
// hills without knowing about them.
ImVec2 ArenaView::toScreen(float worldX, float worldY) const {
  return ImVec2(worldX - camera->x + Config::Screen::WIDTH / 2.0f,
                worldY - liftAt(worldX, worldY) - camera->y +
                    Config::Screen::HEIGHT / 2.0f);
}

ImVec2 ArenaView::toZoomedScreen(float worldX, float worldY) const {
  return ImVec2(
      (worldX - camera->x) * camera->zoom + Config::Screen::WIDTH / 2.0f,
      (worldY - liftAt(worldX, worldY) - camera->y) * camera->zoom +
          Config::Screen::HEIGHT / 2.0f);
}

bool ArenaView::inView(ImVec2 layout, float left, float top, float right,
                       float bottom) const {
  // The layout is magnified around the screen centre, so the part of it
  // that ends up on screen is 1/zoom times the screen.
  const float cx = Config::Screen::WIDTH / 2.0f;
  const float cy = Config::Screen::HEIGHT / 2.0f;
  const float halfW = cx / camera->zoom;
  const float halfH = cy / camera->zoom;
  return layout.x >= cx - halfW - left && layout.x <= cx + halfW + right &&
         layout.y >= cy - halfH - top && layout.y <= cy + halfH + bottom;
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
  float now = arena->nowMs() / 1000.0f;
  float dt = std::clamp(now - timeSeconds, 0.0f, 0.1f);
  timeSeconds = now;

  if (renderIntro(dt)) return;
  updateFog();
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
  drawFog(dl);
  drawDistant(dl);
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
  // Your own melee hit landing kicks the camera, harder for heavier hits.
  {
    auto local = arena->players().find(localId());
    float held = local != arena->players().end() ? local->second.hitStopMs
                                                 : 0.0f;
    if (held > lastLocalHitStopMs + 1.0f) {
      effectFx.kick(std::min(5.0f, 0.8f + held / 70.0f));
    }
    lastLocalHitStopMs = held;
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
// Fog of war: shared crew sight
// ---------------------------------------------------------------------------

void ArenaView::collectSight(std::vector<FogOfWar::Source>& out) const {
  namespace C = Config::CameraView;
  uint32_t me = localId();
  const Player& local = prediction->getLocalPlayer();
  for (const auto& [id, view] : arena->players()) {
    const PlayerState& state = view.state;
    if (state.characterId == 0) continue;
    bool down = (state.flags & PlayerFlags::Dead) != 0 || state.health <= 0.0f;
    out.push_back({id == me ? local.x : view.x, id == me ? local.y : view.y,
                   down ? C::DOWNED_PLAYER_SIGHT : C::PLAYER_SIGHT});
  }
  if (arena->hasMatch()) {
    out.push_back({arena->match().shipX, arena->match().shipY, C::SHIP_SIGHT});
  }
  if (out.empty()) {
    // Before the match exists (connecting): at least see around yourself.
    out.push_back({local.x, local.y, C::PLAYER_SIGHT});
  }
}

void ArenaView::updateFog() {
  if (!fog) {
    fog = std::make_unique<FogOfWar>(camera->worldWidth, camera->worldHeight);
  }
  // A new match starts unexplored.
  bool inLobby = arena->phase() == Arena::MatchPhase::Lobby;
  if (inLobby && !fogWasInLobby) fog->reset();
  fogWasInLobby = inLobby;

  for (const ArenaClient::Reveal& scan : arena->takeReveals()) {
    fog->reveal(scan.x, scan.y, scan.radius);
  }
  std::vector<FogOfWar::Source> sources;
  collectSight(sources);
  fog->update(sources);
  if (!fogEnabled) return;

  GLint previous = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
  if (fogTexture == 0) {
    glGenTextures(1, &fogTexture);
    glBindTexture(GL_TEXTURE_2D, fogTexture);
    // Linear, so the coarse grid becomes a soft edge on screen.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fog->gridWidth(),
                 fog->gridHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 fog->texels().data());
  } else {
    glBindTexture(GL_TEXTURE_2D, fogTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fog->gridWidth(),
                    fog->gridHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                    fog->texels().data());
  }
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
}

void ArenaView::drawFog(ImDrawList* dl) {
  if (!fogEnabled || !fog || fogTexture == 0) return;
  // One quad over everything in view (plus room for the camera shake), in
  // world coordinates so it is magnified with the rest of the world.
  const float halfW = Config::Screen::WIDTH * 0.5f / camera->zoom + 64.0f;
  const float halfH = Config::Screen::HEIGHT * 0.5f / camera->zoom + 64.0f;
  const float x0 = camera->x - halfW, x1 = camera->x + halfW;
  const float y0 = camera->y - halfH, y1 = camera->y + halfH;
  auto uv = [this](float x, float y) {
    return ImVec2((x - fog->originX()) / fog->spanX(),
                  (y - fog->originY()) / fog->spanY());
  };
  // Matte: fog also puts out the glow of whatever it covers.
  beginMatte(dl);
  dl->AddImage((ImTextureID)(intptr_t)fogTexture, toScreen(x0, y0),
               toScreen(x1, y1), uv(x0, y0), uv(x1, y1));
  endMatte(dl);
}

bool ArenaView::inSight(float worldX, float worldY, float reach) const {
  return !fogEnabled || !fog ||
         fog->reveals(worldX, worldY, std::max(0.0f, reach));
}

bool ArenaView::wasSeen(float worldX, float worldY) const {
  return !fogEnabled || !fog || fog->explored(worldX, worldY) >= 0.5f;
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
  // Pools are placed where the ground is drawn: lifted by its height.
  auto add = [&](float x, float y, float radius, Rgb c, float strength) {
    if (strength <= 0.02f || radius < 1.0f) return;
    y -= liftAt(x, y);
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

  // What the crew can see is lifted out of the dark (fog of war): a wide,
  // faint, cool light over each source of sight.
  if (fogEnabled) {
    namespace C = Config::CameraView;
    std::vector<FogOfWar::Source> sight;
    collectSight(sight);
    for (const FogOfWar::Source& source : sight) {
      const float radius = source.radius * C::SIGHT_LIGHT_RADIUS_SCALE;
      if (std::abs(source.x - camera->x) > halfW + radius ||
          std::abs(source.y - camera->y) > halfH + radius) {
        continue;
      }
      out.push_back({source.x, source.y - liftAt(source.x, source.y), radius,
                     C::SIGHT_LIGHT_R,
                     C::SIGHT_LIGHT_G, C::SIGHT_LIGHT_B,
                     C::SIGHT_LIGHT_STRENGTH, 1.0f});
    }
  }

  // Telegraphs and one-shot effects.
  for (const ArenaClient::Fx& f : arena->effects()) {
    if (!inSight(f.x, f.y, f.value)) continue;  // hidden by the fog
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

  // What the fog hides must not light the ground either, or its pool would
  // give it away.
  for (const ArenaClient::Projectile& shot : arena->projectiles()) {
    if (shot.ageMs > shot.lifeMs) continue;
    if (!inSight(shot.x, shot.y)) continue;
    add(shot.x, shot.y, shot.radius * L::PROJECTILE_POOL_RADIUS_SCALE,
        themeRgb(shot.fromEnemy ? VfxTheme::Danger : shot.theme),
        L::PROJECTILE_POOL_STRENGTH);
  }

  for (const auto& [id, item] : prediction->getWorldItems()) {
    if (!Arena::PickupId::isInfusion(item.itemId)) continue;
    if (!wasSeen(item.x, item.y)) continue;
    add(item.x, item.y, L::PICKUP_POOL_RADIUS,
        themeRgb(genusTheme(Arena::PickupId::infusionGenus(item.itemId))),
        L::PICKUP_POOL_STRENGTH);
  }

  // Enemies stand in a faint pool of their genus colour, and it turns to
  // danger red as they wind up a hit.
  for (const auto& [id, view] : arena->enemies()) {
    if (view.dying) continue;
    if (!inSight(view.x, view.y)) continue;
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

  // The scenery's own lights: landmarks, neon, genus flora, the places.
  sceneryLights(add);
}

// ---------------------------------------------------------------------------
// Ground layer: zone, hazards, telegraphs, objectives
// ---------------------------------------------------------------------------

void ArenaView::drawBlockedEdges(ImDrawList* dl) {
  const TerrainGrid* grid = terrain();
  if (!grid || !grid->active()) return;
  const float W = Config::Screen::WIDTH, H = Config::Screen::HEIGHT;
  const float zoom = std::max(0.1f, camera->zoom);
  // The visible world, padded for lifted ground drawn from below the screen.
  const float halfW = W * 0.5f / zoom + 64.0f;
  const float halfH = H * 0.5f / zoom + 64.0f;
  const float maxLift =
      (TerrainGrid::LEVEL_COUNT - TerrainGrid::BASE_LEVEL) *
      TerrainGrid::LEVEL_PIXELS;
  float minCx = 1e9f, minCy = 1e9f, maxCx = -1e9f, maxCy = -1e9f;
  for (float sx : {-halfW, halfW}) {
    for (float sy : {-halfH, halfH + maxLift}) {
      float cx, cy;
      grid->worldToCorner(camera->x + sx, camera->y + sy, cx, cy);
      minCx = std::min(minCx, cx);
      maxCx = std::max(maxCx, cx);
      minCy = std::min(minCy, cy);
      maxCy = std::max(maxCy, cy);
    }
  }
  const int x0 = std::max(0, static_cast<int>(minCx) - 1);
  const int y0 = std::max(0, static_cast<int>(minCy) - 1);
  const int x1 = std::min(grid->width() - 1, static_cast<int>(maxCx) + 1);
  const int y1 = std::min(grid->height() - 1, static_cast<int>(maxCy) + 1);

  auto corner = [&](int cx, int cy) {
    float wx, wy;
    grid->cornerToWorld(static_cast<float>(cx), static_cast<float>(cy), wx, wy);
    wy -= grid->cornerLift(cx, cy);
    return ImVec2(wx - camera->x + W * 0.5f, wy - camera->y + H * 0.5f);
  };
  const ImU32 shadow = IM_COL32(0, 0, 0, 150);
  const ImU32 rock = IM_COL32(205, 196, 180, 190);
  const ImU32 foam = IM_COL32(140, 205, 235, 170);
  // Neighbour offsets and the corners of the shared edge, from TerrainGrid's
  // corner layout: top (tx,ty), right (tx+1,ty), bottom (tx+1,ty+1), left
  // (tx,ty+1).
  struct Side {
    int dx, dy, ax, ay, bx, by;
  };
  static const Side SIDES[4] = {
      {1, 0, 1, 0, 1, 1},   // lower right
      {-1, 0, 0, 0, 0, 1},  // upper left
      {0, 1, 0, 1, 1, 1},   // lower left
      {0, -1, 0, 0, 1, 0},  // upper right
  };
  for (int ty = y0; ty <= y1; ty++) {
    for (int tx = x0; tx <= x1; tx++) {
      if (grid->blockedTile(tx, ty)) continue;
      for (const Side& side : SIDES) {
        int nx = tx + side.dx, ny = ty + side.dy;
        if (!grid->inBounds(nx, ny) || !grid->blockedTile(nx, ny)) continue;
        ImVec2 a = corner(tx + side.ax, ty + side.ay);
        ImVec2 b = corner(tx + side.bx, ty + side.by);
        // Sea and lakes sit low (level 2 and under); anything else blocked
        // is a wall: a cliff, a sealed pocket, a building, a landmark.
        bool water = grid->levelOf(nx, ny) <= 3;
        dl->AddLine(a, b, shadow, 4.0f);
        dl->AddLine(a, b, water ? foam : rock, 1.75f);
      }
    }
  }
}

void ArenaView::drawGround(ImDrawList* dl) {
  drawBlockedEdges(dl);
  const MatchStatePacket& match = arena->match();
  Arena::MatchPhase phase = arena->phase();
  bool fighting = phase == Arena::MatchPhase::Round ||
                  phase == Arena::MatchPhase::BossFight;

  if (arena->hasMatch()) {
    ImVec2 ship = toScreen(match.shipX, match.shipY);

    // Around the ship: where you need to be when the round ends.
    if (phase == Arena::MatchPhase::Round) {
      float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 2.0f);
      dl->AddCircle(ship,
                    Arena::Tuning::SHIP_TIER_SAFE_RADIUS[std::min<int>(
                        match.shipTier, Arena::Tuning::SHIP_TIER_COUNT - 1)],
                    IM_COL32(120, 255, 170, static_cast<int>(40 + 30 * pulse)),
                    72, 2.0f);
    }

    // The finale's shield: everything outside is tinted and deadly. A round
    // has no zone (the crew roams and camps where night finds it).
    if (phase == Arena::MatchPhase::BossFight) {
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

  // Hauling something home: light up the drop-off ring around the ship and
  // point the way from the hauler's feet.
  {
    const MatchPlayerState* mine = arena->matchPlayer(localId());
    bool heavyLoad =
        mine && mine->scrapCarried >= Arena::Tuning::SCRAP_HEAVY_LOAD;
    bool hauling = heavyLoad;
    for (const auto& [id, objective] : prediction->getObjectives()) {
      if (objective.type == ObjectiveType::RecoverProbe &&
          objective.state == ObjectiveState::ReadyToDeposit) {
        hauling = true;
      }
    }
    if (fighting && hauling) {
      ImVec2 ship = toScreen(match.shipX, match.shipY);
      float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 4.0f);
      float r = Arena::Tuning::SHIP_DROP_RADIUS;
      dl->AddCircleFilled(ship, r, IM_COL32(255, 180, 70, 22 + 18 * pulse), 64);
      dl->AddCircle(ship, r, IM_COL32(255, 190, 90, 150 + 90 * pulse), 64,
                    4.0f);
      // Ticks sweeping inward: "bring it in here".
      for (int i = 0; i < 12; i++) {
        float a = i * PI / 6.0f + timeSeconds * 0.4f;
        float in = r - 14.0f - 10.0f * pulse;
        dl->AddLine(
            ImVec2(ship.x + std::cos(a) * (r + 10.0f),
                   ship.y + std::sin(a) * (r + 10.0f)),
            ImVec2(ship.x + std::cos(a) * in, ship.y + std::sin(a) * in),
            IM_COL32(255, 200, 110, 200), 3.0f);
      }
      float px, py;
      localPosition(px, py);
      float dx = match.shipX - px, dy = match.shipY - py;
      float d = std::sqrt(dx * dx + dy * dy);
      if (heavyLoad && d > r) {
        // A chevron at the feet, leaning toward home.
        ImVec2 feet = toScreen(px, py);
        float ux = dx / d, uy = dy / d;
        float lead = 46.0f + 6.0f * pulse;
        ImVec2 tip(feet.x + ux * (lead + 18.0f), feet.y + uy * (lead + 18.0f));
        ImVec2 base(feet.x + ux * lead, feet.y + uy * lead);
        ImVec2 left(base.x - uy * 14.0f, base.y + ux * 14.0f);
        ImVec2 right(base.x + uy * 14.0f, base.y - ux * 14.0f);
        dl->AddTriangleFilled(tip, left, right, IM_COL32(255, 190, 90, 230));
        dl->AddTriangle(tip, left, right, IM_COL32(40, 20, 10, 255), 2.0f);
      }
    }
  }

  // Objectives: the set piece (drawEntities) says what it is; the ring says
  // where to stand. Faint from afar, full once you are close or it is in
  // play, so the ground is not a field of identical reticles.
  float meX = 0.0f, meY = 0.0f;
  localPosition(meX, meY);
  for (const auto& [id, objective] : prediction->getObjectives()) {
    ImVec2 centre = toScreen(objective.x, objective.y);
    if (!inView(centre, 200, 200, 200, 200)) continue;
    bool near = std::hypot(objective.x - meX, objective.y - meY) <
                objective.radius + 260.0f;
    bool live = objective.state != ObjectiveState::Inactive;
    if (!near && !live) {
      ImU32 faint = objective.state == ObjectiveState::Completed
                        ? IM_COL32(110, 230, 130, 40)
                        : IM_COL32(255, 220, 90, 55);
      dl->AddCircle(centre, objective.radius, faint, 48, 1.5f);
      continue;
    }
    ImU32 colour = IM_COL32(255, 220, 90, 150);
    if (objective.state == ObjectiveState::InProgress) {
      colour = IM_COL32(90, 190, 255, 190);
    } else if (objective.state == ObjectiveState::ReadyToDeposit) {
      colour = IM_COL32(255, 160, 60, 190);
    } else if (objective.state == ObjectiveState::Completed) {
      colour = IM_COL32(110, 230, 130, 90);
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

    if (zone.variant == Arena::JeffMode::BiomassZone) {
      // Remains of a slain enemy: a glowing heap that pulses brighter as it
      // rots away, so a Jeff can tell what is still worth vacuuming.
      float left = std::max(0.0f, zone.lifeMs - zone.ageMs);
      float hurry = left < 3000.0f ? 0.5f + 0.5f * std::sin(timeSeconds * 14.0f) : 1.0f;
      dl->AddEllipseFilled(centre, ImVec2(zone.radius, zone.radius * 0.5f),
                           themeColor(zone.theme, 0.30f * alpha * hurry), 0.0f, 18);
      for (int i = 0; i < 4; i++) {
        float a = zone.id * 1.7f + i * 1.57f + timeSeconds * 0.6f;
        ImVec2 p(centre.x + std::cos(a) * zone.radius * 0.45f,
                 centre.y + std::sin(a) * zone.radius * 0.22f - 2.0f);
        dl->AddCircleFilled(p, 3.0f + (i % 2), themeHot(zone.theme, 0.75f * alpha * hurry), 8);
      }
      dl->AddEllipse(centre, ImVec2(zone.radius + 3.0f, (zone.radius + 3.0f) * 0.5f),
                     themeColor(zone.theme, 0.6f * alpha * hurry), 0.0f, 18, 1.5f);
    } else if (zone.variant == Arena::KadeMode::MirrorZone) {
      // Kade's mirror stands in the world (drawEntities); on the floor only
      // a faint blue footprint.
      dl->AddEllipse(centre, ImVec2(zone.radius, zone.radius * 0.45f),
                     themeColor(zone.theme, 0.35f * alpha), 0.0f, 24, 1.5f);
    } else if (zone.variant == 0) {
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
  // (Fog of war: shown as soon as any part of the attack reaches into sight.)
  for (const ArenaClient::Fx& f : arena->effects()) {
    if (!inSight(f.x, f.y, f.value)) continue;
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

namespace {

// Where each tier burns, as offsets from where that tier is drawn. `from`
// is the hull damage (0 = full health) at which a spot catches fire.
// `always` spots are the crash itself: they burn a little even at full
// health. Up a tier, nothing burns until the hull is hurt again.
struct WreckSpot {
  float x, y;
  float from;  // hull damage that sets it alight
  float size;
  bool always;
};
constexpr int WRECK_SPOTS_PER_TIER = 6;
constexpr WreckSpot
    WRECK_SPOTS[Arena::Tuning::SHIP_TIER_COUNT][WRECK_SPOTS_PER_TIER] = {
        {
            // The crash (Ship.png)
            {195.0f, -268.0f, 0.0f, 1.2f, true},  // upper nozzle
            {-215.0f, -70.0f, 0.0f, 0.9f, true},  // crushed cab, in the rubble
            {-70.0f, -150.0f, 0.25f, 1.0f, false},  // torn roof
            {205.0f, -205.0f, 0.45f, 1.1f, false},  // lower nozzle
            {95.0f, -175.0f, 0.65f, 1.0f, false},   // rear quarter
            {-150.0f, -110.0f, 0.8f, 0.9f, false},  // windows
        },
        {
            // Dug out and patched (ShipPatched.png)
            {180.0f, -200.0f, 0.1f, 1.1f, false},   // nozzles
            {-170.0f, 0.0f, 0.15f, 0.9f, false},    // cab
            {-10.0f, -40.0f, 0.3f, 1.0f, false},    // the patched breach
            {-30.0f, -110.0f, 0.5f, 1.0f, false},   // roof
            {110.0f, -100.0f, 0.65f, 1.0f, false},  // rear
            {-110.0f, -30.0f, 0.8f, 0.9f, false},   // windows
        },
        {
            // The fort (ShipFort.png)
            {210.0f, -220.0f, 0.15f, 1.1f, false},  // tarped nozzles
            {-200.0f, 0.0f, 0.2f, 0.9f, false},     // cab
            {-80.0f, -110.0f, 0.35f, 1.0f, false},  // roof deck
            {20.0f, -50.0f, 0.5f, 1.0f, false},     // flank
            {160.0f, -150.0f, 0.65f, 1.0f, false},  // rocket tarps
            {80.0f, 20.0f, 0.8f, 0.9f, false},      // stairs and sandbags
        },
};

// A column of smoke: born at `at`, rising and drifting with the wind,
// darkening and spreading as it goes. Ash grey with a lighter core so it
// reads against the dark ground.
void drawWreckSmoke(ImDrawList* dl, ImVec2 at, float t, float thickness,
                    float size, uint32_t salt) {
  int puffs = 6 + static_cast<int>(10.0f * thickness);
  for (int i = 0; i < puffs; i++) {
    uint32_t seed = static_cast<uint32_t>(i) * 2654435761u + salt * 97u;
    float h0 = nanoHash(seed), h1 = nanoHash(seed + 1), h2 = nanoHash(seed + 2);
    float u = fract(t * (0.16f + 0.10f * h0) + h1);  // 0 born .. 1 gone
    float rise = (90.0f + 150.0f * thickness) * size * u;
    float drift = 46.0f * u * u + std::sin(t * 0.9f + h2 * 6.0f) * 10.0f * u;
    ImVec2 p(at.x + (h2 - 0.5f) * 16.0f * size + drift, at.y - rise);
    float r = (10.0f + 30.0f * u) * size * (0.7f + 0.5f * thickness);
    float a = (u < 0.15f ? u / 0.15f : 1.0f - (u - 0.15f) / 0.85f) *
              (0.45f + 0.5f * thickness);
    int grey = static_cast<int>(78 - 30 * u);  // dark, to read on the hull
    dl->AddCircleFilled(
        p, r, IM_COL32(grey, grey - 4, grey - 2, static_cast<int>(215 * a)),
        16);
    dl->AddCircleFilled(ImVec2(p.x - r * 0.25f, p.y - r * 0.25f), r * 0.5f,
                        IM_COL32(170, 164, 158, static_cast<int>(90 * a)), 12);
  }
}

// Flames licking up from `at`, with embers spat upward. Glowing light, so
// it is the brightest thing on the hull (the world is dark, effects glow).
void drawWreckFire(ImDrawList* dl, ImVec2 at, float t, float heat, float size,
                   uint32_t salt) {
  size *= 1.8f;  // read at game zoom, over the pale hull
  // A warm glow on the hull under the flames.
  float flicker = 0.8f + 0.2f * std::sin(t * 17.0f + salt);
  dl->AddCircleFilled(at, 26.0f * size * (0.6f + 0.4f * heat),
                      IM_COL32(255, 120, 40, static_cast<int>(50 * flicker)),
                      20);
  int tongues = 5 + static_cast<int>(7.0f * heat);
  for (int i = 0; i < tongues; i++) {
    uint32_t seed = static_cast<uint32_t>(i) * 40503u + salt * 131u;
    float h0 = nanoHash(seed), h1 = nanoHash(seed + 1), h2 = nanoHash(seed + 2);
    float u = fract(t * (1.3f + 0.9f * h0) + h1);
    float height = (22.0f + 30.0f * heat) * size;
    float sway = std::sin(t * 9.0f + h2 * 6.0f) * 4.0f * u;
    ImVec2 p(at.x + (h2 - 0.5f) * 22.0f * size + sway, at.y - height * u);
    float r = (8.0f - 6.0f * u) * size * (0.8f + 0.4f * heat);
    // White-yellow at the root, orange, then deep red at the tips.
    int g = static_cast<int>(230 - 170 * u);
    int b = static_cast<int>(140 * (1.0f - u) * (1.0f - u));
    int a = static_cast<int>(235 * (1.0f - u * u));
    dl->AddCircleFilled(p, r, IM_COL32(255, g, b, a), 12);
  }
  int embers = static_cast<int>(4.0f + 6.0f * heat);
  for (int i = 0; i < embers; i++) {
    uint32_t seed = static_cast<uint32_t>(i) * 7841u + salt * 17u + 991u;
    float h0 = nanoHash(seed), h1 = nanoHash(seed + 1), h2 = nanoHash(seed + 2);
    float u = fract(t * (0.45f + 0.35f * h0) + h1);
    ImVec2 p(at.x + (h2 - 0.5f) * 40.0f * size + 30.0f * u * (h0 - 0.3f),
             at.y - (30.0f + 110.0f * h2) * size * u);
    int a = static_cast<int>(255 * (1.0f - u));
    dl->AddRectFilled(ImVec2(p.x - 1.5f, p.y - 1.5f),
                      ImVec2(p.x + 1.5f, p.y + 1.5f),
                      IM_COL32(255, static_cast<int>(200 - 120 * u), 60, a));
  }
}

}  // namespace

void ArenaView::drawShip(ImDrawList* dl) {
  if (!arena->hasMatch()) return;
  const MatchStatePacket& match = arena->match();
  // Wrecked and listing; tinted as it takes damage.
  float damage = 1.0f - match.shipHealth / std::max(1.0f, match.shipMaxHealth);
  damage = std::clamp(damage, 0.0f, 1.0f);
  ImU32 tint = IM_COL32(255, static_cast<int>(255 - 90 * damage),
                        static_cast<int>(255 - 110 * damage), 255);
  // The ship at its tier, at the heroes' scale (tools/sprite_forge/actors/
  // ship.py, ship_patched.py, ship_fort.py).
  int tier = std::min<int>(match.shipTier, Arena::Tuning::SHIP_TIER_COUNT - 1);
  const ShipTierLook& look = SHIP_TIER_LOOK[tier];
  const ActorSheet* sheet = actorFor(look.actor);
  const ActorAnim* still = sheet ? sheet->find("idle") : nullptr;
  if (sheet && still) {
    ImVec2 anchor = toScreen(match.shipX, match.shipY);
    ImVec2 centre(anchor.x + look.offsetX, anchor.y + look.offsetY);
    drawShadow(dl, centre, look.shadowRadius, 0.30f);
    drawActorFrame(dl, *sheet, *still, look.direction, 0.0f, centre, 1.0f, tint,
                   true, -1, 0.0f, true);
    drawShipWreckage(dl, centre, damage, tier, false);
    drawRoofTurrets(dl, anchor, tier, tint);
    drawShipWreckage(dl, centre, damage, tier, true);
    return;
  }
  ImVec2 base = toScreen(match.shipX, match.shipY + 34.0f);
  drawShadow(dl, ImVec2(base.x, base.y - 16.0f), 62.0f, 0.35f);
  if (shipTexture) {
    float scale = SHIP_DRAW_SIZE / shipTexture->getWidth();
    drawSprite(dl, shipTexture, 0, 0, shipTexture->getWidth(),
               shipTexture->getHeight(), base, scale, scale, -0.05f, false,
               tint);
  }
}

// Smoke and fire on the ship. The crash still burns at the cab and a
// nozzle; as the hull takes damage the smoke thickens and more of it
// catches fire, so the ship's health reads at a glance. A new tier is
// repaired in full, so its fires are out until raiders hurt it again. Fire is
// drawn first (on the hull), the smoke over it and over the roof turrets.
void ArenaView::drawShipWreckage(ImDrawList* dl, ImVec2 centre, float damage,
                                 int tier, bool smoke) {
  assert(damage >= 0.0f && damage <= 1.0f);
  assert(tier >= 0 && tier < Arena::Tuning::SHIP_TIER_COUNT);
  uint32_t salt = 1;
  for (const WreckSpot& spot : WRECK_SPOTS[tier]) {
    salt++;
    ImVec2 at(centre.x + spot.x, centre.y + spot.y);
    // The crash spots always burn a little; the rest catch as damage grows.
    float heat = std::clamp((damage - spot.from) / 0.25f, 0.0f, 1.0f);
    if (spot.always) heat = 0.6f + 0.4f * damage;
    if (heat <= 0.0f) continue;
    if (smoke) {
      drawWreckSmoke(dl, ImVec2(at.x, at.y - 10.0f), timeSeconds,
                     0.6f + 0.4f * damage, spot.size, salt);
    } else {
      drawWreckFire(dl, at, timeSeconds, heat, spot.size, salt);
    }
  }
}

// The roof turrets the crew stood up by repairing Derelict Turrets: a bolted
// foot and a head that swivels on it (16 headings from two baked sheets),
// kicking back and flashing at the muzzle when it fires.
void ArenaView::drawRoofTurrets(ImDrawList* dl, ImVec2 anchor, int tier,
                                ImU32 tint) {
  const MatchStatePacket& match = arena->match();
  if (match.roofTurrets == 0) return;
  const ActorSheet* base = actorFor("ShipTurretBase");
  const ActorSheet* head = actorFor("ShipTurret");
  const ActorSheet* tween = actorFor("ShipTurretTween");
  if (!base || !head || !tween) return;
  const ActorAnim* baseIdle = base->find("idle");
  const ActorAnim* headIdle = head->find("idle");
  const ActorAnim* tweenIdle = tween->find("idle");
  if (!baseIdle || !headIdle || !tweenIdle) return;
  namespace T = Arena::Tuning;
  for (int i = 0; i < T::ROOF_TURRET_COUNT; i++) {
    if (!(match.roofTurrets & (1u << i))) continue;
    ImVec2 foot(anchor.x + T::ROOF_TURRET_MOUNT_X[tier][i],
                anchor.y + T::ROOF_TURRET_MOUNT_Y[tier][i]);
    float aim = match.roofAim[i] / 256.0f * 2.0f * PI;  // cw from down
    float recoil = match.roofRecoil[i] / 255.0f;
    float dirX = std::sin(aim), dirY = std::cos(aim) * 0.5f;
    drawActorFrame(dl, *base, *baseIdle, 0, 0.0f, foot, 1.0f, tint, true, -1,
                   0.0f, false);
    int heading = static_cast<int>(std::lround(match.roofAim[i] / 16.0f)) % 16;
    bool between = heading % 2 == 1;
    ImVec2 kicked(foot.x - dirX * 7.0f * recoil, foot.y - dirY * 7.0f * recoil);
    drawActorFrame(dl, between ? *tween : *head,
                   between ? *tweenIdle : *headIdle, heading / 2, 0.0f, kicked,
                   1.0f, tint, true, -1, 0.0f, false);
    if (recoil > 0.55f) {
      float flash = (recoil - 0.55f) / 0.45f;
      ImVec2 muzzle(foot.x + dirX * T::ROOF_TURRET_BARREL_PX,
                    foot.y + dirY * T::ROOF_TURRET_BARREL_PX -
                        T::ROOF_TURRET_MUZZLE_LIFT_PX);
      dl->AddCircleFilled(muzzle, 16.0f * flash,
                          IM_COL32(255, 170, 60, static_cast<int>(120 * flash)),
                          16);
      dl->AddCircleFilled(
          muzzle, 7.0f * flash,
          IM_COL32(255, 245, 200, static_cast<int>(240 * flash)), 12);
    }
  }
}

bool ArenaView::drawDerelictTurret(ImDrawList* dl, ImVec2 foot, bool repaired,
                                   uint32_t salt) {
  const ActorSheet* base = actorFor("ShipTurretBase");
  const ActorSheet* head = actorFor("ShipTurret");
  if (!base || !head) return false;
  const ActorAnim* baseIdle = base->find("idle");
  const ActorAnim* headIdle = head->find("idle");
  if (!baseIdle || !headIdle) return false;
  // Out in the dark and dead: dimmer and cooler than the live roof guns.
  const ImU32 tint = IM_COL32(150, 146, 150, 255);
  drawActorFrame(dl, *base, *baseIdle, 0, 0.0f, foot, 1.0f, tint, true, -1,
                 0.0f, true);
  if (repaired) return true;  // the gun is on the ship's roof now
  // Knocked off true: sat low on the mount, facing down-screen and tipped
  // forward so the barrel points at the ground.
  constexpr int SLUMP_HEADING = 1;  // head sheet direction (of 8)
  ImVec2 seat(foot.x + 3.0f, foot.y + 5.0f);
  drawActorFrame(dl, *head, *headIdle, SLUMP_HEADING, 0.0f, seat, 1.0f, tint,
                 true, -1, 0.35f, false);
  // A thin column of smoke and the odd spark from the breech.
  ImVec2 breech(foot.x, foot.y - Arena::Tuning::ROOF_TURRET_MUZZLE_LIFT_PX);
  drawWreckSmoke(dl, breech, timeSeconds, 0.5f, 0.8f, salt);
  drawWreckFire(dl, ImVec2(breech.x + 4.0f, breech.y + 6.0f), timeSeconds,
                0.12f, 0.35f, salt);
  return true;
}

void ArenaView::drawRock(ImDrawList* dl, const CollisionShape& rock,
                         bool inReach, bool bonding) {
  const AABB& b = rock.aabb;
  assert(b.width > 0.0f && b.height > 0.0f);
  // Seeded by position so each rock keeps its own chipped outline.
  uint32_t seed = static_cast<uint32_t>(b.x * 7.0f + b.y * 13.0f) * 2654435761u;
  auto jitter = [&seed](float amount) {
    seed = seed * 1664525u + 1013904223u;
    return ((seed >> 8) & 0xFFFF) / 65535.0f * amount;
  };
  // The footprint as an octagon with chipped corners.
  const float cut = std::min(b.width, b.height) * 0.28f;
  ImVec2 foot[8] = {
      toScreen(b.x + cut + jitter(6), b.y),
      toScreen(b.x + b.width - cut - jitter(6), b.y),
      toScreen(b.x + b.width, b.y + cut + jitter(5)),
      toScreen(b.x + b.width, b.y + b.height - cut - jitter(5)),
      toScreen(b.x + b.width - cut - jitter(6), b.y + b.height),
      toScreen(b.x + cut + jitter(6), b.y + b.height),
      toScreen(b.x, b.y + b.height - cut - jitter(5)),
      toScreen(b.x, b.y + cut + jitter(5)),
  };
  // It stands up out of the ground: the top face is the footprint lifted,
  // drawn in toward the middle and broken, so it reads as an outcrop of
  // rock rather than a cut block.
  const float rise = 18.0f + b.height * 0.55f;
  ImVec2 mid(0.0f, 0.0f);
  for (const ImVec2& f : foot) {
    mid.x += f.x * 0.125f;
    mid.y += f.y * 0.125f;
  }
  ImVec2 top[8];
  for (int i = 0; i < 8; i++) {
    float in = 0.62f + jitter(0.18f);
    top[i] = ImVec2(mid.x + (foot[i].x - mid.x) * in,
                    mid.y + (foot[i].y - mid.y) * in - rise - jitter(16));
  }
  ImVec2 base = toScreen(b.x + b.width * 0.5f, b.y + b.height);
  drawShadow(dl, base, b.width * 0.62f, 0.45f);

  // The dark, quiet world: cool grey stone, the face in shadow, inked.
  const ImU32 face = IM_COL32(30, 32, 42, 255);
  const ImU32 cap = IM_COL32(52, 56, 70, 255);
  const ImU32 ink = IM_COL32(8, 8, 12, 255);
  // The south-facing sides (foot vertices 2..7 seen from above the south).
  for (int i = 2; i < 7; i++) {
    ImVec2 q[4] = {foot[i], foot[i + 1], top[i + 1], top[i]};
    dl->AddConvexPolyFilled(q, 4, face);
  }
  dl->AddConcavePolyFilled(top, 8, cap);
  // A cooler rim along the top's north edge: the light comes from above.
  dl->AddLine(top[7], top[0], IM_COL32(84, 92, 112, 255), 2.0f);
  dl->AddLine(top[0], top[1], IM_COL32(84, 92, 112, 255), 2.0f);
  dl->AddLine(top[1], top[2], IM_COL32(84, 92, 112, 255), 2.0f);
  // Ink: the outer silhouette and the edge where the top meets the face.
  ImVec2 outline[12] = {top[7],  top[0],  top[1],  top[2], top[3], foot[3],
                        foot[4], foot[5], foot[6], top[6], top[7], top[7]};
  dl->AddPolyline(outline, 11, ink, ImDrawFlags_Closed, 3.0f);
  for (int i = 3; i < 6; i++) dl->AddLine(top[i], top[i + 1], ink, 2.0f);

  if (inReach) {
    // Isaac's light finds the rocks his Lifeline can reach.
    float pulse = bonding ? 0.5f + 0.5f * std::sin(timeSeconds * 8.0f) : 0.0f;
    int alpha = bonding ? static_cast<int>(120 + 100 * pulse) : 70;
    dl->AddPolyline(top, 8, IM_COL32(255, 170, 60, alpha), ImDrawFlags_Closed,
                    bonding ? 2.5f : 1.5f);
  }
}

bool ArenaView::scrapSprite(const ActorSheet** sheet, const ActorAnim** idle) {
  *sheet = actorFor("Scrap");
  *idle = *sheet ? (*sheet)->find("idle") : nullptr;
  return *sheet && *idle;
}

void ArenaView::drawPickup(ImDrawList* dl, uint32_t itemId, float worldX,
                           float worldY) {
  float bob = std::sin(timeSeconds * 4.0f + worldX * 0.05f) * 3.0f;
  ImVec2 feet = toScreen(worldX, worldY);
  ImVec2 centre(feet.x, feet.y - 14.0f + bob);
  drawShadow(dl, feet, 9.0f, 0.3f);
  const ActorSheet* sheet = nullptr;
  const ActorAnim* idle = nullptr;

  if (Arena::PickupId::isInfusion(itemId)) {
    // An infusion is the run's most valuable drop: a pillar of its genus's
    // light marks it from across the screen, and the token carries the
    // genus's mark and the part's name (Clarity).
    Arena::Genus genus = Arena::PickupId::infusionGenus(itemId);
    VfxTheme theme = genusTheme(genus);
    float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 5.0f + worldY * 0.03f);
    const float pillar = 90.0f + 10.0f * pulse;
    dl->AddRectFilledMultiColor(ImVec2(centre.x - 5, centre.y - pillar),
                                ImVec2(centre.x + 5, centre.y),
                                themeColor(theme, 0.0f), themeColor(theme, 0.0f),
                                themeColor(theme, 0.45f), themeColor(theme, 0.45f));
    dl->AddRectFilledMultiColor(ImVec2(centre.x - 1.5f, centre.y - pillar),
                                ImVec2(centre.x + 1.5f, centre.y),
                                themeHot(theme, 0.0f), themeHot(theme, 0.0f),
                                themeHot(theme, 0.9f), themeHot(theme, 0.9f));
    dl->AddCircleFilled(centre, 18.0f + 4.0f * pulse, themeColor(theme, 0.22f), 24);
    dl->AddCircleFilled(centre, 13.0f, IM_COL32(14, 10, 18, 255), 24);
    dl->AddCircle(centre, 13.0f, themeColor(theme, 1.0f), 24, 2.5f);
    drawGenusGlyph(dl, centre, 8.5f, genus, themeHot(theme, 1.0f),
                   IM_COL32(14, 10, 18, 255));
    const Arena::InfusionPartInfo& part =
        Arena::partInfo(Arena::PickupId::infusionPart(itemId));
    text(dl, ImVec2(centre.x, centre.y - 32), themeHot(theme, 1.0f), 14.0f,
         part.name);
  } else if (Arena::PickupId::isScrap(itemId) && scrapSprite(&sheet, &idle)) {
    // The salvage pile (art/models/Scrap.glb): a bigger pile for more, its
    // facing varied by amount so a field of drops is not one stamp. It
    // sits on the ground, no bob: it is heavy.
    int amount = Arena::PickupId::scrapAmount(itemId);
    float scale =
        std::clamp(0.7f + 0.06f * static_cast<float>(amount), 0.75f, 1.25f);
    // Loot glows on the dark ground: a warm pool under the pile.
    float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 3.0f + worldX * 0.05f);
    dl->AddEllipseFilled(
        feet, ImVec2(30.0f * scale, 14.0f * scale),
        IM_COL32(255, 170, 70, static_cast<int>(40 + 30 * pulse)), 0.0f, 24);
    drawActorFrame(dl, *sheet, *idle, amount % 8, 0.0f, feet, scale,
                   IM_COL32_WHITE, true, -1, 0.0f, true);
    char label[24];
    std::snprintf(label, sizeof label, "SCRAP +%d", amount);
    text(dl, ImVec2(feet.x, feet.y - 52.0f * scale - 12.0f),
         IM_COL32(255, 215, 150, 255), 12.0f, label);
  } else if (Arena::PickupId::isScrap(itemId)) {
    // A bigger pile is a bigger plate; the label says how much.
    int amount = Arena::PickupId::scrapAmount(itemId);
    float w = std::min(16.0f, 7.0f + 2.0f * static_cast<float>(amount));
    float h = w * 0.75f;
    dl->AddRectFilled(ImVec2(centre.x - w, centre.y - h),
                      ImVec2(centre.x + w, centre.y + h),
                      IM_COL32(150, 150, 165, 255), 2.0f);
    dl->AddRect(ImVec2(centre.x - w, centre.y - h),
                ImVec2(centre.x + w, centre.y + h), IM_COL32(20, 14, 24, 255),
                2.0f, 0, 2.0f);
    dl->AddLine(ImVec2(centre.x - w * 0.55f, centre.y - h * 0.4f),
                ImVec2(centre.x + w * 0.55f, centre.y + h * 0.4f),
                IM_COL32(90, 90, 105, 255), 2.0f);
    char label[24];
    std::snprintf(label, sizeof label, "SCRAP +%d", amount);
    text(dl, ImVec2(centre.x, centre.y - h - 17.0f),
         IM_COL32(220, 220, 235, 255), 11.0f, label);
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

// ---- Gravon's fury -----------------------------------------------------------
// Fury of the Blade: the same dark violet that fills the host's face oozes
// out of him. Thick drops well up on his shoulders and arms, run down and
// fall; a few wisps lift off his back. Stateless, like the nanites.
static void drawFuryOoze(ImDrawList* dl, ImVec2 feet, float t, uint32_t seed,
                  float alpha) {
  const ImU32 core = IM_COL32(28, 6, 44, static_cast<int>(235 * alpha));
  for (int i = 0; i < 26; i++) {
    float h0 = nanoHash(seed + i * 7u);
    float h1 = nanoHash(seed + i * 7u + 1u);
    float h2 = nanoHash(seed + i * 7u + 2u);
    float life = 0.9f + 0.7f * h2;
    float u = fract(t / life + h0);  // 0 welling up .. 1 gone
    // Where on the body it wells up: shoulders, arms and back.
    float x = feet.x + (h1 - 0.5f) * 34.0f;
    float y0 = feet.y - 74.0f + 34.0f * h0 * h0;
    float fall = u < 0.35f ? 0.0f : (u - 0.35f) * (u - 0.35f) * 120.0f;
    float y = y0 + 10.0f * u + fall;
    float r = (1.6f + 2.2f * h2) * (u < 0.2f ? u / 0.2f : 1.0f - 0.5f * u);
    float a = alpha * (1.0f - u * u);
    ImU32 glow = themeColor(VfxTheme::Shadow, 0.45f * a);
    // The drip stretches as it runs.
    float stretch = 1.0f + 6.0f * std::max(0.0f, u - 0.3f);
    dl->AddCircleFilled(ImVec2(x, y - r * stretch * 0.5f), r * 1.9f, glow, 10);
    dl->AddRectFilled(ImVec2(x - r * 0.7f, y - r * stretch), ImVec2(x + r * 0.7f, y),
                      withAlpha(core, a), r);
    dl->AddCircleFilled(ImVec2(x, y), r, withAlpha(core, a), 10);
    dl->AddCircleFilled(ImVec2(x - r * 0.3f, y - r * 0.3f), r * 0.35f,
                        themeHot(VfxTheme::Shadow, 0.8f * a), 6);
  }
  // Wisps: violet smoke leaving his back.
  for (int i = 0; i < 8; i++) {
    float h0 = nanoHash(seed + 500u + i * 3u);
    float h1 = nanoHash(seed + 501u + i * 3u);
    float u = fract(t * 0.6f + h0);
    float x = feet.x + (h1 - 0.5f) * 26.0f + std::sin(t * 2.0f + i) * 5.0f * u;
    float y = feet.y - 60.0f - 46.0f * u;
    dl->AddCircleFilled(ImVec2(x, y), 3.0f + 7.0f * u,
                        themeColor(VfxTheme::Shadow, 0.28f * alpha * (1.0f - u)), 12);
  }
  // A dark pool where it lands.
  dl->AddEllipseFilled(feet, ImVec2(24.0f, 9.0f),
                       IM_COL32(24, 4, 38, static_cast<int>(120 * alpha)), 0.0f, 20);
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

  // Face the way you walk (W = N, W+A = NW, ...). Using an ability turns
  // you to the cursor for the cast; standing still keeps the last facing.
  AnimState& facing = playerAnim[state.playerId];
  bool acting = !dead && (view.castMs > 0.0f || charging ||
                         (state.characterId == Arena::CharacterId::Kade &&
                          state.kitMode == Arena::KadeMode::Releasing));
  float aimDX = aimX - worldX;
  float aimDY = aimY - worldY;
  // Refraction Wall faces the pinned mirror (its cursor turns the glass),
  // through the dash and the slap.
  if (!dead && state.characterId == Arena::CharacterId::Kade &&
      (state.chargeSlot == 0 || (view.castMs > 0.0f && view.castSlot == 0))) {
    aimDX = state.kitX - worldX;
    aimDY = state.kitY - worldY;
  }
  if (acting && std::abs(aimDX) + std::abs(aimDY) >= 1.0f) {
    facing.faceX = aimDX;
    facing.faceY = aimDY;
  } else if (moving) {
    facing.faceX = vx;
    facing.faceY = vy;
  }
  float faceX = facing.faceX;
  float faceY = facing.faceY;

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

  // Kade holding Refraction Wall is aiming a mirror, not charging: the
  // hologram shows it (drawKadeHologram).
  const bool kadePlacing = state.characterId == Arena::CharacterId::Kade &&
                           state.chargeSlot == 0;
  // Charge ring
  if (charging && !dead && !kadePlacing) {
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
  if (charging && !dead && state.characterId == Arena::CharacterId::Kade &&
      !kadePlacing) {
    drawKadeCharge(dl, feet, state.charge / 255.0f, timeSeconds);
  }
  const bool isJeff = state.characterId == Arena::CharacterId::Jeff;
  if (isJeff && !dead) drawJeffGauges(dl, state, feet, timeSeconds);
  // On treads Jeff is a different body: its own sheet.
  const ActorSheet* actor =
      definition ? actorFor(isJeff && Arena::JeffMode::treads(state.kitMode)
                                ? std::string("JeffTreads")
                                : definition->name)
                 : nullptr;

  // Gravon is the blade. When the host has fallen there is no body to draw:
  // the blade stands in the floor (or rides an ally, drawn with them) and
  // the host's body sinks away where he fell.
  const bool isGravon = state.characterId == Arena::CharacterId::Gravon;
  const uint8_t gravonForm =
      isGravon ? Arena::GravonMode::form(state.kitMode) : 0;
  if (isGravon && !dead && gravonForm != Arena::GravonMode::Host) {
    drawGravonFallen(dl, state);
    if (gravonForm == Arena::GravonMode::Exposed) {
      drawGravonBlade(dl, view, feet, -1);
    }
    return;
  }

  // Nyx is made of nanites: as a swarm she has no body to draw, and as a
  // body she is always fraying into pixels.
  const bool isNyx = state.characterId == Arena::CharacterId::Nyx;
  const uint8_t nyxForm = isNyx ? Arena::NyxMode::self(state.kitMode) : 0;
  float nyxFormSeconds = 0.0f;
  float bodyAlpha = 1.0f;
  if (isNyx) {
    using namespace Arena::NyxMode;
    const NyxView& nv = nyxViews[state.playerId];
    nyxFormSeconds = timeSeconds - nv.formSince;
    bool cloud = nyxForm == Flying || nyxForm == OnEnemy ||
                 nyxForm == OnAlly || nyxForm == Homing;
    if (cloud && !dead) {
      float bias = nyxForm == OnAlly ? 0.85f : (nyxForm == OnEnemy ? 0.0f : 0.4f);
      drawNaniteCloud(dl, feet, timeSeconds, 64, 26.0f, 40.0f, bias,
                      state.playerId);
      effectFx.drawOver(dl, state.playerId, false, feet, 64.0f, playerEffects);
      return;
    }
    // The body fades out as she dissolves, and in as she reforms.
    if (nyxForm == Dropping) {
      float u = std::min(1.0f, nyxFormSeconds / 0.29f);
      bodyAlpha = 1.0f - 0.8f * std::max(0.0f, (u - 0.45f) / 0.55f);
    } else if (nyxForm == Reforming) {
      bodyAlpha = 0.25f + 0.75f * std::min(1.0f, nyxFormSeconds / 0.18f);
    }
    tint = withAlpha(tint, bodyAlpha * ((tint >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f);
  }

  if (actor) {
    // Real animation: choose a clip from what the character is doing.
    AnimState& anim = playerAnim[state.playerId];
    anim.direction = directionFrame(faceX, faceY);
    if (view.castCount != anim.castCount) {
      anim.castCount = view.castCount;
      anim.clip = actionClip(definition->name, view.castSlot, view.castCount,
                             view.castAlt, view.castVariant);
      anim.startSeconds = view.actionSeconds;
    }
    const ActorAnim* clip = nullptr;
    float elapsed = 0.0f;
    if (introGetUp >= 0.0f && !dead) {
      // The crash intro: everyone gets up off the ground. Their fall,
      // played backwards.
      clip = actor->find("death");
      if (clip) {
        float length = static_cast<float>(clip->frames) / clip->fps;
        elapsed = (1.0f - introGetUp) * length * 0.999f;
        anim.clip.clear();
      }
    }
    if (!clip && dead) {
      clip = actor->find("death");
      elapsed = view.deadMs / 1000.0f;
    }
    if (!clip && charging && definition->name == "Nolan") {
      clip = actor->find("lotus_charge");
      elapsed = view.animTime;
    }
    if (!clip && charging && kadePlacing) {
      // Crouched, ready to dash and slap the mirror up.
      clip = actor->find("slap");
      elapsed = 0.0f;
    }
    if (!clip && charging && definition->name == "Kade") {
      // The charge-up follows how long the button has been held; at full
      // charge he holds, trembling.
      float progress = state.charge / 255.0f;
      const ActorAnim* up = actor->find("beam_charge");
      if (up && progress < 0.995f) {
        clip = up;
        elapsed = progress * static_cast<float>(up->frames - 1) / up->fps;
      } else {
        clip = actor->find("beam_hold");
        elapsed = view.animTime;
      }
    }
    if (!clip && !dead && state.characterId == Arena::CharacterId::Kade &&
        state.kitMode != Arena::KadeMode::Ready) {
      // Full Release: the beam held, trembling; then spent, heaving.
      clip = actor->find(state.kitMode == Arena::KadeMode::Releasing
                             ? "release_hold"
                             : "spent");
      elapsed = view.animTime;
      anim.clip.clear();
    }
    if (!clip && charging && isJeff) {
      clip = actor->find("harvest_charge");
      elapsed = view.animTime;
    }
    if (!clip && charging && definition->name == "Isaac") {
      clip = actor->find("lifeline_charge");
      elapsed = view.animTime;
    }
    if (!clip && isGravon) {
      // Ravager's Edge held: the coil. Just risen: the rise.
      float sinceRise = timeSeconds - gravonViews[state.playerId].riseAt;
      const ActorAnim* rise = actor->find("rise");
      if (charging) {
        clip = actor->find("ravager_charge");
        elapsed = view.animTime;
        anim.clip.clear();
      } else if (rise && sinceRise < static_cast<float>(rise->frames) / rise->fps) {
        clip = rise;
        elapsed = sinceRise;
        anim.clip.clear();
      }
    }
    const bool gravonDrawn =
        isGravon && (state.kitMode & Arena::GravonMode::DrawnBit) != 0;
    if (!clip && isNyx) {
      if (nyxForm == Arena::NyxMode::Dropping) {
        clip = actor->find("drop");
      } else if (nyxForm == Arena::NyxMode::Reforming) {
        clip = actor->find("lift");
      } else if (charging) {
        clip = actor->find("plague_charge");
      }
      elapsed = charging && !clip ? elapsed
                                  : (nyxForm == Arena::NyxMode::Body
                                         ? view.animTime
                                         : nyxFormSeconds);
      if (clip && nyxForm != Arena::NyxMode::Body) anim.clip.clear();
    }
    if (!clip && !anim.clip.empty()) {
      const ActorAnim* action = actor->find(anim.clip);
      float t = view.actionSeconds - anim.startSeconds;
      if (action && t < static_cast<float>(action->frames) / action->fps) {
        clip = action;
        elapsed = t;
      } else {
        anim.clip.clear();
      }
    }
    if (!clip && isGravon && !gravonDrawn) {
      // Not attacking for a while: he puts it away (an attack cuts it off).
      float since = timeSeconds - gravonViews[state.playerId].sheatheAt;
      const ActorAnim* sheathe = actor->find("sheathe");
      if (sheathe && since < static_cast<float>(sheathe->frames) / sheathe->fps) {
        clip = sheathe;
        elapsed = since;
      }
    }
    if (!clip && view.hitMs < 220.0f) {
      clip = actor->find(gravonDrawn ? "hit_drawn" : "hit");
      elapsed = view.hitMs / 1000.0f;
    }
    if (!clip && gravonDrawn) {
      // The blade out: braced over it standing, dragging it on the move.
      clip = actor->find(moving ? "drag_run" : "guard");
      elapsed = view.animTime;
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

  if (state.characterId == Arena::CharacterId::Kade && !dead &&
      state.kitMode == Arena::KadeMode::Spent) {
    drawKadeSmoke(dl, feet, timeSeconds, state.playerId);
  }

  if (isNyx && !dead) {
    float fray = nyxForm == Arena::NyxMode::Dropping
                     ? 1.0f + 2.5f * std::min(1.0f, nyxFormSeconds / 0.29f)
                 : nyxForm == Arena::NyxMode::Reforming ? 2.0f
                                                        : 1.0f;
    drawNaniteDissolve(dl, feet, timeSeconds + state.playerId * 3.7f, fray,
                       hidden ? 0.4f : 1.0f);
  }

  if (isGravon && !dead && moving &&
      (state.kitMode & Arena::GravonMode::DrawnBit)) {
    // The point scraping along the floor behind him: grit and sparks.
    float inv = 1.0f / std::max(speed, 1.0f);
    ImVec2 back = toScreen(worldX - vx * inv * 70.0f, worldY - vy * inv * 70.0f);
    ImVec2 tip(back.x, back.y);
    for (int i = 0; i < 9; i++) {
      float h0 = nanoHash(state.playerId * 31u + i * 5u);
      float h1 = nanoHash(state.playerId * 31u + i * 5u + 1u);
      float u = fract(timeSeconds * (1.6f + h0) + h1);
      float sx = tip.x + (h0 - 0.5f) * 10.0f - vx * inv * 18.0f * u;
      float sy = tip.y - 3.0f - 10.0f * u * (1.0f - u) * 4.0f * h1;
      if (i < 4) {
        dl->AddCircleFilled(ImVec2(sx, sy), 1.4f,
                            IM_COL32(255, 210, 170, static_cast<int>(230 * (1.0f - u))), 6);
      } else {
        dl->AddCircleFilled(ImVec2(sx, sy + 2.0f), 2.5f + 5.0f * u,
                            IM_COL32(120, 110, 120, static_cast<int>(70 * (1.0f - u))), 10);
      }
    }
  }
  if (isGravon && !dead && (state.kitMode & Arena::GravonMode::FuryBit)) {
    drawFuryOoze(dl, feet, timeSeconds, state.playerId * 131u,
                 hidden ? 0.4f : 1.0f);
  }
  if (isGravon && charging && (state.kitMode & Arena::GravonMode::ChargedBit)) {
    // Full charge: the blade's will flares (the strike will crit).
    float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 18.0f);
    dl->AddCircleFilled(ImVec2(feet.x, feet.y - 40.0f), 30.0f + 6.0f * pulse,
                        themeColor(VfxTheme::Blood, 0.16f + 0.10f * pulse), 24);
    dl->AddCircle(feet, 34.0f + 4.0f * pulse, themeHot(VfxTheme::Blood, 0.9f), 32,
                  3.0f);
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

void ArenaView::updateNyxView(const PlayerState& state) {
  NyxView& nv = nyxViews[state.playerId];
  uint8_t form = Arena::NyxMode::self(state.kitMode);
  float dt = nv.seen ? std::max(0.0f, timeSeconds - nv.lastSeconds) : 0.0f;
  nv.lastSeconds = timeSeconds;
  if (!nv.seen || form != nv.form) {
    if (form == Arena::NyxMode::Dropping) {
      // The CRT is set down facing the way she faced.
      auto it = playerAnim.find(state.playerId);
      nv.crtDirection = it == playerAnim.end() ? 0 : it->second.direction;
    }
    nv.form = form;
    nv.formSince = timeSeconds;
  }
  // State updates come in steps; ease toward them, but jump a long way.
  float dx = state.kitX - nv.swarmX, dy = state.kitY - nv.swarmY;
  if (!nv.seen || dx * dx + dy * dy > 500.0f * 500.0f) {
    nv.swarmX = state.kitX;
    nv.swarmY = state.kitY;
  } else {
    float k = std::min(1.0f, dt * 16.0f);
    nv.swarmX += dx * k;
    nv.swarmY += dy * k;
  }
  nv.seen = true;
}

void ArenaView::drawNyxSwarm(ImDrawList* dl, const PlayerState& state) {
  using namespace Arena::NyxMode;
  const NyxView& nv = nyxViews[state.playerId];
  ImVec2 at = toScreen(nv.swarmX, nv.swarmY);
  if (!inView(at, 80, 120, 80, 80)) return;
  uint8_t mode = swarm(state.kitMode);
  bool riding = mode == SwarmOnEnemy || mode == SwarmOnAlly;
  float bias = mode == SwarmOnAlly ? 0.85f : (mode == SwarmOnEnemy ? 0.0f : 0.4f);
  drawNaniteCloud(dl, at, timeSeconds, riding ? 40 : 30, riding ? 24.0f : 18.0f,
                  riding ? 34.0f : 20.0f, bias, state.playerId + 7u);
  // A thread of pixels back to Nyx, so you can always tell whose it is.
  ImVec2 home = toScreen(state.x, state.y - 50.0f);
  ImVec2 to(at.x, at.y - (riding ? 34.0f : 20.0f));
  for (int i = 0; i < 6; i++) {
    float u = fract(timeSeconds * 0.8f + i / 6.0f);
    naniteSquare(dl, home.x + (to.x - home.x) * u, home.y + (to.y - home.y) * u,
                 2.0f, naniteColour(nanoHash(i + 300u), bias, 0.55f * (1.0f - u)));
  }
}

void ArenaView::drawNyxCrt(ImDrawList* dl, const PlayerState& state) {
  ImVec2 feet = toScreen(state.propX, state.propY);
  if (!inView(feet, 80, 120, 80, 40)) return;
  const NyxView& nv = nyxViews[state.playerId];
  const ActorSheet* sheet = actorFor("NyxCRT");
  const ActorAnim* still = sheet ? sheet->find("idle") : nullptr;
  if (!sheet || !still) {
    // Unbaked: a plain box so the spot still reads.
    dl->AddRectFilled(ImVec2(feet.x - 12, feet.y - 22), ImVec2(feet.x + 12, feet.y),
                      IM_COL32(205, 198, 176, 255), 2.0f);
    dl->AddRectFilled(ImVec2(feet.x - 8, feet.y - 18), ImVec2(feet.x + 6, feet.y - 6),
                      IM_COL32(80, 210, 245, 255));
    return;
  }
  drawShadow(dl, feet, 13.0f);
  drawActorFrame(dl, *sheet, *still, nv.crtDirection, 0.0f, feet, 1.0f,
                 IM_COL32(255, 255, 255, 255), true, -1, 0.0f, true);
  // The screen glows: a soft cyan pool, flickering like an old tube.
  float flicker = 0.85f + 0.15f * std::sin(timeSeconds * 31.0f) *
                              std::sin(timeSeconds * 7.0f);
  dl->AddCircleFilled(ImVec2(feet.x, feet.y - 14.0f), 22.0f,
                      themeColor(VfxTheme::Wind, 0.12f * flicker), 20);
}

namespace {
// A short arrow on the floor out of a mirror's glass: where whatever strikes
// it will go.
void drawMirrorFacing(ImDrawList* dl, ImVec2 feet, float dirX, float dirY,
                      float alpha) {
  if (std::abs(dirX) + std::abs(dirY) < 0.01f) return;
  ImVec2 a(feet.x + dirX * 16.0f, feet.y + dirY * 16.0f);
  ImVec2 b(feet.x + dirX * 40.0f, feet.y + dirY * 40.0f);
  ImU32 c = themeHot(VfxTheme::Optic, alpha);
  dl->AddLine(a, b, c, 2.0f);
  float nx = -dirY, ny = dirX;
  ImVec2 head[3] = {ImVec2(b.x + dirX * 8.0f, b.y + dirY * 8.0f),
                    ImVec2(b.x + nx * 5.0f, b.y + ny * 5.0f),
                    ImVec2(b.x - nx * 5.0f, b.y - ny * 5.0f)};
  dl->AddConvexPolyFilled(head, 3, c);
}

// A Kade mirror's partners: the mirrors of the same owner, oldest first.
std::vector<const ArenaClient::Zone*> kadeMirrorsOf(
    const std::vector<ArenaClient::Zone>& zones, uint32_t ownerId) {
  std::vector<const ArenaClient::Zone*> out;
  for (const ArenaClient::Zone& zone : zones) {
    if (zone.variant == Arena::KadeMode::MirrorZone && zone.ownerId == ownerId) {
      out.push_back(&zone);
    }
  }
  std::sort(out.begin(), out.end(),
            [](const ArenaClient::Zone* a, const ArenaClient::Zone* b) {
              return a->id < b->id;
            });
  return out;
}
}  // namespace

bool ArenaView::isKadeMirrorAt(float x, float y) const {
  for (const ArenaClient::Zone& zone : arena->zones()) {
    if (zone.variant != Arena::KadeMode::MirrorZone) continue;
    if (std::abs(zone.x - x) < 2.0f && std::abs(zone.y - y) < 2.0f) return true;
  }
  return false;
}

void ArenaView::drawKadeMirror(ImDrawList* dl, const ArenaClient::Zone& zone) {
  ImVec2 feet = toScreen(zone.x, zone.y);
  if (!inView(feet, 80, 120, 80, 40)) return;
  // The glass faces the way Kade turned it: what strikes it leaves that way.
  int direction = std::abs(zone.dirX) + std::abs(zone.dirY) > 0.01f
                      ? directionFrame(zone.dirX, zone.dirY)
                      : 0;
  // At the limit, the mirror that goes next flickers while he aims another.
  float going = 1.0f;
  auto owner = arena->players().find(zone.ownerId);
  if (owner != arena->players().end() && owner->second.state.chargeSlot == 0) {
    auto chain = kadeMirrorsOf(arena->zones(), zone.ownerId);
    if (static_cast<int>(chain.size()) >= Arena::KadeMode::MAX_MIRRORS &&
        chain.front()->id == zone.id) {
      going = 0.45f + 0.35f * std::sin(timeSeconds * 14.0f);
    }
  }
  const ActorSheet* sheet = actorFor("KadeMirror");
  const ActorAnim* deploy = sheet ? sheet->find("deploy") : nullptr;
  const ActorAnim* still = sheet ? sheet->find("idle") : nullptr;
  float age = zone.ageMs / 1000.0f;
  float fadeIn = std::min(1.0f, zone.ageMs / 120.0f);
  if (!sheet || !still) {
    // Unbaked: a tall pane so the spot still reads.
    dl->AddRectFilled(ImVec2(feet.x - 10, feet.y - 74), ImVec2(feet.x + 10, feet.y),
                      themeColor(VfxTheme::Optic, 0.55f * fadeIn), 3.0f);
    dl->AddRect(ImVec2(feet.x - 10, feet.y - 74), ImVec2(feet.x + 10, feet.y),
                IM_COL32(220, 214, 200, static_cast<int>(255 * fadeIn)), 3.0f, 0,
                2.0f);
    return;
  }
  drawShadow(dl, feet, 14.0f);
  bool dropping =
      deploy && age < static_cast<float>(deploy->frames) / deploy->fps;
  drawActorFrame(dl, *sheet, dropping ? *deploy : *still, direction,
                 dropping ? age : 0.0f, feet, 1.0f,
                 withAlpha(IM_COL32(255, 255, 255, 255), fadeIn * going), true, -1,
                 0.0f, true);
  drawMirrorFacing(dl, feet, zone.dirX, zone.dirY, 0.35f * fadeIn);
  // The glass holds a little light, breathing.
  float glow = 0.75f + 0.25f * std::sin(timeSeconds * 2.4f + zone.id * 1.3f);
  dl->AddCircleFilled(ImVec2(feet.x, feet.y - KADE_MIRROR_BEAM_LIFT), 16.0f,
                      themeColor(VfxTheme::Optic, 0.10f * glow * fadeIn), 16);
}

void ArenaView::drawKadeHolograms(ImDrawList* dl) {
  uint32_t me = localId();
  for (const auto& [id, view] : arena->players()) {
    const PlayerState& state = view.state;
    if (state.characterId != Arena::CharacterId::Kade) continue;
    if (state.chargeSlot != 0 || state.health <= 0.0f) continue;
    float kx = view.x, ky = view.y, ax = state.aimX, ay = state.aimY;
    if (id == me) {
      // The local cursor and body, without the round trip.
      const Player& local = prediction->getLocalPlayer();
      kx = local.x;
      ky = local.y;
      ax = prediction->getAimX();
      ay = prediction->getAimY();
    }
    float fx, fy;
    Arena::KadeMode::mirrorFacing(state.kitX, state.kitY, ax, ay, kx, ky, fx, fy);
    ImVec2 feet = toScreen(state.kitX, state.kitY);
    float pulse = 0.8f + 0.2f * std::sin(timeSeconds * 6.0f);
    // The barrier it will join: a faint line to his newest mirror.
    auto chain = kadeMirrorsOf(arena->zones(), id);
    if (!chain.empty()) {
      size_t newest = chain.size() - 1;
      ImVec2 to = toScreen(chain[newest]->x, chain[newest]->y);
      ImVec2 from(feet.x, feet.y - KADE_MIRROR_BEAM_LIFT);
      to.y -= KADE_MIRROR_BEAM_LIFT;
      const int dashes = 14;
      for (int i = 0; i < dashes; i += 2) {
        float u0 = static_cast<float>(i) / dashes, u1 = (i + 1.0f) / dashes;
        dl->AddLine(ImVec2(from.x + (to.x - from.x) * u0, from.y + (to.y - from.y) * u0),
                    ImVec2(from.x + (to.x - from.x) * u1, from.y + (to.y - from.y) * u1),
                    themeColor(VfxTheme::Optic, 0.45f * pulse), 2.0f);
      }
    }
    // Its footprint and the way out of the glass.
    dl->AddEllipse(feet, ImVec2(26.0f, 11.0f), themeColor(VfxTheme::Optic, 0.6f * pulse),
                   0.0f, 24, 1.5f);
    drawMirrorFacing(dl, feet, fx, fy, 0.9f * pulse);
    const ActorSheet* sheet = actorFor("KadeMirror");
    const ActorAnim* still = sheet ? sheet->find("idle") : nullptr;
    if (sheet && still) {
      // The mirror itself, as blue light.
      drawActorFrame(dl, *sheet, *still, directionFrame(fx, fy), 0.0f, feet, 1.0f,
                     IM_COL32(110, 170, 255, static_cast<int>(120 * pulse)), true,
                     -1, 0.0f, false);
      // Scan lines across it, rising.
      for (int i = 0; i < 4; i++) {
        float u = fract(timeSeconds * 0.8f + i * 0.25f);
        float y = feet.y - 4.0f - 66.0f * u;
        dl->AddLine(ImVec2(feet.x - 14.0f, y), ImVec2(feet.x + 14.0f, y),
                    themeHot(VfxTheme::Optic, 0.35f * (1.0f - u)), 1.0f);
      }
    } else {
      dl->AddRectFilled(ImVec2(feet.x - 10, feet.y - 74), ImVec2(feet.x + 10, feet.y),
                        themeColor(VfxTheme::Optic, 0.30f * pulse), 3.0f);
    }
  }
}

void ArenaView::drawKadeBarriers(ImDrawList* dl) {
  // One chain per Kade whose body is up: a down Kade's barrier is off.
  for (const auto& [id, view] : arena->players()) {
    if (view.state.characterId != Arena::CharacterId::Kade) continue;
    if (view.state.health <= 0.0f) continue;
    auto chain = kadeMirrorsOf(arena->zones(), id);
    for (size_t i = 0; i + 1 < chain.size(); i++) {
      const ArenaClient::Zone& a = *chain[i];
      const ArenaClient::Zone& b = *chain[i + 1];
      // It joins once the newer mirror has landed.
      float on = std::min(1.0f, std::max(0.0f, (b.ageMs - 150.0f) / 200.0f));
      if (on <= 0.0f) continue;
      float shimmer = 0.75f + 0.25f * std::sin(timeSeconds * 9.0f + a.id);
      drawOpticBeam(dl, [&](float x, float y) { return toScreen(x, y); }, a.x,
                    a.y, b.x, b.y, 9.0f, timeSeconds * 0.35f + 1.0f,
                    0.55f * on * shimmer, KADE_MIRROR_BEAM_LIFT,
                    KADE_MIRROR_BEAM_LIFT);
    }
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
  if (!inView(feet, 260, 120, 260, 360)) return;

  bool elite = (view.state.flags & EnemyFlags::Elite) != 0;
  bool stunned = (view.state.flags & EnemyFlags::Stunned) != 0;
  bool confused = (view.state.flags & EnemyFlags::Confused) != 0;
  bool boss = kind == Arena::EnemyKind::Boss;
  // An elite with its own sheet is already drawn at elite size.
  bool ownEliteArt = elite && Arena::eliteHasOwnActor(kind);
  float scale = def.drawScale * (elite && !ownEliteArt ? 1.22f : 1.0f);
  // How much wider the body is on the ground than the kind's radius.
  float girth = (ownEliteArt ? Arena::eliteTuning(kind).size : 1.0f) * scale /
                def.drawScale;
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
    if (view.hitStopMs > 0.0f) {
      // Held by a hit: a hard shudder for as long as the hold lasts.
      offsetX += std::sin(view.hitStopMs * 0.9f) * 6.0f;
      offsetY += std::cos(view.hitStopMs * 0.7f) * 2.0f;
    }
    if (view.hitFlashMs > 0.0f) {
      tint = IM_COL32(255, 110, 110, 255);
      scaleX *= 1.06f;
    }
  }

  if (!view.dying) drawShadow(dl, feet, def.radius * girth * 1.05f);

  static const std::vector<EffectInstance> noEnemyEffects;
  const std::vector<EffectInstance>& enemyEffects =
      effects && !view.dying ? effects->getEffects(view.state.id, true)
                             : noEnemyEffects;
  float bodyRadius = def.radius * girth;
  effectFx.drawUnder(dl, view.state.id, true, feet, bodyRadius, enemyEffects);
  tint = multiplyColor(tint, effectFx.bodyTint(enemyEffects));
  if (elite && !view.dying) {
    dl->AddCircle(feet, def.radius * girth * 1.5f, IM_COL32(255, 210, 80, 200),
                  28, 2.5f);
  }

  ImVec2 at(feet.x + offsetX, feet.y + offsetY);
  const ActorSheet* actor =
      boss ? nullptr : actorFor(Arena::enemyActorName(kind, elite));
  // Grappled: in El Presidente's hands. Pinned to the carry socket his
  // animation exports, so the foe rises with his leaps, whirls with the
  // Látigo and is driven down by the powerbomb.
  bool lifted = (view.state.flags & EnemyFlags::Grappled) != 0;
  float liftRotation = 0.0f;
  const CarryPoint* carrier = lifted ? carrierNear(view.x, view.y) : nullptr;
  if (carrier) {
    // Rolls are authored for an upright body (90 = lying across his hands).
    // The old aliens are long, low creatures, so a quarter turn more puts
    // them on their backs, legs up. Thugs stand upright and need none.
    float extraRoll = Arena::hasThugRoster(def.genus) ? 0.0f : 90.0f;
    liftRotation = (carrier->rollDeg + extraRoll) * (PI / 180.0f) *
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
  // Knocked up: tumbling in the air above its shadow, which stays on the
  // ground where it will land.
  bool airborne = !lifted && !view.dying && view.air > 0.0f;
  if (airborne) {
    float h = 70.0f * view.air * (2.0f - view.air);
    at.y -= h;
    feet.y -= h;
    liftRotation = (view.faceLeft ? -0.6f : 0.6f) * view.air;
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
      if (lifted || airborne) {
        clip = actor->find("hit");
        forced = 1;
      }
      if (clip) {
        drawActorFrame(dl, *actor, *clip, anim.direction, elapsed, feet, scale,
                       actorTint, true, forced, liftRotation,
                       !view.dying && !lifted && !airborne);
      }
    }
  } else if (sprite && sprite->texture) {
    if (!view.dying && !lifted && !airborne) {
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

void ArenaView::updateGravonView(const PlayerState& state) {
  GravonView& gv = gravonViews[state.playerId];
  uint8_t form = Arena::GravonMode::form(state.kitMode);
  bool drawn = (state.kitMode & Arena::GravonMode::DrawnBit) != 0;
  if (!gv.seen) {
    gv.form = form;
    gv.drawn = drawn;
    gv.seen = true;
    return;
  }
  if (gv.drawn && !drawn && form == Arena::GravonMode::Host &&
      gv.form == Arena::GravonMode::Host && state.health > 0.0f) {
    gv.sheatheAt = timeSeconds;  // put it away
  }
  gv.drawn = drawn;
  if (form == gv.form) return;
  bool wasHost = gv.form == Arena::GravonMode::Host;
  bool isHost = form == Arena::GravonMode::Host;
  if (wasHost && !isHost) {
    // The host fell: remember where, and which way he faced.
    gv.fallX = state.propX;
    gv.fallY = state.propY;
    auto it = playerAnim.find(state.playerId);
    gv.fallDirection = it == playerAnim.end() ? 0 : it->second.direction;
    gv.formSince = timeSeconds;
  } else if (!wasHost && isHost && state.health > 0.0f) {
    gv.riseAt = timeSeconds;  // the blade raised him
  }
  gv.form = form;
}

void ArenaView::drawGravonFallen(ImDrawList* dl, const PlayerState& state) {
  const GravonView& gv = gravonViews[state.playerId];
  float t = timeSeconds - gv.formSince;
  const float FADE_FROM = 0.7f, GONE = 1.8f;
  if (t >= GONE) return;
  ImVec2 at = toScreen(gv.fallX, gv.fallY);
  if (!inView(at, 120, 160, 120, 60)) return;
  const ActorSheet* sheet = actorFor("Gravon");
  const ActorAnim* death = sheet ? sheet->find("death") : nullptr;
  if (!sheet || !death) return;
  float alpha = t < FADE_FROM ? 1.0f : 1.0f - (t - FADE_FROM) / (GONE - FADE_FROM);
  drawActorFrame(dl, *sheet, *death, gv.fallDirection, t, at, 1.0f,
                 IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), true);
  // The body goes to violet smoke: what is left of the host leaves him.
  if (t > FADE_FROM) {
    drawFuryOoze(dl, at, timeSeconds, state.playerId * 977u, alpha);
  }
}

void ArenaView::drawGravonBlade(ImDrawList* dl, const ArenaClient::PlayerView& view,
                                ImVec2 feet, int direction) {
  const PlayerState& state = view.state;
  if (!inView(feet, 120, 200, 120, 60)) return;
  bool onBack = direction >= 0;
  ImU32 tint = IM_COL32(255, 255, 255, 255);
  if (view.hitFlashMs > 0.0f) tint = IM_COL32(255, 120, 120, 255);

  if (!onBack) {
    // How close the host is to rising: a ring that fills, with the blade's
    // pulse. Everyone can see where to go and how long is left.
    float progress = state.resource / 255.0f;
    float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 4.0f);
    dl->AddEllipseFilled(feet, ImVec2(30.0f, 12.0f),
                         themeColor(VfxTheme::Shadow, 0.14f + 0.08f * pulse), 0.0f, 24);
    dl->AddEllipse(feet, ImVec2(34.0f, 13.5f), themeColor(VfxTheme::Shadow, 0.35f),
                   0.0f, 32, 2.0f);
    const int segments = 40;
    for (int i = 0; i < segments; i++) {
      float a0 = -PI * 0.5f + 2.0f * PI * i / segments;
      float a1 = -PI * 0.5f + 2.0f * PI * (i + 1) / segments;
      if (static_cast<float>(i) / segments >= progress) break;
      dl->AddLine(ImVec2(feet.x + std::cos(a0) * 34.0f, feet.y + std::sin(a0) * 13.5f),
                  ImVec2(feet.x + std::cos(a1) * 34.0f, feet.y + std::sin(a1) * 13.5f),
                  themeHot(VfxTheme::Shadow, 0.95f), 3.0f);
    }
  }

  const ActorSheet* sheet = actorFor("GravonBlade");
  const ActorAnim* clip = sheet ? sheet->find(onBack ? "back" : "floor") : nullptr;
  if (!sheet || !clip) {
    // Unbaked: a plain slab so the spot still reads.
    float top = onBack ? 90.0f : 110.0f;
    dl->AddLine(ImVec2(feet.x - 6, feet.y - (onBack ? 20.0f : 0.0f)),
                ImVec2(feet.x + 10, feet.y - top), IM_COL32(150, 40, 110, 255), 9.0f);
    return;
  }
  if (!onBack) drawShadow(dl, feet, 10.0f);
  drawActorFrame(dl, *sheet, *clip, onBack ? direction : 0, 0.0f, feet, 1.0f, tint,
                 true, -1, 0.0f, !onBack);
  if (!onBack) {
    // The red eye in the crossguard watches.
    float glow = 0.6f + 0.4f * std::sin(timeSeconds * 3.0f);
    dl->AddCircleFilled(ImVec2(feet.x + 6.0f, feet.y - 88.0f), 7.0f,
                        themeColor(VfxTheme::Blood, 0.25f * glow), 12);
  }
}

void ArenaView::drawEntities(ImDrawList* dl) {
  struct Item {
    float depth;
    int kind;  // 0 player, 1 enemy, 2 pickup, 3 ship, 4 Nyx's CRT, 5 swarm,
               // 6 Gravon's blade on a carrier's back, 7 a terrain rock,
               // 8 Kade's mirror, 9 a prop, 10 an objective's set piece
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
    if (view.state.characterId == Arena::CharacterId::Nyx) {
      updateNyxView(view.state);
      using namespace Arena::NyxMode;
      uint8_t form = self(view.state.kitMode);
      // The CRT on the floor while she is a swarm.
      if (form != Body && form != Dropping && form != Reforming) {
        items.push_back({view.state.propY, 4, &view, id, view.state.propX,
                         view.state.propY});
      }
      // Her swarm, drawn over whoever it rides.
      if (swarm(view.state.kitMode) != SwarmHome) {
        const NyxView& nv = nyxViews[id];
        items.push_back({nv.swarmY + 30.0f, 5, &view, id, nv.swarmX, nv.swarmY});
      }
    }
    if (view.state.characterId == Arena::CharacterId::Gravon) {
      updateGravonView(view.state);
      if (Arena::GravonMode::form(view.state.kitMode) == Arena::GravonMode::Carried) {
        // On the carrier's back: in front of them when they face away.
        uint32_t carrierId = static_cast<uint32_t>(view.state.kitX);
        auto anim = playerAnim.find(carrierId);
        int dir = anim == playerAnim.end() ? 0 : anim->second.direction;
        bool facingAway = dir >= 3 && dir <= 5;
        float cx = x, cy = y;
        if (carrierId == me) {
          cx = local.x;
          cy = local.y;
        } else {
          auto carrier = arena->players().find(carrierId);
          if (carrier != arena->players().end()) {
            cx = carrier->second.x;
            cy = carrier->second.y;
          }
        }
        items.push_back({cy + (facingAway ? 0.5f : -0.5f), 6, &view, carrierId,
                         cx, cy});
      }
    }
  }
  for (const auto& [id, view] : arena->enemies()) {
    if (!inSight(view.x, view.y)) continue;  // fog of war
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
  for (const ArenaClient::Zone& zone : arena->zones()) {
    if (zone.variant != Arena::KadeMode::MirrorZone) continue;
    items.push_back({zone.y, 8, &zone, zone.id, zone.x, zone.y});
  }
  for (const auto& [id, item] : prediction->getWorldItems()) {
    if (!wasSeen(item.x, item.y)) continue;
    items.push_back({item.y - 1.0f, 2, nullptr, item.itemId, item.x, item.y});
  }
  if (arena->hasMatch()) {
    items.push_back({arena->match().shipY, 3, nullptr, 0, 0, 0});
  }
  // The planet's scenery and the objectives' set pieces stand in the world
  // too (ArenaWorld.cpp). Only what is near the view is sorted.
  {
    const float reachX = Config::Screen::WIDTH * 0.5f / camera->zoom + 500.0f;
    const float reachY = Config::Screen::HEIGHT * 0.5f / camera->zoom + 700.0f;
    if (props) {
      for (size_t i = 0; i < props->size(); i++) {
        const MapProp& prop = (*props)[i];
        if (std::abs(prop.x - camera->x) > reachX ||
            std::abs(prop.y - camera->y) > reachY) {
          continue;
        }
        if (prop.kind == "skyline") continue;  // drawn in the distance
        items.push_back({prop.y, 9, &prop, static_cast<uint32_t>(i), prop.x,
                         prop.y});
      }
    }
    for (const auto& [id, objective] : prediction->getObjectives()) {
      if (std::abs(objective.x - camera->x) > reachX ||
          std::abs(objective.y - camera->y) > reachY) {
        continue;
      }
      items.push_back({objective.y, 10, &objective, id, objective.x,
                       objective.y});
    }
  }
  // Terrain stands in the world like anything else: what is south of a rock
  // is drawn over it, what is north is hidden behind it.
  if (const CollisionSystem* solid = prediction->getCollisionSystem()) {
    for (const CollisionShape& shape : solid->getShapes()) {
      if (!CollisionSystem::isTerrain(shape)) continue;
      items.push_back({shape.aabb.y + shape.aabb.height, 7, &shape, 0,
                       shape.aabb.x, shape.aabb.y});
    }
  }

  // The hull is a long diagonal, so one depth cannot place everything
  // around it: anything standing south of the hull's footprint (the current
  // tier's "hull" boxes under it) is in front of the whole ship, even north
  // of its centre, and anything north of it is behind, even south of it.
  if (arena->hasMatch()) {
    if (const CollisionSystem* solid = prediction->getCollisionSystem()) {
      float shipY = arena->match().shipY;
      for (Item& item : items) {
        if (item.kind == 3 || item.kind == 7 || item.kind == 9) continue;
        for (const CollisionShape& shape : solid->getShapes()) {
          if (!solid->isActiveHull(shape)) continue;
          const AABB& b = shape.aabb;
          if (item.x < b.x || item.x > b.x + b.width) continue;
          // Only things on the wrong side of the ship's depth move; both
          // keep their order among themselves.
          if (item.y >= b.y + b.height && item.depth <= shipY) {
            item.depth =
                shipY + 1.0f + (item.depth - (shipY - 1000.0f)) * 0.001f;
          } else if (item.y <= b.y && item.depth >= shipY) {
            item.depth = shipY - 1.0f - (shipY + 1000.0f - item.depth) * 0.001f;
          }
          break;
        }
      }
    }
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
      case 4:
        drawNyxCrt(dl, static_cast<const ArenaClient::PlayerView*>(item.ptr)->state);
        break;
      case 5:
        drawNyxSwarm(dl, static_cast<const ArenaClient::PlayerView*>(item.ptr)->state);
        break;
      case 8:
        drawKadeMirror(dl, *static_cast<const ArenaClient::Zone*>(item.ptr));
        break;
      case 9:
        drawProp(dl, *static_cast<const MapProp*>(item.ptr), item.id);
        break;
      case 10:
        drawObjectiveSite(dl, *static_cast<const ClientObjective*>(item.ptr),
                          item.id);
        break;
      case 7: {
        const auto& rock = *static_cast<const CollisionShape*>(item.ptr);
        // Isaac sees which rocks his Lifeline can reach, and the one he holds.
        bool isaac = local.characterId == Arena::CharacterId::Isaac;
        float fx, fy;
        CollisionSystem::closestPoint(rock.aabb, local.x, local.y, fx, fy);
        bool inReach = isaac && std::hypot(fx - local.x, fy - local.y) <=
                                    Arena::Tuning::LIFELINE_RANGE;
        bool bonding = false;
        if (inReach) {
          auto mine = arena->players().find(me);
          bonding = mine != arena->players().end() &&
                    mine->second.state.chargeSlot == 1;
        }
        drawRock(dl, rock, inReach, bonding);
        break;
      }
      case 6: {
        auto anim = playerAnim.find(item.id);
        int dir = anim == playerAnim.end() ? 0 : anim->second.direction;
        drawGravonBlade(dl, *static_cast<const ArenaClient::PlayerView*>(item.ptr),
                        toScreen(item.x, item.y), dir);
        break;
      }
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
  drawKadeBarriers(dl);
  drawKadeHolograms(dl);
  effectFx.drawVerbs(
      dl, [this](float x, float y) { return toScreen(x, y); },
      [this](uint32_t id, bool isEnemy, float& x, float& y) {
        if (isEnemy) {
          auto it = arena->enemies().find(id);
          if (it == arena->enemies().end()) return false;
          x = it->second.x;
          y = it->second.y;
          return inSight(x, y);
        }
        if (id == localId()) return localPosition(x, y);
        auto it = arena->players().find(id);
        if (it == arena->players().end()) return false;
        x = it->second.x;
        y = it->second.y;
        return true;
      });
  for (const ArenaClient::Fx& f : arena->effects()) {
    if (!inSight(f.x, f.y, f.value)) continue;  // hidden by the fog
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
        // param 1: from the swinger's right to their left.
        float way = (f.param & 1) ? -1.0f : 1.0f;
        float start = angle - way * half;
        float head = start + way * 2.0f * half * sweep;
        // The trail behind the head, never past where the swing began.
        auto trailFrom = [&](float length) {
          return way > 0.0f ? std::max(start, head - length)
                            : std::min(start, head + length);
        };
        float radius = f.value * (0.86f + 0.14f * sweep);
        int segments = std::max(8, static_cast<int>(half * radius / 8.0f));
        // Echoes: wider, fainter, slightly smaller.
        for (int i = 3; i >= 1; i--) {
          float r = radius * (1.0f - 0.09f * i);
          float tail = trailFrom(half * (0.5f + 0.35f * i));
          dl->PathArcTo(origin, r, tail, head, segments);
          dl->PathStroke(themeColor(f.theme, (0.55f - 0.13f * i) * fade), 0,
                         radius * 0.11f * (1.0f + 0.5f * i));
        }
        float tail = trailFrom(half * 0.9f);
        dl->PathArcTo(origin, radius, tail, head, segments);
        dl->PathStroke(themeHot(f.theme, 0.85f * fade), 0, radius * 0.13f);
        dl->PathArcTo(origin, radius, trailFrom(half * 0.45f), head, segments);
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
        if (f.theme == VfxTheme::Optic) {
          drawOpticBeam(dl, [&](float x, float y) { return toScreen(x, y); },
                        f.x, f.y, f.x2, f.y2, f.value, t, fade,
                        isKadeMirrorAt(f.x, f.y) ? KADE_MIRROR_BEAM_LIFT
                                                 : KADE_BEAM_LIFT,
                        isKadeMirrorAt(f.x2, f.y2) ? KADE_MIRROR_BEAM_LIFT
                                                   : KADE_BEAM_LIFT);
          break;
        }
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
    if (!inSight(shot.x, shot.y)) continue;
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

  // An infusion just picked up: its genus's ring bursts out from the hero.
  if (timeSeconds < pickupFlashUntil) {
    float t = 1.0f - (pickupFlashUntil - timeSeconds) / 0.7f;
    VfxTheme theme = genusTheme(pickupFlashGenus);
    ImVec2 at = toZoomedScreen(local.x, local.y);
    at.y -= 30.0f * camera->zoom;
    dl->AddCircle(at, (20.0f + 70.0f * t) * camera->zoom,
                  themeHot(theme, 1.0f - t), 32, 3.0f);
    drawGenusGlyph(dl, ImVec2(at.x, at.y - 40.0f * t * camera->zoom),
                   14.0f * camera->zoom, pickupFlashGenus,
                   themeHot(theme, 1.0f - t * 0.6f),
                   IM_COL32(14, 10, 18, static_cast<int>(255 * (1.0f - t))));
  }


  // Objectives: name and progress
  for (const auto& [id, objective] : prediction->getObjectives()) {
    ImVec2 centre = toZoomedScreen(objective.x, objective.y);
    if (centre.x < -100 || centre.x > Config::Screen::WIDTH + 100 ||
        centre.y < -100 || centre.y > Config::Screen::HEIGHT + 100) {
      continue;
    }
    if (!wasSeen(objective.x, objective.y)) continue;  // not discovered yet
    const char* label = objectiveLabel(objective.type);
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

  // Frogs: the pond is where they go, so mark it while one is carried.
  for (const auto& [id, objective] : prediction->getObjectives()) {
    if (objective.type != ObjectiveType::SaveTheFrogs) continue;
    if (objective.state != ObjectiveState::ReadyToDeposit) continue;
    ImVec2 pond = toZoomedScreen(objective.depositX, objective.depositY);
    float r = objective.radius * camera->zoom;
    dl->AddCircle(pond, r, IM_COL32(120, 220, 255, 200), 40, 2.5f);
    text(dl, ImVec2(pond.x, pond.y - 10.0f), IM_COL32(150, 230, 255, 255),
         15.0f, "POND");
  }

  // Enemies: health bar once hurt (always for elites and the boss)
  for (const auto& [id, view] : arena->enemies()) {
    if (view.dying) continue;
    if (!inSight(view.x, view.y)) continue;
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
        kind == Arena::EnemyKind::Boss
            ? nullptr
            : actorFor(Arena::enemyActorName(kind, elite));
    bool ownEliteArt = elite && Arena::eliteHasOwnActor(kind);
    float spriteHeight = enemyActor ? enemyActor->pivotY * enemyActor->scale * 0.72f
                                    : (sprite ? sprite->cellH : 40);
    float height = spriteHeight * def.drawScale *
                   (elite && !ownEliteArt ? 1.22f : 1.0f) * camera->zoom;
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
    if (mp && mp->scrapCarried >= Arena::Tuning::SCRAP_HEAVY_LOAD) {
      // A bundle of scrap held overhead, bobbing with the walk.
      float bob = std::sin(timeSeconds * 6.0f + id) * 2.0f;
      ImVec2 c(feet.x, top - (id == me ? 26.0f : 52.0f) + bob);
      const ActorSheet* pile = nullptr;
      const ActorAnim* pileIdle = nullptr;
      if (scrapSprite(&pile, &pileIdle)) {
        drawActorFrame(dl, *pile, *pileIdle, 0, 0.0f, ImVec2(c.x, c.y + 10.0f),
                       0.75f, IM_COL32_WHITE, true, -1, 0.0f, false);
      } else {
        ImVec2 a(c.x - 15.0f, c.y - 10.0f), b(c.x + 15.0f, c.y + 10.0f);
        dl->AddRectFilled(ImVec2(a.x - 2, a.y - 2), ImVec2(b.x + 2, b.y + 2),
                          IM_COL32(20, 14, 10, 255), 3.0f);
        dl->AddRectFilled(a, b, IM_COL32(176, 112, 54, 255), 2.0f);
      }
      char load[24];
      std::snprintf(load, sizeof load, "SCRAP %d", mp->scrapCarried);
      text(dl, ImVec2(c.x, c.y - 34.0f), IM_COL32(255, 200, 110, 255), 11.0f,
           load);
    }
  }

  // Floating numbers and status words
  for (const ArenaClient::FloatText& t : arena->floatTexts()) {
    if (!inSight(t.x, t.y)) continue;
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
