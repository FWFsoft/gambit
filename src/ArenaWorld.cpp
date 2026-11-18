// The planet's scenery: set dressing (MapProp), objective set pieces,
// landmarks, the city and its skyline, and the far beacons that pull the
// crew outward. Part of ArenaView (see ArenaView.h).
//
// Every kind has an asset slot. If the sprite manifest has an actor sheet
// named for it (assets/sprites/manifest.txt), that art is drawn; otherwise a
// placeholder is built here from simple inked prisms. Slot names:
//   Prop_<kind>                     e.g. Prop_boulder, Prop_monolith
//   Prop_<kind>_<genus>             genus-dressed kinds (flora), e.g.
//                                   Prop_flora_venoxarid
//   Objective_<type>                e.g. Objective_capture_outpost
// See docs/design/MAP_PIPELINE.md, "Asset slots".
//
// The look (ART_DIRECTION.md): the world is dark and quiet; identity comes
// from silhouette and from light. Placeholders keep to slate values with
// genus colour arriving as glow, never as bright surfaces.

#include <imgui.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <string>

#include "ArenaDraw.h"
#include "ArenaView.h"
#include "ClientPrediction.h"
#include "CollisionSystem.h"
#include "config/ScreenConfig.h"

using namespace ArenaDraw;

namespace {

// Unit vectors of the tile axes in world space (+cx runs right-down, +cy
// runs left-down on screen).
constexpr float AXIS_X = 0.894427f;
constexpr float AXIS_Y = 0.447214f;

float hash01(uint32_t n) {
  n = (n << 13) ^ n;
  n = n * (n * n * 15731u + 789221u) + 1376312589u;
  return static_cast<float>(n & 0x7fffffffu) / 2147483647.0f;
}

ImU32 rgbA(float r, float g, float b, float a) {
  return IM_COL32(static_cast<int>(std::clamp(r, 0.0f, 1.0f) * 255),
                  static_cast<int>(std::clamp(g, 0.0f, 1.0f) * 255),
                  static_cast<int>(std::clamp(b, 0.0f, 1.0f) * 255),
                  static_cast<int>(std::clamp(a, 0.0f, 1.0f) * 255));
}

// A material: lit top, the side facing the light, the side away, the ink.
struct Stone {
  Rgb top, lit, dark;
};
constexpr Stone SLATE{{0.30f, 0.31f, 0.38f}, {0.22f, 0.23f, 0.29f},
                      {0.12f, 0.12f, 0.17f}};
constexpr Stone METAL{{0.26f, 0.27f, 0.32f}, {0.18f, 0.19f, 0.24f},
                      {0.09f, 0.09f, 0.13f}};
constexpr Stone BONE{{0.62f, 0.58f, 0.50f}, {0.46f, 0.43f, 0.38f},
                     {0.26f, 0.24f, 0.22f}};
constexpr Stone OBSIDIAN{{0.16f, 0.15f, 0.21f}, {0.10f, 0.10f, 0.14f},
                         {0.05f, 0.05f, 0.08f}};
constexpr Stone TOWER{{0.18f, 0.19f, 0.26f}, {0.13f, 0.14f, 0.20f},
                      {0.07f, 0.07f, 0.11f}};
const ImU32 INK = IM_COL32(6, 6, 10, 235);

ImU32 tone(Rgb c, float alpha = 1.0f, float mult = 1.0f) {
  return rgbA(c.r * mult, c.g * mult, c.b * mult, alpha);
}

Rgb genusRgb(Arena::Genus genus) {
  if (genus == Arena::Genus::None) return {0.55f, 0.75f, 0.85f};
  return themeRgb(genusTheme(genus));
}

}  // namespace

// ---------------------------------------------------------------------------
// Drawing primitives (in screen space; `base` is the footprint on screen)
// ---------------------------------------------------------------------------

namespace {

// A prism standing on a convex footprint (screen points in order), `tall`
// pixels high, its top scaled by `taper` about the footprint's centre.
// Faces turned toward the camera are drawn, shaded by which way they face.
void prism(ImDrawList* dl, const ImVec2* base, int n, float tall, float taper,
           const Stone& s, float alpha = 1.0f, bool ink = true) {
  assert(n >= 3 && n <= 32);
  ImVec2 c(0, 0);
  for (int i = 0; i < n; i++) {
    c.x += base[i].x;
    c.y += base[i].y;
  }
  c.x /= n;
  c.y /= n;
  ImVec2 top[32];
  for (int i = 0; i < n; i++) {
    top[i] = ImVec2(c.x + (base[i].x - c.x) * taper,
                    c.y + (base[i].y - c.y) * taper - tall);
  }
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;
    ImVec2 mid((base[i].x + base[j].x) * 0.5f, (base[i].y + base[j].y) * 0.5f);
    float nx = mid.x - c.x, ny = mid.y - c.y;
    if (ny < -0.5f) continue;  // faces away from the camera
    float len = std::sqrt(nx * nx + ny * ny) + 1e-3f;
    float side = nx / len;     // -1 facing left (lit) .. +1 facing right
    Rgb col{s.lit.r + (s.dark.r - s.lit.r) * (side * 0.5f + 0.5f),
            s.lit.g + (s.dark.g - s.lit.g) * (side * 0.5f + 0.5f),
            s.lit.b + (s.dark.b - s.lit.b) * (side * 0.5f + 0.5f)};
    ImVec2 quad[4] = {base[i], base[j], top[j], top[i]};
    dl->AddConvexPolyFilled(quad, 4, tone(col, alpha));
  }
  dl->AddConvexPolyFilled(top, n, tone(s.top, alpha));
  if (ink) {
    dl->AddPolyline(top, n, rgbA(0.02f, 0.02f, 0.04f, 0.9f * alpha),
                    ImDrawFlags_Closed, 1.5f);
    // The silhouette's sides: the leftmost and rightmost footprint points.
    int l = 0, r = 0;
    for (int i = 1; i < n; i++) {
      if (base[i].x < base[l].x) l = i;
      if (base[i].x > base[r].x) r = i;
    }
    dl->AddLine(base[l], top[l], INK, 2.0f);
    dl->AddLine(base[r], top[r], INK, 2.0f);
  }
}

// A soft contact shadow under something standing on the ground.
void groundShadow(ImDrawList* dl, ImVec2 c, float rx, float alpha = 0.35f) {
  dl->AddEllipseFilled(c, ImVec2(rx, rx * 0.45f), IM_COL32(0, 0, 0, alpha * 255),
                       0.0f, 20);
}

// A glow: concentric discs fading out. Drawn after the body.
void glow(ImDrawList* dl, ImVec2 c, float r, Rgb col, float alpha) {
  for (int i = 3; i >= 1; i--) {
    float k = i / 3.0f;
    dl->AddCircleFilled(c, r * k, tone(col, alpha * (1.2f - k) * 0.6f), 16);
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// Footprints in world space -> screen
// ---------------------------------------------------------------------------

void ArenaView::footprintBox(float x, float y, float alongX, float alongY,
                             float angle, ImVec2 out[4]) const {
  // Corners around (x, y): half-extents along the tile axes, rotated by
  // `angle` (radians) about the centre on the ground.
  const float ux = AXIS_X, uy = AXIS_Y, vx = -AXIS_X, vy = AXIS_Y;
  const float ca = std::cos(angle), sa = std::sin(angle);
  const float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
  for (int i = 0; i < 4; i++) {
    float a = sx[i] * alongX, b = sy[i] * alongY;
    float ra = a * ca - b * sa, rb = a * sa + b * ca;
    float wx = x + ux * ra + vx * rb;
    float wy = y + uy * ra + vy * rb;
    out[i] = toScreen(wx, wy);
  }
}

void ArenaView::footprintRound(float x, float y, float radius, int n,
                               ImVec2* out, float jitter, uint32_t seed) const {
  assert(n >= 3 && n <= 32);
  for (int i = 0; i < n; i++) {
    float a = i * 2.0f * PI / n;
    float r = radius * (1.0f + jitter * (hash01(seed * 31u + i) - 0.5f));
    out[i] = toScreen(x + std::cos(a) * r, y + std::sin(a) * r * 0.5f);
  }
}

Arena::Genus ArenaView::regionGenusAt(float x, float y) const {
  const TerrainGrid* grid = terrain();
  if (!grid || !arena->hasMatch()) return Arena::Genus::None;
  int region = grid->regionAt(x, y);
  if (region <= 0 || region >= Arena::REGION_SLOTS) return Arena::Genus::None;
  return static_cast<Arena::Genus>(arena->match().regionGenus[region]);
}

bool ArenaView::drawSlotArt(ImDrawList* dl, const std::string& slot, float x,
                            float y, int variant, float scale) {
  const ActorSheet* sheet = actorFor(slot);
  if (!sheet) return false;
  const ActorAnim* anim = sheet->find("idle");
  if (!anim) return false;
  int frame = anim->frames > 0 ? variant % anim->frames : 0;
  drawActorFrame(dl, *sheet, *anim, 0, 0.0f, toScreen(x, y), scale,
                 IM_COL32_WHITE, true, frame, 0.0f, true);
  return true;
}

// ---------------------------------------------------------------------------
// Props
// ---------------------------------------------------------------------------

void ArenaView::drawProp(ImDrawList* dl, const MapProp& p, uint32_t index) {
  const float s = p.scale;
  const float t = timeSeconds;
  const uint32_t seed = index * 7919u + 17u;

  // Genus-dressed kinds look like whoever holds the region this match.
  if (p.kind == "flora") {
    Arena::Genus genus = regionGenusAt(p.x, p.y);
    std::string slot = std::string("Prop_flora_") +
                       (genus == Arena::Genus::None ? "none" : Arena::genusName(genus));
    for (char& ch : slot) ch = static_cast<char>(std::tolower(ch));
    if (drawSlotArt(dl, slot, p.x, p.y, p.variant, s)) return;
    drawFlora(dl, p, genus, seed);
    return;
  }
  if (drawSlotArt(dl, "Prop_" + p.kind, p.x, p.y, p.variant, s)) return;

  ImVec2 feet = toScreen(p.x, p.y);
  ImVec2 pts[12];
  if (p.kind == "boulder") {
    footprintRound(p.x, p.y, 22.0f * s, 7, pts, 0.5f, seed);
    groundShadow(dl, feet, 30.0f * s);
    prism(dl, pts, 7, (16.0f + 18.0f * hash01(seed)) * s, 0.65f, SLATE);
  } else if (p.kind == "debris") {
    footprintBox(p.x, p.y, 16.0f * s, 8.0f * s, hash01(seed) * 3.0f, pts);
    groundShadow(dl, feet, 18.0f * s);
    prism(dl, pts, 4, 8.0f * s, 0.9f, METAL);
    float ember = 0.5f + 0.5f * std::sin(t * 2.3f + seed);
    dl->AddCircleFilled(ImVec2(feet.x + 6, feet.y - 9 * s), 2.0f,
                        rgbA(1.0f, 0.5f, 0.2f, 0.4f + 0.5f * ember), 8);
  } else if (p.kind == "bones") {
    // A rib cage half sunk in the dust.
    for (int i = 0; i < 4; i++) {
      ImVec2 a(feet.x - 24 * s + i * 14 * s, feet.y);
      dl->PathArcTo(ImVec2(a.x, a.y), 14.0f * s, PI * 1.05f, PI * 1.95f, 10);
      dl->PathStroke(tone(BONE.lit, 0.9f), 0, 3.0f);
    }
    dl->AddLine(ImVec2(feet.x - 30 * s, feet.y), ImVec2(feet.x + 30 * s, feet.y),
                tone(BONE.dark), 3.0f);
  } else if (p.kind == "ruin") {
    footprintRound(p.x, p.y, 14.0f * s, 8, pts);
    groundShadow(dl, feet, 22.0f * s);
    float tall = (34.0f + 50.0f * hash01(seed)) * s;
    prism(dl, pts, 8, tall, 0.92f, SLATE);
    float pulse = 0.5f + 0.5f * std::sin(t * 1.3f + seed * 0.37f);
    dl->AddLine(ImVec2(feet.x - 4, feet.y - tall * 0.3f),
                ImVec2(feet.x - 4, feet.y - tall * 0.75f),
                rgbA(0.35f, 0.9f, 1.0f, 0.25f + 0.35f * pulse), 2.0f);
  } else if (p.kind == "lamp") {
    Arena::Genus genus = regionGenusAt(p.x, p.y);
    Rgb neon = genusRgb(genus);
    groundShadow(dl, feet, 8.0f);
    dl->AddLine(feet, ImVec2(feet.x, feet.y - 58), INK, 3.0f);
    ImVec2 head(feet.x + 8, feet.y - 58);
    dl->AddLine(ImVec2(feet.x, feet.y - 58), head, INK, 3.0f);
    float flicker = hash01(seed + static_cast<uint32_t>(t * 6.0f)) > 0.97f ? 0.3f : 1.0f;
    glow(dl, head, 16.0f, neon, 0.8f * flicker);
    dl->AddCircleFilled(head, 3.0f, tone(neon, flicker, 1.3f), 8);
  } else if (p.kind == "sign") {
    Arena::Genus genus = regionGenusAt(p.x, p.y);
    Rgb neon = genusRgb(genus);
    dl->AddLine(feet, ImVec2(feet.x, feet.y - 40), INK, 3.0f);
    ImVec2 a(feet.x - 22, feet.y - 74), b(feet.x + 22, feet.y - 44);
    dl->AddRectFilled(a, b, rgbA(0.05f, 0.05f, 0.08f, 0.95f), 2.0f);
    float flicker = 0.75f + 0.25f * std::sin(t * 9.0f + seed);
    dl->AddRect(a, b, tone(neon, 0.9f * flicker, 1.2f), 2.0f, 0, 2.0f);
    // Alien script: a few glowing strokes.
    for (int i = 0; i < 3; i++) {
      float y = a.y + 8 + i * 8;
      float w = 10 + 18 * hash01(seed + i);
      dl->AddLine(ImVec2(a.x + 6, y), ImVec2(a.x + 6 + w, y),
                  tone(neon, 0.7f * flicker, 1.1f), 2.0f);
    }
  } else if (p.kind == "city_block") {
    drawCityBlock(dl, p, seed);
  } else if (p.kind == "monolith" || p.kind == "spire" || p.kind == "ribcage" ||
             p.kind == "crater" || p.kind == "freighter") {
    drawLandmark(dl, p, seed);
  }
  // "skyline" is drawn far away, after the fog (drawDistant).
}

void ArenaView::drawFlora(ImDrawList* dl, const MapProp& p, Arena::Genus genus,
                          uint32_t seed) {
  const float s = p.scale;
  const float t = timeSeconds;
  ImVec2 feet = toScreen(p.x, p.y);
  Rgb hue = genusRgb(genus);
  ImVec2 pts[8];
  switch (genus) {
    case Arena::Genus::Venoxarid: {
      // Egg sacs: dark, wet bulbs with a sick glow inside.
      groundShadow(dl, feet, 18.0f * s, 0.4f);
      for (int i = 0; i < 3; i++) {
        float ox = (hash01(seed + i) - 0.5f) * 22.0f * s;
        float r = (6.0f + 5.0f * hash01(seed + 9 + i)) * s;
        ImVec2 c(feet.x + ox, feet.y - r - i * 2.0f);
        dl->AddCircleFilled(c, r, rgbA(0.10f, 0.16f, 0.10f, 1.0f), 14);
        float beat = 0.5f + 0.5f * std::sin(t * 2.0f + seed + i);
        dl->AddCircleFilled(ImVec2(c.x - r * 0.2f, c.y - r * 0.1f), r * 0.55f,
                            rgbA(0.95f, 0.35f, 0.75f, 0.25f + 0.25f * beat), 12);
        dl->AddCircle(c, r, INK, 14, 1.5f);
      }
      break;
    }
    case Arena::Genus::Cerebrian: {
      // Thought crystals: thin shards, glowing at the tips.
      for (int i = 0; i < 3; i++) {
        float ox = (hash01(seed + i) - 0.5f) * 26.0f * s;
        footprintRound(p.x + ox, p.y + i * 3.0f, 5.0f * s, 4, pts);
        float tall = (22.0f + 26.0f * hash01(seed + 5 + i)) * s;
        prism(dl, pts, 4, tall, 0.15f, OBSIDIAN);
        dl->AddCircleFilled(ImVec2(feet.x + ox, feet.y + i * 1.5f - tall), 3.0f,
                            tone(hue, 0.85f), 8);
      }
      break;
    }
    case Arena::Genus::Frigidariant: {
      // Rime spikes.
      for (int i = 0; i < 3; i++) {
        float ox = (hash01(seed + i) - 0.5f) * 24.0f * s;
        footprintRound(p.x + ox, p.y + i * 3.0f, 7.0f * s, 5, pts);
        float tall = (14.0f + 22.0f * hash01(seed + 5 + i)) * s;
        Stone ice{{0.52f, 0.62f, 0.72f}, {0.34f, 0.42f, 0.52f}, {0.16f, 0.20f, 0.28f}};
        prism(dl, pts, 5, tall, 0.1f, ice, 0.95f);
      }
      break;
    }
    case Arena::Genus::Magnarok: {
      // Cinder vents: a low mound split by glowing cracks.
      footprintRound(p.x, p.y, 16.0f * s, 7, pts, 0.4f, seed);
      groundShadow(dl, feet, 20.0f * s);
      prism(dl, pts, 7, 9.0f * s, 0.55f, OBSIDIAN);
      float flare = 0.5f + 0.5f * std::sin(t * 3.0f + seed);
      dl->AddLine(ImVec2(feet.x - 8 * s, feet.y - 8 * s),
                  ImVec2(feet.x + 6 * s, feet.y - 11 * s),
                  tone(hue, 0.6f + 0.4f * flare, 1.2f), 2.0f);
      break;
    }
    default: {
      // Unclaimed scrub: low dark tufts.
      for (int i = 0; i < 3; i++) {
        float ox = (hash01(seed + i) - 0.5f) * 20.0f * s;
        dl->AddCircleFilled(ImVec2(feet.x + ox, feet.y - 5 * s), 6.0f * s,
                            rgbA(0.12f, 0.16f, 0.15f, 1.0f), 10);
      }
      break;
    }
  }
}

void ArenaView::drawCityBlock(ImDrawList* dl, const MapProp& p, uint32_t seed) {
  // A block of the coalition city: a dark tower with lit windows, its neon
  // in the colour of the genus whose district this is this match.
  const float tileEdge = 35.78f;  // world length of one tile along an axis
  float ax = p.width * tileEdge * 0.5f - 6.0f;
  float ay = p.depth * tileEdge * 0.5f - 6.0f;
  ImVec2 base[4];
  footprintBox(p.x, p.y, ax, ay, 0.0f, base);
  float tall = std::max(40.0f, p.tall);
  // Too far off screen to matter?
  if (!inView(base[2], 400, 400 + tall, 400, 400)) return;
  // A hero behind the tower must not vanish (Clarity): if one stands in
  // its shadow on screen, north of its front edge, the tower thins to a
  // ghost. Checked for every hero, not only you: you need to see your crew.
  float alpha = 1.0f;
  float left = std::min(base[0].x, base[3].x), right = std::max(base[1].x, base[2].x);
  float bottom = base[2].y, topY = base[0].y - tall;
  auto behind = [&](float wx, float wy) {
    ImVec2 q = toScreen(wx, wy);
    return q.x > left - 10 && q.x < right + 10 && q.y > topY && q.y < bottom &&
           wy < p.y + 40.0f;
  };
  const Player& me = prediction->getLocalPlayer();
  if (behind(me.x, me.y)) alpha = 0.28f;
  for (const auto& [id, view] : arena->players()) {
    if (view.state.characterId != 0 && behind(view.x, view.y)) alpha = 0.28f;
  }
  prism(dl, base, 4, tall, 1.0f, TOWER, alpha);
  if (alpha < 1.0f) return;
  Rgb neon = genusRgb(regionGenusAt(p.x, p.y));
  // Windows on the two faces we see: left (W..S) and right (S..E).
  auto windows = [&](ImVec2 a, ImVec2 b, uint32_t salt) {
    const int cols = 5;
    const int rows = std::max(2, static_cast<int>(tall / 26.0f));
    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < cols; c++) {
        if (hash01(seed * 131u + salt * 17u + r * 7u + c) > 0.42f) continue;
        float u = (c + 0.5f) / cols, v = (r + 0.6f) / rows;
        ImVec2 q(a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u - tall * (1.0f - v));
        bool warm = hash01(seed + r * 3u + c) > 0.7f;
        ImU32 col = warm ? rgbA(1.0f, 0.78f, 0.45f, 0.55f) : tone(neon, 0.45f, 1.1f);
        dl->AddRectFilled(ImVec2(q.x - 2.5f, q.y - 3.5f), ImVec2(q.x + 2.5f, q.y + 3.5f),
                          col);
      }
    }
  };
  windows(base[3], base[2], 1);
  windows(base[2], base[1], 2);
  // Rooftop neon trim.
  ImVec2 top[4];
  for (int i = 0; i < 4; i++) top[i] = ImVec2(base[i].x, base[i].y - tall);
  float pulse = 0.7f + 0.3f * std::sin(timeSeconds * 1.7f + seed);
  dl->AddLine(top[3], top[2], tone(neon, 0.8f * pulse, 1.2f), 2.0f);
  dl->AddLine(top[2], top[1], tone(neon, 0.8f * pulse, 1.2f), 2.0f);
}

void ArenaView::drawLandmark(ImDrawList* dl, const MapProp& p, uint32_t seed) {
  const float t = timeSeconds;
  ImVec2 feet = toScreen(p.x, p.y);
  ImVec2 pts[16];
  if (p.kind == "monolith") {
    // A black slab taller than anything near it, cut with a glowing glyph.
    footprintBox(p.x, p.y, 30.0f, 12.0f, 0.4f, pts);
    groundShadow(dl, feet, 60.0f, 0.45f);
    prism(dl, pts, 4, 230.0f, 0.82f, OBSIDIAN);
    float pulse = 0.5f + 0.5f * std::sin(t * 0.9f + seed);
    for (int i = 0; i < 5; i++) {
      float y = feet.y - 40 - i * 36;
      dl->AddLine(ImVec2(feet.x - 6, y), ImVec2(feet.x + 4, y - 14),
                  rgbA(0.4f, 0.95f, 1.0f, 0.35f + 0.5f * pulse), 3.0f);
    }
  } else if (p.kind == "spire") {
    footprintRound(p.x, p.y, 46.0f, 9, pts);
    groundShadow(dl, feet, 70.0f, 0.45f);
    prism(dl, pts, 9, 330.0f, 0.08f, OBSIDIAN);
    float pulse = 0.5f + 0.5f * std::sin(t * 1.5f + seed);
    for (int i = 1; i <= 3; i++) {
      float y = feet.y - i * 80.0f;
      float r = 46.0f * (1.0f - i * 80.0f / 330.0f) + 8.0f;
      dl->AddEllipse(ImVec2(feet.x, y), ImVec2(r, r * 0.45f),
                     rgbA(0.75f, 0.45f, 1.0f, 0.35f + 0.4f * pulse), 0.0f, 20, 2.0f);
    }
    glow(dl, ImVec2(feet.x, feet.y - 330.0f), 26.0f, {0.8f, 0.5f, 1.0f}, 0.9f);
  } else if (p.kind == "ribcage") {
    // The bones of something enormous: a spine and arching ribs.
    groundShadow(dl, feet, 140.0f, 0.3f);
    ImVec2 head(feet.x - 150, feet.y + 20), tail(feet.x + 150, feet.y - 30);
    dl->AddLine(head, tail, tone(BONE.dark), 10.0f);
    for (int i = 0; i < 7; i++) {
      float u = (i + 1) / 8.0f;
      ImVec2 c(head.x + (tail.x - head.x) * u, head.y + (tail.y - head.y) * u);
      float h = 110.0f * std::sin(u * PI) + 30.0f;
      dl->PathArcTo(ImVec2(c.x, c.y), h * 0.55f, PI * 1.08f, PI * 1.92f, 14);
      dl->PathStroke(tone(BONE.lit), 0, 7.0f);
      dl->PathArcTo(ImVec2(c.x, c.y), h * 0.55f, PI * 1.08f, PI * 1.92f, 14);
      dl->PathStroke(INK, 0, 1.5f);
    }
  } else if (p.kind == "crater") {
    // Whatever fell here still glows.
    float pulse = 0.5f + 0.5f * std::sin(t * 2.2f + seed);
    footprintRound(p.x, p.y, 22.0f, 5, pts, 0.6f, seed);
    prism(dl, pts, 5, 70.0f, 0.2f, OBSIDIAN);
    glow(dl, ImVec2(feet.x, feet.y - 40), 60.0f, {0.4f, 1.0f, 0.6f}, 0.5f + 0.4f * pulse);
  } else if (p.kind == "freighter") {
    // A coalition freighter, broken-backed where it came down.
    footprintBox(p.x - 60, p.y + 10, 120.0f, 34.0f, 0.25f, pts);
    groundShadow(dl, feet, 170.0f, 0.4f);
    prism(dl, pts, 4, 60.0f, 0.85f, METAL);
    footprintBox(p.x + 110, p.y - 30, 70.0f, 30.0f, -0.35f, pts);
    prism(dl, pts, 4, 48.0f, 0.8f, METAL);
    float blink = std::fmod(t + seed * 0.1f, 1.6f) < 0.25f ? 1.0f : 0.15f;
    glow(dl, ImVec2(feet.x - 150, feet.y - 50), 10.0f, {1.0f, 0.2f, 0.2f}, blink);
    glow(dl, ImVec2(feet.x + 150, feet.y - 70), 10.0f, {1.0f, 0.2f, 0.2f}, blink);
  }
}

// ---------------------------------------------------------------------------
// Objective set pieces: each objective is a place, not a reticle
// ---------------------------------------------------------------------------

namespace {
const char* objectiveSlot(ObjectiveType type) {
  switch (type) {
    case ObjectiveType::AlienScrapyard: return "Objective_alien_scrapyard";
    case ObjectiveType::CaptureOutpost: return "Objective_capture_outpost";
    case ObjectiveType::SalvageMedpacks: return "Objective_salvage_medpacks";
    case ObjectiveType::RecoverProbe: return "Objective_recover_probe";
    case ObjectiveType::SaveTheFrogs: return "Objective_save_the_frogs";
    case ObjectiveType::NoxiousGas: return "Objective_noxious_gas";
    case ObjectiveType::LittleJohn: return "Objective_little_john";
    case ObjectiveType::DerelictTurret: return "Objective_derelict_turret";
    default: return "Objective_other";
  }
}
}  // namespace

void ArenaView::drawObjectiveSite(ImDrawList* dl, const ClientObjective& o,
                                  uint32_t id) {
  const float t = timeSeconds;
  const bool done = o.state == ObjectiveState::Completed;
  const bool busy = o.state == ObjectiveState::InProgress ||
                    o.state == ObjectiveState::ReadyToDeposit;
  if (drawSlotArt(dl, objectiveSlot(o.type), o.x, o.y, done ? 1 : 0, 1.0f)) {
    return;
  }
  const float dim = done ? 0.55f : 1.0f;
  ImVec2 c = toScreen(o.x, o.y);
  ImVec2 pts[12];
  float pulse = 0.5f + 0.5f * std::sin(t * (busy ? 6.0f : 1.8f) + id);
  Arena::Genus genus = regionGenusAt(o.x, o.y);
  Rgb local = genusRgb(genus);
  switch (o.type) {
    case ObjectiveType::AlienScrapyard: {
      // A heap of wreckage under a leaning crane.
      groundShadow(dl, c, 70.0f, 0.4f);
      for (int i = 0; i < 5; i++) {
        float ox = (hash01(id * 5 + i) - 0.5f) * 90.0f;
        float oy = (hash01(id * 7 + i) - 0.5f) * 40.0f;
        footprintBox(o.x + ox, o.y + oy, 18.0f, 10.0f, hash01(id + i) * 3.0f, pts);
        prism(dl, pts, 4, 10.0f + 18.0f * hash01(id * 3 + i), 0.85f, METAL, dim);
      }
      ImVec2 mast(c.x + 40, c.y - 10);
      dl->AddLine(mast, ImVec2(mast.x - 10, mast.y - 110), INK, 5.0f);
      dl->AddLine(ImVec2(mast.x - 10, mast.y - 110), ImVec2(mast.x - 80, mast.y - 80),
                  INK, 4.0f);
      dl->AddLine(ImVec2(mast.x - 80, mast.y - 80), ImVec2(mast.x - 80, mast.y - 40),
                  rgbA(0.6f, 0.6f, 0.65f, 0.8f * dim), 1.5f);
      if (!done) glow(dl, ImVec2(mast.x - 10, mast.y - 110), 10.0f, {1.0f, 0.75f, 0.3f}, 0.6f + 0.4f * pulse);
      break;
    }
    case ObjectiveType::SalvageMedpacks: {
      // A dropped med pod, lid sealed until the camp is cleared.
      footprintRound(o.x, o.y, 26.0f, 10, pts);
      groundShadow(dl, c, 40.0f);
      prism(dl, pts, 10, 30.0f, 0.75f, METAL, dim);
      ImVec2 top(c.x, c.y - 34);
      Rgb green{0.4f, 1.0f, 0.6f};
      dl->AddRectFilled(ImVec2(top.x - 3, top.y - 9), ImVec2(top.x + 3, top.y + 3),
                        tone(green, done ? 0.2f : 0.6f + 0.4f * pulse));
      dl->AddRectFilled(ImVec2(top.x - 9, top.y - 6), ImVec2(top.x + 9, top.y),
                        tone(green, done ? 0.2f : 0.6f + 0.4f * pulse));
      break;
    }
    case ObjectiveType::CaptureOutpost: {
      // A coalition watchtower with its banner and floodlight.
      footprintBox(o.x, o.y, 22.0f, 22.0f, 0.0f, pts);
      groundShadow(dl, c, 50.0f, 0.4f);
      prism(dl, pts, 4, 120.0f, 0.7f, METAL, dim);
      ImVec2 top(c.x, c.y - 120);
      footprintBox(o.x, o.y, 30.0f, 30.0f, 0.0f, pts);
      for (ImVec2& q : pts) q.y -= 120.0f;
      prism(dl, pts, 4, 14.0f, 1.0f, TOWER, dim);
      Rgb flag = done ? Rgb{0.4f, 0.9f, 0.6f} : Rgb{0.95f, 0.25f, 0.25f};
      dl->AddLine(ImVec2(top.x + 14, top.y - 14), ImVec2(top.x + 14, top.y - 60), INK, 2.5f);
      float wave = std::sin(t * 4.0f + id) * 4.0f;
      ImVec2 banner[4] = {ImVec2(top.x + 14, top.y - 60), ImVec2(top.x + 44, top.y - 54 + wave),
                          ImVec2(top.x + 44, top.y - 38 + wave), ImVec2(top.x + 14, top.y - 44)};
      dl->AddConvexPolyFilled(banner, 4, tone(flag, 0.95f));
      glow(dl, ImVec2(top.x - 10, top.y - 4), 14.0f, flag, 0.5f + 0.4f * pulse);
      break;
    }
    case ObjectiveType::RecoverProbe: {
      // A probe nose-down in a scorched dent, its light still blinking.
      dl->AddEllipseFilled(c, ImVec2(46.0f, 20.0f), IM_COL32(8, 6, 8, 200), 0.0f, 20);
      if (o.state == ObjectiveState::Inactive) {
        footprintRound(o.x, o.y - 6, 12.0f, 8, pts);
        prism(dl, pts, 8, 42.0f, 0.55f, METAL);
        float blink = std::fmod(t + id * 0.3f, 1.2f) < 0.2f ? 1.0f : 0.2f;
        glow(dl, ImVec2(c.x, c.y - 48), 9.0f, {0.4f, 0.7f, 1.0f}, blink);
      }
      break;
    }
    case ObjectiveType::SaveTheFrogs: {
      // A pond, and the frogs that still need saving around it.
      dl->AddEllipseFilled(c, ImVec2(54.0f, 24.0f), IM_COL32(18, 34, 40, 235), 0.0f, 24);
      dl->AddEllipse(c, ImVec2(54.0f, 24.0f), tone(local, 0.35f), 0.0f, 24, 2.0f);
      for (int i = 0; i < 4; i++) {
        float a = i * 1.6f + id;
        ImVec2 pad(c.x + std::cos(a) * 30.0f, c.y + std::sin(a) * 12.0f);
        dl->AddEllipseFilled(pad, ImVec2(7, 3), IM_COL32(40, 80, 50, 230), 0.0f, 10);
      }
      if (!done) {
        float hop = std::max(0.0f, std::sin(t * 5.0f + id)) * 6.0f;
        dl->AddCircleFilled(ImVec2(c.x + 70, c.y - 4 - hop), 5.0f,
                            rgbA(0.5f, 0.95f, 0.3f, 1.0f), 10);
      }
      break;
    }
    case ObjectiveType::NoxiousGas: {
      // Spore vents breathing a poisonous haze.
      for (int i = 0; i < 3; i++) {
        float a = i * 2.1f + id;
        float ox = std::cos(a) * 34.0f, oy = std::sin(a) * 14.0f;
        footprintRound(o.x + ox, o.y + oy * 2.0f, 14.0f, 7, pts, 0.3f, id + i);
        prism(dl, pts, 7, 18.0f, 0.35f, OBSIDIAN, dim);
        if (!done) {
          float puff = std::fmod(t * 0.6f + i * 0.33f, 1.0f);
          dl->AddCircleFilled(ImVec2(c.x + ox, c.y + oy - 24 - puff * 40), 6 + puff * 16,
                              rgbA(0.5f, 0.95f, 0.3f, 0.30f * (1.0f - puff)), 14);
        }
      }
      break;
    }
    case ObjectiveType::LittleJohn: {
      // A gate between two pillars, sealed by a field.
      for (int side = -1; side <= 1; side += 2) {
        footprintBox(o.x + side * 46.0f, o.y - side * 10.0f, 12.0f, 12.0f, 0.0f, pts);
        prism(dl, pts, 4, 110.0f, 0.9f, SLATE, dim);
      }
      ImVec2 l = toScreen(o.x - 46.0f, o.y + 10.0f), r = toScreen(o.x + 46.0f, o.y - 10.0f);
      ImVec2 lintel[4] = {ImVec2(l.x - 10, l.y - 110), ImVec2(r.x + 10, r.y - 110),
                          ImVec2(r.x + 10, r.y - 124), ImVec2(l.x - 10, l.y - 124)};
      dl->AddConvexPolyFilled(lintel, 4, tone(SLATE.top, dim));
      dl->AddPolyline(lintel, 4, INK, ImDrawFlags_Closed, 2.0f);
      if (!done) {
        ImVec2 field[4] = {ImVec2(l.x, l.y), ImVec2(r.x, r.y), ImVec2(r.x, r.y - 108),
                           ImVec2(l.x, l.y - 108)};
        dl->AddConvexPolyFilled(field, 4, rgbA(0.7f, 0.4f, 1.0f, 0.18f + 0.12f * pulse));
      }
      break;
    }
    case ObjectiveType::DerelictTurret: {
      footprintBox(o.x, o.y, 20.0f, 20.0f, 0.0f, pts);
      groundShadow(dl, c, 36.0f);
      prism(dl, pts, 4, 24.0f, 0.8f, METAL, dim);
      float aim = done ? 0.0f : 0.3f * std::sin(t * 0.5f + id);
      ImVec2 hub(c.x, c.y - 30);
      dl->AddLine(hub, ImVec2(hub.x + 40 * std::cos(aim - 0.4f), hub.y - 14 + 30 * std::sin(aim - 0.4f)),
                  INK, 7.0f);
      dl->AddCircleFilled(hub, 9.0f, tone(METAL.lit, dim), 12);
      break;
    }
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// The distance: the skyline beyond the shore and the beacons of unvisited
// places. Drawn after the fog: you see a glow out there before you know
// what it is (Strategic Crunch: a reason to go; Clarity: where to go).
// ---------------------------------------------------------------------------

void ArenaView::drawDistant(ImDrawList* dl) {
  const float t = timeSeconds;
  const Player& local = prediction->getLocalPlayer();

  if (props) {
    for (size_t i = 0; i < props->size(); i++) {
      const MapProp& p = (*props)[i];
      if (p.kind != "skyline") continue;
      ImVec2 foot = toScreen(p.x, p.y);
      float w = std::max(60.0f, p.width), h = std::max(200.0f, p.tall);
      if (!inView(foot, w, h + 200, w, 200)) continue;
      ImVec2 a(foot.x - w * 0.5f, foot.y - h), b(foot.x + w * 0.5f, foot.y);
      dl->AddRectFilledMultiColor(a, b, IM_COL32(16, 18, 32, 245), IM_COL32(16, 18, 32, 245),
                                  IM_COL32(8, 9, 16, 160), IM_COL32(8, 9, 16, 160));
      uint32_t seed = static_cast<uint32_t>(i) * 977u;
      for (int k = 0; k < 40; k++) {
        if (hash01(seed + k) > 0.7f) continue;
        float u = hash01(seed + k * 3u), v = hash01(seed + k * 5u + 1u);
        ImVec2 q(a.x + 8 + u * (w - 16), a.y + 12 + v * (h * 0.85f));
        bool warm = hash01(seed + k * 11u) > 0.6f;
        dl->AddRectFilled(ImVec2(q.x - 2, q.y - 3), ImVec2(q.x + 2, q.y + 3),
                          warm ? IM_COL32(255, 200, 120, 120) : IM_COL32(120, 200, 255, 110));
      }
      float blink = std::fmod(t * 0.7f + i * 0.13f, 1.0f) < 0.15f ? 1.0f : 0.3f;
      dl->AddCircleFilled(ImVec2(foot.x, a.y - 6), 3.0f, IM_COL32(255, 60, 60, 255 * blink), 8);
    }
  }

  // Beacons: places not yet done, seen as a column of light from afar.
  for (const auto& [id, o] : prediction->getObjectives()) {
    if (o.state == ObjectiveState::Completed) continue;
    float d = std::hypot(o.x - local.x, o.y - local.y);
    if (d > BEACON_RANGE || inSight(o.x, o.y)) continue;
    float far = std::hypot(o.x, o.y);  // the ship is at the origin
    Rgb col = far > 4000.0f ? Rgb{1.0f, 0.62f, 0.2f} : Rgb{1.0f, 0.9f, 0.55f};
    float fade = std::clamp((BEACON_RANGE - d) / 900.0f, 0.0f, 1.0f);
    float pulse = 0.75f + 0.25f * std::sin(t * 1.4f + id);
    ImVec2 foot = toScreen(o.x, o.y);
    for (int k = 0; k < 3; k++) {
      float wdt = 10.0f + k * 9.0f;
      dl->AddRectFilledMultiColor(ImVec2(foot.x - wdt, foot.y - 420.0f), ImVec2(foot.x + wdt, foot.y),
                                  tone(col, 0.0f), tone(col, 0.0f),
                                  tone(col, 0.16f * fade * pulse / (k + 1)),
                                  tone(col, 0.16f * fade * pulse / (k + 1)));
    }
    dl->AddCircleFilled(foot, 6.0f, tone(col, 0.7f * fade * pulse), 10);
  }
  // Landmarks glow on the horizon too, in their own colours.
  if (props) {
    for (size_t i = 0; i < props->size(); i++) {
      const MapProp& p = (*props)[i];
      Rgb col;
      if (p.kind == "monolith") col = {0.4f, 0.95f, 1.0f};
      else if (p.kind == "spire") col = {0.8f, 0.5f, 1.0f};
      else if (p.kind == "crater") col = {0.4f, 1.0f, 0.6f};
      else continue;
      float d = std::hypot(p.x - local.x, p.y - local.y);
      if (d > BEACON_RANGE * 1.4f || inSight(p.x, p.y)) continue;
      ImVec2 foot = toScreen(p.x, p.y);
      float fade = std::clamp((BEACON_RANGE * 1.4f - d) / 900.0f, 0.0f, 1.0f);
      dl->AddRectFilledMultiColor(ImVec2(foot.x - 22, foot.y - 520), ImVec2(foot.x + 22, foot.y),
                                  tone(col, 0.0f), tone(col, 0.0f), tone(col, 0.14f * fade),
                                  tone(col, 0.14f * fade));
    }
  }
}

void ArenaView::sceneryLights(
    const std::function<void(float, float, float, Rgb, float)>& add) const {
  if (props) {
    for (const MapProp& p : *props) {
      if (p.kind == "monolith") add(p.x, p.y, 260.0f, {0.4f, 0.95f, 1.0f}, 0.45f);
      else if (p.kind == "spire") add(p.x, p.y, 320.0f, {0.8f, 0.5f, 1.0f}, 0.5f);
      else if (p.kind == "crater") add(p.x, p.y, 300.0f, {0.4f, 1.0f, 0.6f}, 0.6f);
      else if (p.kind == "lamp" || p.kind == "sign") {
        add(p.x, p.y, 110.0f, genusRgb(regionGenusAt(p.x, p.y)), 0.35f);
      } else if (p.kind == "flora") {
        Arena::Genus g = regionGenusAt(p.x, p.y);
        if (g != Arena::Genus::None) add(p.x, p.y, 70.0f, genusRgb(g), 0.18f);
      }
    }
  }
  for (const auto& [id, o] : prediction->getObjectives()) {
    if (o.state == ObjectiveState::Completed) continue;
    add(o.x, o.y, 220.0f, {1.0f, 0.82f, 0.5f}, 0.35f);
  }
}
