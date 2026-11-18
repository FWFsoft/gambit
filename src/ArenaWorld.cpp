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
#include "config/WorldScale.h"

using namespace ArenaDraw;

namespace {

// Unit vectors of the tile axes in world space (+cx runs right-down, +cy
// runs left-down on screen).
constexpr float AXIS_X = 0.894427f;
constexpr float AXIS_Y = 0.447214f;

// Scenery is sized in metres (Config::World): a hero is about 1.8 m.
constexpr float M = Config::World::METRE;

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
  if (drawVegetation(dl, p, seed)) return;

  ImVec2 feet = toScreen(p.x, p.y);
  ImVec2 pts[12];
  if (p.kind == "boulder") {
    // Waist to head high: cover-sized rocks.
    footprintRound(p.x, p.y, 0.9f * M * s, 7, pts, 0.5f, seed);
    groundShadow(dl, feet, 1.2f * M * s);
    prism(dl, pts, 7, (0.7f + 0.8f * hash01(seed)) * M * s, 0.65f, SLATE);
  } else if (p.kind == "debris") {
    // Hull plates and machinery from the crash, a car door to a bonnet.
    footprintBox(p.x, p.y, 0.9f * M * s, 0.5f * M * s, hash01(seed) * 3.0f, pts);
    groundShadow(dl, feet, 1.1f * M * s);
    prism(dl, pts, 4, 0.4f * M * s, 0.9f, METAL);
    float ember = 0.5f + 0.5f * std::sin(t * 2.3f + seed);
    dl->AddCircleFilled(ImVec2(feet.x + 0.3f * M * s, feet.y - 0.6f * M * s), 3.0f,
                        rgbA(1.0f, 0.5f, 0.2f, 0.4f + 0.5f * ember), 8);
  } else if (p.kind == "bones") {
    // The rib cage of a large beast, half sunk in the dust: knee high.
    for (int i = 0; i < 4; i++) {
      ImVec2 a(feet.x - 1.3f * M * s + i * 0.85f * M * s, feet.y);
      dl->PathArcTo(ImVec2(a.x, a.y), 0.8f * M * s, PI * 1.05f, PI * 1.95f, 12);
      dl->PathStroke(tone(BONE.lit, 0.9f), 0, 4.0f);
    }
    dl->AddLine(ImVec2(feet.x - 1.7f * M * s, feet.y),
                ImVec2(feet.x + 1.7f * M * s, feet.y), tone(BONE.dark), 4.0f);
  } else if (p.kind == "ruin") {
    // Broken columns, from head height to twice it.
    footprintRound(p.x, p.y, 0.45f * M * s, 8, pts);
    groundShadow(dl, feet, 0.8f * M * s);
    float tall = (1.5f + 3.0f * hash01(seed)) * M * s;
    prism(dl, pts, 8, tall, 0.92f, SLATE);
    float pulse = 0.5f + 0.5f * std::sin(t * 1.3f + seed * 0.37f);
    dl->AddLine(ImVec2(feet.x - 0.1f * M, feet.y - tall * 0.3f),
                ImVec2(feet.x - 0.1f * M, feet.y - tall * 0.75f),
                rgbA(0.35f, 0.9f, 1.0f, 0.25f + 0.35f * pulse), 3.0f);
  } else if (p.kind == "lamp") {
    // A street lamp: 4.5 m, well over a hero's head.
    Arena::Genus genus = regionGenusAt(p.x, p.y);
    Rgb neon = genusRgb(genus);
    const float pole = 4.5f * M, arm = 1.0f * M;
    groundShadow(dl, feet, 0.35f * M);
    dl->AddLine(feet, ImVec2(feet.x, feet.y - pole), INK, 5.0f);
    ImVec2 head(feet.x + arm, feet.y - pole + 0.15f * M);
    dl->AddLine(ImVec2(feet.x, feet.y - pole), head, INK, 4.0f);
    float flicker = hash01(seed + static_cast<uint32_t>(t * 6.0f)) > 0.97f ? 0.3f : 1.0f;
    glow(dl, head, 0.9f * M, neon, 0.8f * flicker);
    dl->AddCircleFilled(head, 0.12f * M, tone(neon, flicker, 1.3f), 10);
  } else if (p.kind == "sign") {
    // A neon sign on a post, its board above head height.
    Arena::Genus genus = regionGenusAt(p.x, p.y);
    Rgb neon = genusRgb(genus);
    dl->AddLine(feet, ImVec2(feet.x, feet.y - 2.6f * M), INK, 5.0f);
    ImVec2 a(feet.x - 1.3f * M, feet.y - 4.2f * M), b(feet.x + 1.3f * M, feet.y - 2.5f * M);
    dl->AddRectFilled(a, b, rgbA(0.05f, 0.05f, 0.08f, 0.95f), 3.0f);
    float flicker = 0.75f + 0.25f * std::sin(t * 9.0f + seed);
    dl->AddRect(a, b, tone(neon, 0.9f * flicker, 1.2f), 3.0f, 0, 3.0f);
    // Alien script: a few glowing strokes.
    for (int i = 0; i < 3; i++) {
      float y = a.y + 0.4f * M + i * 0.42f * M;
      float w = (0.5f + 1.2f * hash01(seed + i)) * M;
      dl->AddLine(ImVec2(a.x + 0.3f * M, y), ImVec2(a.x + 0.3f * M + w, y),
                  tone(neon, 0.7f * flicker, 1.1f), 4.0f);
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
  // The shapes below were drawn at about 1/2.2 of real size; this puts egg
  // sacs at 0.25-0.45 m, crystals and rime spikes at 0.6-1.9 m.
  const float s = p.scale * 2.2f;
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

// ---------------------------------------------------------------------------
// Biome dressing: trees, cacti, palms, reeds, grass and sea stacks
// (generate_planet.py, VEGETATION). Placeholders until art fills the
// Prop_<kind> slots. Real sizes from MAP_PIPELINE.md, "Biomes": anything
// taller than a hero thins to a ghost with a hero behind it (Clarity).
// Dark, low-saturation foliage: the world stays quiet, the heroes are the
// brightest things on screen.
// ---------------------------------------------------------------------------

namespace {

constexpr Rgb FIR{0.10f, 0.20f, 0.19f};  // PNW conifer, blue-green
constexpr Rgb FIR_DARK{0.05f, 0.11f, 0.11f};
constexpr Rgb SNOW{0.62f, 0.68f, 0.76f};    // cool and dim, never white
constexpr Rgb CANOPY{0.10f, 0.25f, 0.12f};  // jungle, saturated green
constexpr Rgb CANOPY_DARK{0.05f, 0.13f, 0.06f};
constexpr Rgb MOSSY{0.20f, 0.22f, 0.13f};  // cypress, olive
constexpr Rgb FROND{0.14f, 0.24f, 0.14f};
constexpr Rgb CACTUS{0.16f, 0.24f, 0.15f};
constexpr Rgb STRAW{0.30f, 0.30f, 0.18f};  // dry grass
constexpr Rgb REED{0.22f, 0.26f, 0.16f};
constexpr Rgb BARK{0.13f, 0.10f, 0.08f};

// One tier of a conifer: a flat-bottomed triangle, inked.
void firTier(ImDrawList* dl, ImVec2 base, float halfW, float tall, Rgb lit,
             Rgb dark, float alpha) {
  ImVec2 apex(base.x, base.y - tall);
  ImVec2 l(base.x - halfW, base.y), r(base.x + halfW, base.y);
  ImVec2 m(base.x, base.y + halfW * 0.12f);
  // Lit from the upper left: the left half a shade lighter.
  ImVec2 left[3] = {apex, l, m};
  ImVec2 right[3] = {apex, m, r};
  dl->AddConvexPolyFilled(left, 3, tone(lit, alpha));
  dl->AddConvexPolyFilled(right, 3, tone(dark, alpha));
  ImVec2 outline[4] = {l, apex, r, m};
  dl->AddPolyline(outline, 4, rgbA(0.02f, 0.02f, 0.04f, 0.85f * alpha),
                  ImDrawFlags_Closed, 1.5f);
}

}  // namespace

bool ArenaView::drawVegetation(ImDrawList* dl, const MapProp& p,
                               uint32_t seed) {
  const std::string& k = p.kind;
  if (k != "conifer" && k != "snow_conifer" && k != "jungle_tree" &&
      k != "cypress" && k != "palm" && k != "cactus" && k != "grass" &&
      k != "reeds" && k != "sea_stack") {
    return false;
  }
  const float s = p.scale;
  const float t = timeSeconds;
  ImVec2 feet = toScreen(p.x, p.y);
  // A slow sway for anything with leaves (a pixel or two, never a strobe).
  const float sway = std::sin(t * 0.8f + seed * 0.61f) * 0.08f * M;

  if (k == "conifer" || k == "snow_conifer") {
    // PNW fir 9 to 14 m; in the snow a little shorter and narrower.
    const bool snowy = k == "snow_conifer";
    const float tall =
        (snowy ? 8.0f + 3.5f * hash01(seed) : 9.0f + 5.0f * hash01(seed)) * M *
        s;
    const float halfW = (snowy ? 1.6f : 2.1f) * M * s;
    groundShadow(dl, feet, halfW * 1.1f, 0.4f);
    float alpha = ghostAlpha(feet.x - halfW, feet.x + halfW, feet.y - tall,
                             feet.y + 0.3f * M, p.y + 20.0f);
    // Trunk: the bottom eighth, below the lowest boughs.
    dl->AddLine(feet, ImVec2(feet.x, feet.y - tall * 0.2f), tone(BARK, alpha),
                0.35f * M * s);
    const int tiers = 4;
    for (int i = 0; i < tiers; i++) {
      float f = static_cast<float>(i) / tiers;
      float y = feet.y - tall * (0.12f + 0.2f * i);
      float w = halfW * (1.0f - 0.2f * i);
      float h = tall * (0.42f - 0.05f * i);
      ImVec2 base(feet.x + sway * f, y);
      firTier(dl, base, w, h, FIR, FIR_DARK, alpha);
      if (snowy) {
        // Snow lying along the bough tips of each tier, the part the tier
        // above does not cover: a pale sliver on both lower edges.
        for (int side = -1; side <= 1; side += 2) {
          ImVec2 cap[4] = {
              ImVec2(base.x + side * w * 0.98f, base.y - h * 0.02f),
              ImVec2(base.x + side * w * 0.45f, base.y - h * 0.58f),
              ImVec2(base.x + side * w * 0.38f, base.y - h * 0.50f),
              ImVec2(base.x + side * w * 0.80f, base.y + h * 0.02f)};
          dl->AddConvexPolyFilled(cap, 4,
                                  tone(SNOW, (side < 0 ? 0.9f : 0.6f) * alpha));
        }
      }
    }
    return true;
  }

  if (k == "jungle_tree") {
    // A rainforest tree: a pale trunk 6 to 9 m to a broad dark canopy 6 to
    // 9 m across, a few lobes of it overlapping.
    const float trunk = (6.0f + 3.0f * hash01(seed)) * M * s;
    const float crown = (3.0f + 1.5f * hash01(seed + 3)) * M * s;
    groundShadow(dl, feet, crown * 0.9f, 0.45f);
    float alpha =
        ghostAlpha(feet.x - crown, feet.x + crown, feet.y - trunk - crown,
                   feet.y + 0.3f * M, p.y + 20.0f);
    ImVec2 top(feet.x + sway * 0.5f, feet.y - trunk);
    dl->AddLine(feet, top, tone(BARK, alpha, 1.6f), 0.5f * M * s);
    // Buttress roots.
    dl->AddLine(feet, ImVec2(feet.x - 0.9f * M * s, feet.y + 0.1f * M),
                tone(BARK, alpha), 0.25f * M);
    dl->AddLine(feet, ImVec2(feet.x + 0.8f * M * s, feet.y + 0.15f * M),
                tone(BARK, alpha), 0.25f * M);
    for (int i = 0; i < 4; i++) {
      float ox = (hash01(seed + 10 + i) - 0.5f) * crown * 1.3f;
      float oy = (hash01(seed + 20 + i) - 0.5f) * crown * 0.5f;
      float r = crown * (0.55f + 0.25f * hash01(seed + 30 + i));
      ImVec2 c(top.x + ox + sway, top.y + oy);
      dl->AddCircleFilled(c, r, tone(i % 2 ? CANOPY_DARK : CANOPY, alpha), 18);
      dl->AddCircle(c, r, rgbA(0.02f, 0.02f, 0.04f, 0.7f * alpha), 18, 1.5f);
    }
    return true;
  }

  if (k == "cypress") {
    // Swamp cypress 6 to 10 m: a flared foot in the water, a narrow,
    // drooping crown hung with moss.
    const float tall = (6.0f + 4.0f * hash01(seed)) * M * s;
    const float halfW = 1.3f * M * s;
    groundShadow(dl, feet, halfW, 0.35f);
    float alpha = ghostAlpha(feet.x - halfW * 1.4f, feet.x + halfW * 1.4f,
                             feet.y - tall, feet.y + 0.3f * M, p.y + 20.0f);
    ImVec2 flare[3] = {ImVec2(feet.x - 0.9f * M * s, feet.y),
                       ImVec2(feet.x + 0.9f * M * s, feet.y),
                       ImVec2(feet.x, feet.y - 1.6f * M * s)};
    dl->AddConvexPolyFilled(flare, 3, tone(BARK, alpha, 1.3f));
    ImVec2 top(feet.x + sway * 0.4f, feet.y - tall);
    dl->AddLine(feet, top, tone(BARK, alpha, 1.3f), 0.4f * M * s);
    for (int i = 0; i < 3; i++) {
      float y = top.y + tall * (0.12f + 0.2f * i);
      ImVec2 c(top.x + (hash01(seed + i) - 0.5f) * halfW + sway, y);
      dl->AddEllipseFilled(c, ImVec2(halfW * (1.0f + 0.2f * i), tall * 0.12f),
                           tone(MOSSY, alpha, 1.0f - 0.15f * i), 0.0f, 16);
      // Hanging moss: two short strands under each mass.
      for (int j = -1; j <= 1; j += 2) {
        ImVec2 a(c.x + j * halfW * 0.5f, c.y + tall * 0.06f);
        dl->AddLine(a, ImVec2(a.x + sway, a.y + 1.2f * M * s),
                    tone(MOSSY, 0.7f * alpha, 0.8f), 2.0f);
      }
    }
    return true;
  }

  if (k == "palm") {
    // A palm 6 to 9 m: a leaning, curved trunk and a burst of fronds.
    const float tall = (6.0f + 3.0f * hash01(seed)) * M * s;
    const float lean = (hash01(seed + 1) - 0.5f) * 2.4f * M * s;
    groundShadow(dl, feet, 1.6f * M * s, 0.35f);
    ImVec2 top(feet.x + lean + sway, feet.y - tall);
    float alpha = ghostAlpha(std::min(feet.x, top.x) - 3.0f * M * s,
                             std::max(feet.x, top.x) + 3.0f * M * s,
                             top.y - 1.0f * M, feet.y + 0.3f * M, p.y + 20.0f);
    ImVec2 trunk[6];
    for (int i = 0; i < 6; i++) {
      float f = i / 5.0f;
      // Curve: lean grows toward the top.
      trunk[i] = ImVec2(feet.x + (lean + sway) * f * f, feet.y - tall * f);
    }
    dl->AddPolyline(trunk, 6, tone(BARK, alpha, 1.7f), 0, 0.35f * M * s);
    for (int i = 0; i < 7; i++) {
      float a = PI * (1.05f + 0.9f * i / 6.0f) +
                (hash01(seed + 40 + i) - 0.5f) * 0.3f;
      float len = (2.4f + 1.0f * hash01(seed + 50 + i)) * M * s;
      ImVec2 mid(top.x + std::cos(a) * len * 0.55f,
                 top.y + std::sin(a) * len * 0.35f);
      ImVec2 tip(top.x + std::cos(a) * len,
                 top.y + std::sin(a) * len * 0.3f + 0.9f * M * s);
      ImVec2 frond[3] = {top, mid, tip};
      dl->AddPolyline(frond, 3, tone(FROND, alpha), 0, 0.35f * M * s);
    }
    dl->AddCircleFilled(top, 0.35f * M * s, tone(BARK, alpha), 8);
    return true;
  }

  if (k == "cactus") {
    // A saguaro 3 to 5 m: a ribbed column with one or two arms.
    const float tall = (3.0f + 2.0f * hash01(seed)) * M * s;
    const float thick = 0.55f * M * s;
    groundShadow(dl, feet, 0.8f * M * s, 0.4f);
    float alpha = ghostAlpha(feet.x - 1.5f * M * s, feet.x + 1.5f * M * s,
                             feet.y - tall, feet.y + 0.3f * M, p.y + 20.0f);
    ImU32 body = tone(CACTUS, alpha),
          ink = rgbA(0.02f, 0.02f, 0.04f, 0.85f * alpha);
    ImVec2 top(feet.x, feet.y - tall);
    dl->AddLine(feet, top, ink, thick + 3.0f);
    dl->AddLine(feet, top, body, thick);
    dl->AddCircleFilled(top, thick * 0.5f, body, 10);
    const int arms = 1 + static_cast<int>(hash01(seed + 2) * 2.0f);
    for (int i = 0; i < arms; i++) {
      float side = (i % 2 == 0) ? -1.0f : 1.0f;
      float at = tall * (0.35f + 0.2f * hash01(seed + 4 + i));
      ImVec2 a(feet.x, feet.y - at);
      ImVec2 b(feet.x + side * 0.9f * M * s, feet.y - at);
      ImVec2 c(b.x, b.y - (0.9f + 0.8f * hash01(seed + 6 + i)) * M * s);
      ImVec2 arm[3] = {a, b, c};
      dl->AddPolyline(arm, 3, ink, 0, thick * 0.75f + 3.0f);
      dl->AddPolyline(arm, 3, body, 0, thick * 0.75f);
      dl->AddCircleFilled(c, thick * 0.38f, body, 8);
    }
    // A rib down the lit side.
    dl->AddLine(ImVec2(feet.x - thick * 0.2f, feet.y - 0.2f * M),
                ImVec2(top.x - thick * 0.2f, top.y),
                tone(CACTUS, 0.8f * alpha, 1.5f), 1.5f);
    return true;
  }

  if (k == "grass" || k == "reeds") {
    // Knee to waist high: never hides a hero, so no ghost.
    const bool reeds = k == "reeds";
    const int blades = reeds ? 7 : 8;
    const float high = (reeds ? 1.2f : 0.7f) * M * s;
    const Rgb col = reeds ? REED : STRAW;
    for (int i = 0; i < blades; i++) {
      float ox = (hash01(seed + i) - 0.5f) * (reeds ? 1.2f : 1.0f) * M * s;
      float h = high * (0.7f + 0.5f * hash01(seed + 9 + i));
      float bend = sway * 0.6f + (hash01(seed + 17 + i) - 0.5f) * 0.25f * M * s;
      ImVec2 a(feet.x + ox, feet.y);
      ImVec2 b(a.x + bend, a.y - h);
      dl->AddLine(a, b, tone(col, 0.95f, 0.9f + 0.5f * hash01(seed + 25 + i)),
                  reeds ? 3.0f : 3.0f);
      if (reeds && i % 2 == 0) {
        // A cattail head.
        dl->AddLine(ImVec2(b.x, b.y + 0.05f * M), ImVec2(b.x, b.y + 0.3f * M),
                    tone(BARK, 1.0f, 1.6f), 4.0f);
      }
    }
    return true;
  }

  // Sea stack: a rock pillar 6 to 15 m standing in the surf.
  ImVec2 pts[8];
  const float tall = (6.0f + 9.0f * hash01(seed)) * M * s;
  const float r0 = (1.6f + 1.2f * hash01(seed + 1)) * M * s;
  footprintRound(p.x, p.y, r0, 8, pts, 0.5f, seed);
  float alpha = ghostAlpha(feet.x - r0, feet.x + r0, feet.y - tall,
                           feet.y + r0 * 0.5f, p.y + 20.0f);
  prism(dl, pts, 8, tall, 0.7f, SLATE, alpha);
  // Surf at its foot.
  float wash = 0.5f + 0.5f * std::sin(t * 1.1f + seed);
  dl->AddEllipse(feet, ImVec2(r0 * 1.25f, r0 * 0.55f),
                 rgbA(0.55f, 0.70f, 0.80f, 0.15f + 0.15f * wash), 0.0f, 20,
                 2.0f);
  return true;
}

float ArenaView::ghostAlpha(float left, float right, float top, float bottom,
                            float frontY) const {
  // A hero behind something tall must not vanish (Clarity): if one stands
  // in its shadow on screen, north of its front edge, it thins to a ghost.
  // Checked for every hero, not only you: you need to see your crew.
  auto behind = [&](float wx, float wy) {
    ImVec2 q = toScreen(wx, wy);
    return q.x > left - 10 && q.x < right + 10 && q.y > top && q.y < bottom &&
           wy < frontY;
  };
  const Player& me = prediction->getLocalPlayer();
  if (behind(me.x, me.y)) return 0.28f;
  for (const auto& [id, view] : arena->players()) {
    if (view.state.characterId != 0 && behind(view.x, view.y)) return 0.28f;
  }
  return 1.0f;
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
  float alpha = ghostAlpha(std::min(base[0].x, base[3].x), std::max(base[1].x, base[2].x),
                           base[0].y - tall, base[2].y, p.y + 40.0f);
  prism(dl, base, 4, tall, 1.0f, TOWER, alpha);
  if (alpha < 1.0f) return;
  Rgb neon = genusRgb(regionGenusAt(p.x, p.y));
  // Windows on the two faces we see: left (W..S) and right (S..E). One row
  // per storey, one window per ~3 m of facade, each about a hero tall; the
  // ground floor gets a door and a shopfront instead. Doors are the scale
  // cue: a hero is a door high.
  const float storey = Config::World::STOREY * M;
  const int rows = std::max(1, static_cast<int>(std::lround(tall / storey)));
  auto windows = [&](ImVec2 a, ImVec2 b, uint32_t salt) {
    float facade = std::hypot(b.x - a.x, b.y - a.y);
    const int cols = std::max(2, static_cast<int>(facade / (3.0f * M)));
    auto at = [&](float u, float height) {
      return ImVec2(a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u - height);
    };
    for (int r = 1; r < rows; r++) {  // r storeys above the ground floor
      for (int c = 0; c < cols; c++) {
        if (hash01(seed * 131u + salt * 17u + r * 7u + c) > 0.42f) continue;
        ImVec2 q = at((c + 0.5f) / cols, (r + 0.45f) * storey);
        bool warm = hash01(seed + r * 3u + c) > 0.7f;
        // Dim: the world stays dark and quiet; windows are texture, the
        // heroes are the brightest things on screen (ART_DIRECTION.md).
        ImU32 col = warm ? rgbA(1.0f, 0.78f, 0.45f, 0.30f) : tone(neon, 0.22f, 1.0f);
        dl->AddRectFilled(ImVec2(q.x - 0.45f * M, q.y - 0.8f * M),
                          ImVec2(q.x + 0.45f * M, q.y + 0.8f * M), col);
      }
    }
    // Ground floor: a door (2.2 m) and a lit shop window beside it.
    int doorAt = static_cast<int>(hash01(seed * 7u + salt) * cols);
    ImVec2 d = at((doorAt + 0.5f) / cols, 0.0f);
    dl->AddRectFilled(ImVec2(d.x - 0.55f * M, d.y - 2.2f * M), ImVec2(d.x + 0.55f * M, d.y),
                      rgbA(0.02f, 0.02f, 0.04f, 1.0f));
    dl->AddRect(ImVec2(d.x - 0.55f * M, d.y - 2.2f * M), ImVec2(d.x + 0.55f * M, d.y),
                rgbA(1.0f, 0.78f, 0.45f, 0.5f), 0.0f, 0, 2.0f);
    for (int c = 0; c < cols; c++) {
      if (c == doorAt || hash01(seed * 53u + salt * 5u + c) > 0.5f) continue;
      ImVec2 q = at((c + 0.5f) / cols, 1.5f * M);
      dl->AddRectFilled(ImVec2(q.x - 0.9f * M, q.y - 0.6f * M),
                        ImVec2(q.x + 0.9f * M, q.y + 0.6f * M), tone(neon, 0.18f, 1.0f));
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
    // A black slab 14 m tall, cut with a glowing glyph.
    const float tall = 14.0f * M;
    footprintBox(p.x, p.y, 2.2f * M, 0.8f * M, 0.4f, pts);
    groundShadow(dl, feet, 3.5f * M, 0.45f);
    float alpha = ghostAlpha(feet.x - 2.2f * M, feet.x + 2.2f * M, feet.y - tall,
                             feet.y + 1.0f * M, p.y + 40.0f);
    prism(dl, pts, 4, tall, 0.82f, OBSIDIAN, alpha);
    if (alpha < 1.0f) return;
    float pulse = 0.5f + 0.5f * std::sin(t * 0.9f + seed);
    for (int i = 0; i < 5; i++) {
      float y = feet.y - 2.5f * M - i * 2.4f * M;
      dl->AddLine(ImVec2(feet.x - 0.3f * M, y), ImVec2(feet.x + 0.25f * M, y - 0.8f * M),
                  rgbA(0.4f, 0.95f, 1.0f, 0.35f + 0.5f * pulse), 5.0f);
    }
  } else if (p.kind == "spire") {
    // A 24 m needle of black glass, ringed with light.
    const float tall = 24.0f * M, r0 = 3.5f * M;
    footprintRound(p.x, p.y, r0, 9, pts);
    groundShadow(dl, feet, 5.0f * M, 0.45f);
    float alpha = ghostAlpha(feet.x - r0, feet.x + r0, feet.y - tall, feet.y + r0 * 0.5f,
                             p.y + 40.0f);
    prism(dl, pts, 9, tall, 0.08f, OBSIDIAN, alpha);
    if (alpha < 1.0f) return;
    float pulse = 0.5f + 0.5f * std::sin(t * 1.5f + seed);
    for (int i = 1; i <= 3; i++) {
      float h = i * 6.0f * M;
      float r = r0 * (1.0f - h / tall) + 0.4f * M;
      dl->AddEllipse(ImVec2(feet.x, feet.y - h), ImVec2(r, r * 0.45f),
                     rgbA(0.75f, 0.45f, 1.0f, 0.35f + 0.4f * pulse), 0.0f, 24, 3.0f);
    }
    glow(dl, ImVec2(feet.x, feet.y - tall), 1.5f * M, {0.8f, 0.5f, 1.0f}, 0.9f);
  } else if (p.kind == "ribcage") {
    // The bones of something enormous: a 24 m spine and ribs you walk under.
    groundShadow(dl, feet, 12.0f * M, 0.3f);
    ImVec2 head(feet.x - 12.0f * M, feet.y + 1.0f * M);
    ImVec2 tail(feet.x + 12.0f * M, feet.y - 1.5f * M);
    dl->AddLine(head, tail, tone(BONE.dark), 16.0f);
    for (int i = 0; i < 7; i++) {
      float u = (i + 1) / 8.0f;
      ImVec2 c(head.x + (tail.x - head.x) * u, head.y + (tail.y - head.y) * u);
      float h = 6.0f * M * std::sin(u * PI) + 1.5f * M;
      dl->PathArcTo(ImVec2(c.x, c.y), h * 0.55f, PI * 1.08f, PI * 1.92f, 16);
      dl->PathStroke(tone(BONE.lit), 0, 11.0f);
      dl->PathArcTo(ImVec2(c.x, c.y), h * 0.55f, PI * 1.08f, PI * 1.92f, 16);
      dl->PathStroke(INK, 0, 2.0f);
    }
  } else if (p.kind == "crater") {
    // Whatever fell here still glows: a 4 m shard in the crater's heart.
    float pulse = 0.5f + 0.5f * std::sin(t * 2.2f + seed);
    footprintRound(p.x, p.y, 1.5f * M, 5, pts, 0.6f, seed);
    float alpha = ghostAlpha(feet.x - 1.5f * M, feet.x + 1.5f * M, feet.y - 4.0f * M,
                             feet.y + 0.8f * M, p.y + 40.0f);
    prism(dl, pts, 5, 4.0f * M, 0.2f, OBSIDIAN, alpha);
    glow(dl, ImVec2(feet.x, feet.y - 2.2f * M), 3.5f * M, {0.4f, 1.0f, 0.6f},
         0.5f + 0.4f * pulse);
  } else if (p.kind == "freighter") {
    // A coalition freighter, broken-backed where it came down: 28 m long.
    float alpha = ghostAlpha(feet.x - 10.5f * M, feet.x + 10.5f * M, feet.y - 7.0f * M,
                             feet.y + 3.5f * M, p.y + 3.0f * M);
    footprintBox(p.x - 3.5f * M, p.y + 0.5f * M, 7.0f * M, 2.2f * M, 0.25f, pts);
    groundShadow(dl, feet, 10.0f * M, 0.4f);
    prism(dl, pts, 4, 4.5f * M, 0.85f, METAL, alpha);
    footprintBox(p.x + 6.5f * M, p.y - 1.7f * M, 4.0f * M, 1.8f * M, -0.35f, pts);
    prism(dl, pts, 4, 3.5f * M, 0.8f, METAL, alpha);
    float blink = std::fmod(t + seed * 0.1f, 1.6f) < 0.25f ? 1.0f : 0.15f;
    glow(dl, ImVec2(feet.x - 9.0f * M, feet.y - 3.5f * M), 0.6f * M, {1.0f, 0.2f, 0.2f}, blink);
    glow(dl, ImVec2(feet.x + 9.0f * M, feet.y - 4.5f * M), 0.6f * M, {1.0f, 0.2f, 0.2f}, blink);
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
      // Wreckage heaped round the back of the yard under a 9 m crane. The
      // heaps sit behind the zone and stay below waist height, so whoever
      // works it, or walks past it, stays in view (Clarity).
      groundShadow(dl, c, 4.0f * M, 0.4f);
      for (int i = 0; i < 5; i++) {
        float a = PI * (1.0f + i / 4.0f) + (hash01(id * 5 + i) - 0.5f) * 0.4f;
        float r = (2.4f + 0.9f * hash01(id * 7 + i)) * M;
        footprintBox(o.x + std::cos(a) * r, o.y + std::sin(a) * r * 0.5f, 1.0f * M,
                     0.6f * M, hash01(id + i) * 3.0f, pts);
        prism(dl, pts, 4, (0.4f + 0.5f * hash01(id * 3 + i)) * M, 0.85f, METAL, dim);
      }
      ImVec2 mast = toScreen(o.x + 2.6f * M, o.y - 1.6f * M);
      ImVec2 jibRoot(mast.x - 0.5f * M, mast.y - 9.0f * M);
      ImVec2 jibTip(mast.x - 5.5f * M, mast.y - 7.5f * M);
      dl->AddLine(mast, jibRoot, INK, 14.0f);
      dl->AddLine(mast, jibRoot, tone(METAL.dark, dim), 9.0f);
      dl->AddLine(jibRoot, jibTip, INK, 10.0f);
      dl->AddLine(jibRoot, jibTip, tone(METAL.dark, dim), 6.0f);
      dl->AddLine(jibTip, ImVec2(jibTip.x, jibTip.y + 4.0f * M),
                  rgbA(0.6f, 0.6f, 0.65f, 0.8f * dim), 2.0f);
      if (!done) glow(dl, jibRoot, 0.6f * M, {1.0f, 0.75f, 0.3f}, 0.6f + 0.4f * pulse);
      break;
    }
    case ObjectiveType::SalvageMedpacks: {
      // A dropped med pod, waist high, lid sealed until the camp is cleared.
      footprintRound(o.x, o.y, 1.0f * M, 10, pts);
      groundShadow(dl, c, 1.4f * M);
      prism(dl, pts, 10, 1.3f * M, 0.75f, METAL, dim);
      ImVec2 top(c.x, c.y - 1.45f * M);
      Rgb green{0.4f, 1.0f, 0.6f};
      ImU32 cross = tone(green, done ? 0.2f : 0.6f + 0.4f * pulse);
      dl->AddRectFilled(ImVec2(top.x - 0.12f * M, top.y - 0.4f * M),
                        ImVec2(top.x + 0.12f * M, top.y + 0.1f * M), cross);
      dl->AddRectFilled(ImVec2(top.x - 0.37f * M, top.y - 0.27f * M),
                        ImVec2(top.x + 0.37f * M, top.y - 0.03f * M), cross);
      break;
    }
    case ObjectiveType::CaptureOutpost: {
      // A coalition watchtower: a hut on a platform 6 m up, on four legs a
      // hero can stand between, with its banner and floodlight.
      const float legs = 6.0f * M, half = 1.6f * M;
      groundShadow(dl, c, 2.6f * M, 0.4f);
      ImVec2 foot[4], top[4];
      footprintBox(o.x, o.y, half, half, 0.0f, foot);
      footprintBox(o.x, o.y, half * 0.8f, half * 0.8f, 0.0f, top);
      for (ImVec2& q : top) q.y -= legs;
      ImU32 leg = tone(METAL.dark, dim);
      for (int i = 0; i < 4; i++) {
        if (i == 0) continue;  // the back leg hides behind the platform
        dl->AddLine(foot[i], top[i], INK, 7.0f);
        dl->AddLine(foot[i], top[i], leg, 4.0f);
      }
      // Cross braces on the two faces we see.
      dl->AddLine(foot[3], top[2], leg, 2.5f);
      dl->AddLine(foot[2], top[1], leg, 2.5f);
      ImVec2 deck[4];
      footprintBox(o.x, o.y, 2.0f * M, 2.0f * M, 0.0f, deck);
      for (ImVec2& q : deck) q.y -= legs;
      prism(dl, deck, 4, 0.4f * M, 1.0f, METAL, dim);
      ImVec2 hut[4];
      footprintBox(o.x, o.y, 1.4f * M, 1.4f * M, 0.0f, hut);
      for (ImVec2& q : hut) q.y -= legs + 0.4f * M;
      prism(dl, hut, 4, 2.2f * M, 0.9f, TOWER, dim);
      ImVec2 roof(c.x, c.y - legs - 2.6f * M);
      Rgb flag = done ? Rgb{0.4f, 0.9f, 0.6f} : Rgb{0.95f, 0.25f, 0.25f};
      ImVec2 poleTop(roof.x + 0.8f * M, roof.y - 2.8f * M);
      dl->AddLine(ImVec2(roof.x + 0.8f * M, roof.y), poleTop, INK, 3.5f);
      float wave = std::sin(t * 4.0f + id) * 0.2f * M;
      ImVec2 banner[4] = {poleTop, ImVec2(poleTop.x + 1.6f * M, poleTop.y + 0.3f * M + wave),
                          ImVec2(poleTop.x + 1.6f * M, poleTop.y + 1.2f * M + wave),
                          ImVec2(poleTop.x, poleTop.y + 0.9f * M)};
      dl->AddConvexPolyFilled(banner, 4, tone(flag, 0.95f));
      glow(dl, ImVec2(c.x - 1.2f * M, c.y - legs - 1.6f * M), 0.8f * M, flag,
           0.5f + 0.4f * pulse);
      break;
    }
    case ObjectiveType::RecoverProbe: {
      // A 2 m probe nose-down in a scorched dent, its light still blinking.
      dl->AddEllipseFilled(c, ImVec2(2.2f * M, 1.0f * M), IM_COL32(8, 6, 8, 200), 0.0f, 24);
      if (o.state == ObjectiveState::Inactive) {
        footprintRound(o.x, o.y - 0.1f * M, 0.55f * M, 8, pts);
        prism(dl, pts, 8, 1.9f * M, 0.55f, METAL);
        float blink = std::fmod(t + id * 0.3f, 1.2f) < 0.2f ? 1.0f : 0.2f;
        glow(dl, ImVec2(c.x, c.y - 2.1f * M), 0.4f * M, {0.4f, 0.7f, 1.0f}, blink);
      }
      break;
    }
    case ObjectiveType::SaveTheFrogs: {
      // A pond, and the frogs that still need saving around it.
      const float rx = 2.6f * M, ry = 1.2f * M;
      dl->AddEllipseFilled(c, ImVec2(rx, ry), IM_COL32(18, 34, 40, 235), 0.0f, 28);
      dl->AddEllipse(c, ImVec2(rx, ry), tone(local, 0.35f), 0.0f, 28, 2.5f);
      for (int i = 0; i < 4; i++) {
        float a = i * 1.6f + id;
        ImVec2 pad(c.x + std::cos(a) * 1.5f * M, c.y + std::sin(a) * 0.6f * M);
        dl->AddEllipseFilled(pad, ImVec2(0.35f * M, 0.15f * M), IM_COL32(40, 80, 50, 230),
                             0.0f, 12);
      }
      if (!done) {
        float hop = std::max(0.0f, std::sin(t * 5.0f + id)) * 0.3f * M;
        dl->AddCircleFilled(ImVec2(c.x + 3.2f * M, c.y - 0.2f * M - hop), 0.18f * M,
                            rgbA(0.5f, 0.95f, 0.3f, 1.0f), 12);
      }
      break;
    }
    case ObjectiveType::NoxiousGas: {
      // Spore vents, knee high, breathing a poisonous haze.
      for (int i = 0; i < 3; i++) {
        float a = i * 2.1f + id;
        float ox = std::cos(a) * 1.9f * M, oy = std::sin(a) * 0.8f * M;
        footprintRound(o.x + ox, o.y + oy * 2.0f, 0.7f * M, 7, pts, 0.3f, id + i);
        prism(dl, pts, 7, 0.8f * M, 0.35f, OBSIDIAN, dim);
        if (!done) {
          float puff = std::fmod(t * 0.6f + i * 0.33f, 1.0f);
          dl->AddCircleFilled(ImVec2(c.x + ox, c.y + oy - 1.1f * M - puff * 2.0f * M),
                              (0.3f + puff * 0.9f) * M,
                              rgbA(0.5f, 0.95f, 0.3f, 0.30f * (1.0f - puff)), 16);
        }
      }
      break;
    }
    case ObjectiveType::LittleJohn: {
      // A 6 m gate between two pillars, sealed by a field.
      const float gx = 2.5f * M, gy = 0.55f * M, ph = 6.0f * M, lt = 0.7f * M;
      for (int side = -1; side <= 1; side += 2) {
        footprintBox(o.x + side * gx, o.y - side * gy, 0.6f * M, 0.6f * M, 0.0f, pts);
        prism(dl, pts, 4, ph, 0.9f, SLATE, dim);
      }
      ImVec2 l = toScreen(o.x - gx, o.y + gy), r = toScreen(o.x + gx, o.y - gy);
      ImVec2 lintel[4] = {ImVec2(l.x - 0.6f * M, l.y - ph), ImVec2(r.x + 0.6f * M, r.y - ph),
                          ImVec2(r.x + 0.6f * M, r.y - ph - lt),
                          ImVec2(l.x - 0.6f * M, l.y - ph - lt)};
      dl->AddConvexPolyFilled(lintel, 4, tone(SLATE.top, dim));
      dl->AddPolyline(lintel, 4, INK, ImDrawFlags_Closed, 2.5f);
      if (!done) {
        ImVec2 field[4] = {ImVec2(l.x, l.y), ImVec2(r.x, r.y), ImVec2(r.x, r.y - ph),
                           ImVec2(l.x, l.y - ph)};
        dl->AddConvexPolyFilled(field, 4, rgbA(0.7f, 0.4f, 1.0f, 0.18f + 0.12f * pulse));
      }
      break;
    }
    case ObjectiveType::DerelictTurret: {
      // A 2.5 m gun emplacement on a chest-high base.
      footprintBox(o.x, o.y, 1.1f * M, 1.1f * M, 0.0f, pts);
      groundShadow(dl, c, 1.6f * M);
      prism(dl, pts, 4, 1.3f * M, 0.8f, METAL, dim);
      float aim = done ? 0.0f : 0.3f * std::sin(t * 0.5f + id);
      ImVec2 hub(c.x, c.y - 1.6f * M);
      dl->AddLine(hub, ImVec2(hub.x + 2.4f * M * std::cos(aim - 0.4f),
                              hub.y - 0.6f * M + 1.6f * M * std::sin(aim - 0.4f)),
                  INK, 10.0f);
      dl->AddCircleFilled(hub, 0.45f * M, tone(METAL.lit, dim), 14);
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
