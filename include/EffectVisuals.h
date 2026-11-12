#pragma once

// How effects look in the world: every effect has its own colour, motion
// and icon, gets more intense as its stacks grow, and the build-defining
// verbs (apply, amplify, spread, detonate, corrupt, purify) each get a
// moment of their own. See docs/design/EFFECT_VISUALS.md.
//
// Steady state (auras, tints, particles, badges) comes from the effect
// list the server syncs ten times a second; verbs arrive as
// GameEventKind::EffectVerb events and are animated here.

#include <imgui.h>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ArenaDraw.h"
#include "Effect.h"
#include "EffectInstance.h"
#include "EventBus.h"
#include "NetworkProtocol.h"

class EffectVisuals {
public:
  enum class Motif : uint8_t {
    None,
    Bubbles, // poison: bubbles rise and pop around the body
    Drips,   // bleeding: drops fall from the body
    Sparks,  // power: bright sparks rise
    Wisps,   // weakness: dim wisps sink
    Frost,   // slow: ice flecks circle low
    Streaks, // haste: speed lines trail behind
    Motes,   // healing: soft motes drift up
    Plates   // armour: plates orbit
  };
  enum class Glyph : uint8_t {
    Letter,
    Droplet,
    ArrowUp,
    ArrowDown,
    Shield,
    CrackedShield,
    Cross,
    Snowflake,
    Chevrons,
    Star
  };
  struct Look {
    ArenaDraw::Rgb base;
    ArenaDraw::Rgb hot;
    Motif motif;
    Glyph glyph;
    float fullStacks; // stacks at which it reads as "maxed out"
  };
  static Look look(EffectType type);

  // Where an entity is now, for effects that follow their target.
  using PositionFn =
      std::function<bool(uint32_t id, bool isEnemy, float &x, float &y)>;
  using ScreenFn = std::function<ImVec2(float x, float y)>;

  EffectVisuals();

  void beginFrame(float timeSeconds);

  // Per entity, around its sprite (world layer, 1x coordinates).
  // `bodyHeight` is how tall the sprite stands, `radius` its footprint.
  void drawUnder(ImDrawList *dl, uint32_t id, bool isEnemy, ImVec2 feet,
                 float radius, const std::vector<EffectInstance> &effects);
  void drawOver(ImDrawList *dl, uint32_t id, bool isEnemy, ImVec2 feet,
                float bodyHeight, const std::vector<EffectInstance> &effects);
  // Multiply into the sprite's tint.
  ImU32 bodyTint(const std::vector<EffectInstance> &effects) const;
  // Icons with stack counts above the head (overhead layer, final pixels).
  void drawBadges(ImDrawList *dl, uint32_t id, bool isEnemy, ImVec2 centre,
                  const std::vector<EffectInstance> &effects, int limit);
  // Compact coloured pips for ordinary enemies, so packs stay readable.
  void drawPips(ImDrawList *dl, ImVec2 centre,
                const std::vector<EffectInstance> &effects, int limit);

  // Verb moments. Ground marks go under everything, the rest above.
  void drawGround(ImDrawList *dl, const ScreenFn &toScreen);
  void drawVerbs(ImDrawList *dl, const ScreenFn &toScreen,
                 const PositionFn &positionOf);
  // Callout text ("SPREAD x23"), crisp, with the zoomed projection.
  void drawCallouts(ImDrawList *dl, const ScreenFn &toZoomedScreen);

  // Camera shake from big moments; add to every world vertex.
  ImVec2 shakeOffset() const;

private:
  struct Track {
    float shown = 0.0f; // displayed stacks, eases toward the real count
    int last = 0;
    float applyMs = 0.0f; // sigil flash when it first lands
    float popMs = 0.0f;   // threshold pop (x5, x10, x20, x40)
    float seenAt = 0.0f;
  };
  struct Verb {
    EffectVerb verb;
    EffectType type;
    EffectType was = EffectType::Slow; // Corrupt / Purify
    uint32_t target = 0, from = 0;
    bool isEnemy = true;
    float x = 0, y = 0, x2 = 0, y2 = 0;
    float before = 0, after = 0;
    float ageMs = 0, delayMs = 0, lifeMs = 600;
    uint32_t seed = 0;
  };
  struct Callout {
    uint64_t key = 0;
    std::string title, detail;
    ArenaDraw::Rgb color, detailColor;
    float x = 0, y = 0;
    float ageMs = 0, lifeMs = 1100, delayMs = 0;
    float size = 16.0f;
    int count = 0;
  };
  struct Mark {
    float x, y, radius, ageMs, lifeMs;
    ArenaDraw::Rgb color;
  };

  void onPacket(const NetworkPacketReceivedEvent &e);
  void onUpdate(float dtMs);
  void addVerb(const GameEventPacket &p);
  Callout &callout(uint64_t key, float x, float y);
  Track &track(uint32_t id, bool isEnemy, EffectType type);
  float intensity(EffectType type, float stacks) const;

  float now = 0.0f;
  float frameDt = 0.0f;
  std::unordered_map<uint64_t, Track> tracks;
  std::vector<Verb> verbs;
  std::vector<Callout> callouts;
  std::vector<Mark> marks;
  float shakeMs = 0.0f, shakeAmp = 0.0f;
};
