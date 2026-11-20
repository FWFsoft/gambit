// The crash intro: what MatchPhase::Intro looks like (docs/design/DECISIONS.md,
// "Crash intro").
//
//   0 s     the planet fades up from black; distress sirens far off
//   7 s     the burning ship bursts past the camera and falls away into it
//   12.5 s  it hits; a flash on the planet
//   12.7 s  cut to black, the crash
//   13.3 s  the world fades in; the crew gets up off the ground
//   16.8 s  the round starts, no count-in
//
// The server owns the clock (MatchDirector::startIntro); this draws it from
// MatchStatePacket::timeRemaining, so every screen shows the same moment.
// The soundtrack is SfxSystem's. The art is baked by
// tools/cutscene/render_intro.py into assets/cutscene/.

#include <imgui.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

#include "ArenaView.h"
#include "CharacterSelectionState.h"
#include "ClientPrediction.h"
#include "NetworkProtocol.h"
#include "OpenGLUtils.h"
#include "PresentationLayer.h"
#include "Texture.h"
#include "TextureManager.h"
#include "config/ScreenConfig.h"

namespace {

namespace I = Arena::Intro;

constexpr float SCREEN_W = static_cast<float>(Config::Screen::WIDTH);
constexpr float SCREEN_H = static_cast<float>(Config::Screen::HEIGHT);

// The shot. The planet image's disc is ~70% of the image.
const ImVec2 PLANET_CENTER(700.0f, 405.0f);
constexpr float PLANET_DRAW = 880.0f;
// Where the ship falls into the planet: its vanishing point.
const ImVec2 SHIP_TARGET(772.0f, 482.0f);
// Where it is on screen at depth 1 (it comes from behind the camera, lower
// right). Its offset from the target shrinks as 1 / depth.
const ImVec2 SHIP_NEAR(1235.0f, 835.0f);
constexpr float SHIP_DEPTH_START = 0.55f;  // behind the camera's near edge
constexpr float SHIP_DEPTH_END = 70.0f;    // a spark on the planet
constexpr float SHIP_SIZE = 1150.0f;       // a sheet cell, px at depth 1
// The baked views (render_intro.py SHIP_VIEW_DEG): degrees between the
// camera and the ship's tail, from side-on to straight behind.
constexpr float SHIP_VIEW_DEG[] = {78, 70, 62, 55, 48, 42, 37, 32, 28,
                                   24, 21, 18, 15, 13, 11, 9,  7,  6};
constexpr int SHIP_FRAMES = sizeof(SHIP_VIEW_DEG) / sizeof(SHIP_VIEW_DEG[0]);
constexpr int SHIP_COLUMNS = 6;
constexpr float SHIP_SIDEWAYS = 3.0f;  // how far off the camera's line it flies

constexpr float FLAME_RATE = 280.0f;  // fire particles a second
constexpr float SKIP_HOLD = 0.6f;     // seconds holding Space to skip

float smoothstep(float a, float b, float x) {
  float u = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
  return u * u * (3.0f - 2.0f * u);
}

// Stable noise from an integer: particles are a pure function of time, so
// a skip or a late join draws the right frame without any history.
float hash(uint32_t n) {
  n = (n << 13U) ^ n;
  n = n * (n * n * 15731U + 789221U) + 1376312589U;
  return static_cast<float>(n & 0x7fffffffU) / 2147483647.0f;
}

ImU32 rgba(float r, float g, float b, float a) {
  auto c = [](float v) {
    return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
  };
  return IM_COL32(c(r), c(g), c(b), c(a));
}

ImTextureID tex(unsigned int id) {
  return (ImTextureID)(intptr_t)id;
}

void additive(ImDrawList* dl) {
  dl->AddCallback(
      [](const ImDrawList*, const ImDrawCmd*) {
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE,
                            GL_ONE_MINUS_SRC_ALPHA);
      },
      nullptr);
}

void normalBlend(ImDrawList* dl) {
  dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

// The ship's depth (1 = as near as SHIP_NEAR) at a time in the intro.
float shipDepth(float elapsed) {
  float u = std::clamp((elapsed - I::SHIP_ENTER) /
                           (I::SHIP_IMPACT - I::SHIP_ENTER),
                       0.0f, 1.0f);
  return SHIP_DEPTH_START +
         (SHIP_DEPTH_END - SHIP_DEPTH_START) * std::pow(u, 1.25f);
}

ImVec2 project(ImVec2 offsetAtDepthOne, float depth) {
  return ImVec2(SHIP_TARGET.x + offsetAtDepthOne.x / depth,
                SHIP_TARGET.y + offsetAtDepthOne.y / depth);
}

int shipFrame(float depth) {
  float view = std::atan(SHIP_SIDEWAYS / depth) * 180.0f / 3.14159265f;
  int best = 0;
  for (int i = 0; i < SHIP_FRAMES; i++) {
    if (std::fabs(SHIP_VIEW_DEG[i] - view) <
        std::fabs(SHIP_VIEW_DEG[best] - view)) {
      best = i;
    }
  }
  return best;
}

// Which way the ship's tail points on screen: back where it came from.
ImVec2 tailDirection() {
  float dx = SHIP_NEAR.x - SHIP_TARGET.x, dy = SHIP_NEAR.y - SHIP_TARGET.y;
  float len = std::sqrt(dx * dx + dy * dy);
  return ImVec2(dx / len, dy / len);
}

// A point on the hull at depth, on screen. `along` is in cell sizes from
// the sprite's centre toward the tail, `side` across it. Seen more from
// behind, the hull shortens.
ImVec2 shipPoint(float depth, float along, float side) {
  ImVec2 centre = project(ImVec2(SHIP_NEAR.x - SHIP_TARGET.x,
                                 SHIP_NEAR.y - SHIP_TARGET.y),
                          depth);
  float behind = static_cast<float>(shipFrame(depth)) /
                 static_cast<float>(SHIP_FRAMES - 1);
  along *= 1.0f - 0.5f * behind;
  ImVec2 d = tailDirection();
  float size = SHIP_SIZE / depth;
  return ImVec2(centre.x + (d.x * along - d.y * side) * size,
                centre.y + (d.y * along + d.x * side) * size);
}

// The tail (nozzles): near the top of the cell side-on.
ImVec2 shipTail(float depth) { return shipPoint(depth, 0.40f, 0.0f); }

// Where the hull is burning: the roof, the cab, a torn flank (cell units,
// along and across, and how big the fire is).
struct HullFire {
  float along, side, size;
};
constexpr HullFire HULL_FIRES[] = {
    {0.24f, 0.05f, 1.0f},   {0.15f, -0.06f, 0.8f}, {0.05f, 0.07f, 1.2f},
    {-0.04f, -0.03f, 0.7f}, {-0.12f, 0.06f, 0.9f}, {-0.22f, -0.05f, 0.6f},
    {-0.30f, 0.03f, 0.8f},  {0.31f, -0.02f, 0.9f},
};
constexpr int HULL_FIRE_COUNT = sizeof(HULL_FIRES) / sizeof(HULL_FIRES[0]);

}  // namespace

unsigned int ArenaView::introGlow() {
  if (introGlowTexture != 0) return introGlowTexture;
  // A soft round light, white: tinted per draw.
  constexpr int N = 64;
  std::vector<uint8_t> pixels(N * N * 4);
  for (int y = 0; y < N; y++) {
    for (int x = 0; x < N; x++) {
      float dx = (x + 0.5f) / N * 2.0f - 1.0f, dy = (y + 0.5f) / N * 2.0f - 1.0f;
      float r2 = dx * dx + dy * dy;
      float a = r2 >= 1.0f ? 0.0f : std::exp(-r2 * 4.0f) * (1.0f - r2);
      uint8_t* p = &pixels[(y * N + x) * 4];
      p[0] = p[1] = p[2] = 255;
      p[3] = static_cast<uint8_t>(a * 255.0f + 0.5f);
    }
  }
  GLint previous = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
  glGenTextures(1, &introGlowTexture);
  glBindTexture(GL_TEXTURE_2D, introGlowTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels.data());
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
  assert(introGlowTexture != 0);
  return introGlowTexture;
}

bool ArenaView::renderIntro(float dt) {
  // Picked, and so has everyone else: the intro is about to start (the
  // selection is on its way to the server, while the first frames load the
  // sprite sheets). Hold on its opening black rather than flash the lobby.
  if (arena->hasMatch() && arena->phase() == Arena::MatchPhase::Lobby &&
      CharacterSelectionState::instance().hasSelection()) {
    bool everyone = true;
    for (const MatchPlayerState& p : arena->match().players) {
      if (p.playerId != localId() && p.characterId == 0) everyone = false;
    }
    if (everyone) {
      TextureManager::instance().get("assets/cutscene/stars.png");
      TextureManager::instance().get("assets/cutscene/planet.png");
      TextureManager::instance().get("assets/cutscene/ship.png");
      introGlow();
      ImGui::GetBackgroundDrawList()->AddRectFilled(
          ImVec2(0, 0), ImVec2(SCREEN_W, SCREEN_H), IM_COL32(0, 0, 0, 255));
      return true;
    }
  }
  if (!arena->hasMatch() || arena->phase() != Arena::MatchPhase::Intro) {
    introClock = -1.0f;
    introGetUp = -1.0f;
    introSkipHeld = 0.0f;
    introSkipSent = false;
    return false;
  }
  // The server's clock arrives in steps; run a smooth one that never goes
  // backward and never strays far from it.
  float server = I::LENGTH - arena->match().timeRemaining;
  if (introClock < 0.0f || server - introClock > 0.25f) {
    introClock = server;  // started, joined late, or skipped
  } else {
    introClock = std::min(introClock + dt, server + 0.25f);
  }
  const float e = std::clamp(introClock, 0.0f, I::LENGTH);

  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  if (e < I::FADE_START) {
    introGetUp = -1.0f;
    if (e < I::CRASH) {
      drawIntroSpace(dl, e);
    } else {
      dl->AddRectFilled(ImVec2(0, 0), ImVec2(SCREEN_W, SCREEN_H),
                        IM_COL32(0, 0, 0, 255));
    }
  } else {
    // The world, fading in from black as the crew gets up. No HUD until
    // the round starts.
    introGetUp = smoothstep(I::GETUP_START, I::GETUP_END, e);
    updateFog();
    renderWorld(dl);
    float black = 1.0f - smoothstep(I::FADE_START, I::FADE_END, e);
    if (black > 0.0f) {
      dl->AddRectFilled(ImVec2(0, 0), ImVec2(SCREEN_W, SCREEN_H),
                        rgba(0, 0, 0, black));
    }
  }
  drawIntroSkip(ImGui::GetForegroundDrawList(), e, dt);
  return true;
}

void ArenaView::drawIntroSpace(ImDrawList* dl, float e) {
  // The shot is finished art: put the (empty) world scene on screen first,
  // so the bloom and grade made for the world do not wash the shot out.
  if (presentation && presentation->sceneOpen() &&
      !presentation->resolveQueued()) {
    dl->AddCallback(
        [](const ImDrawList*, const ImDrawCmd* cmd) {
          static_cast<PresentationLayer*>(cmd->UserCallbackData)->resolve();
        },
        presentation);
    dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    presentation->markResolveQueued();
  }
  const int firstVertex = dl->VtxBuffer.Size;
  Texture* stars = TextureManager::instance().get("assets/cutscene/stars.png");
  Texture* planet = TextureManager::instance().get("assets/cutscene/planet.png");
  Texture* ship = TextureManager::instance().get("assets/cutscene/ship.png");
  const unsigned int glow = introGlow();

  // Oversized: the shake below must never show an edge.
  dl->AddRectFilled(ImVec2(-100.0f, -100.0f),
                    ImVec2(SCREEN_W + 100.0f, SCREEN_H + 100.0f),
                    IM_COL32(0, 0, 0, 255));
  // The sky drifts very slowly; the shot holds still otherwise.
  if (stars) {
    float drift = e / I::CRASH;
    ImVec2 a(-80.0f - 24.0f * drift, -45.0f - 8.0f * drift);
    dl->AddImage(tex(stars->getID()), a,
                 ImVec2(a.x + stars->getWidth(), a.y + stars->getHeight()));
  }
  if (planet) {
    float half = PLANET_DRAW * 0.5f;
    dl->AddImage(tex(planet->getID()),
                 ImVec2(PLANET_CENTER.x - half, PLANET_CENTER.y - half),
                 ImVec2(PLANET_CENTER.x + half, PLANET_CENTER.y + half));
  }

  const ImVec2 tailDir = tailDirection();
  const float flown = std::min(e, I::SHIP_IMPACT);
  if (e >= I::SHIP_ENTER) {
    // Fire and smoke. Each particle is left in space where the ship was
    // when it came off the hull, so the trail streams back toward the
    // camera in perspective as the ship falls away.
    struct Puff {
      ImVec2 at;
      float radius, life;  // life 0 (fresh) .. 1 (gone)
      float heat;          // 1 white-hot .. 0 smoke
    };
    std::vector<Puff> fire, smoke;
    int first = 0;
    int last = static_cast<int>((flown - I::SHIP_ENTER) * FLAME_RATE);
    for (int k = first; k <= last; k++) {
      float born = I::SHIP_ENTER + k / FLAME_RATE;
      float lifetime = 0.55f + 0.7f * hash(k * 7U + 1U);
      float age = e - born;
      if (age < 0.0f || age > lifetime * 1.6f) continue;
      float depth = shipDepth(born);
      // Re-entry: the closer to the planet, the more it burns.
      float burn = smoothstep(0.45f, 1.0f, (born - I::SHIP_ENTER) /
                                               (I::SHIP_IMPACT - I::SHIP_ENTER));
      // Most stream from the boosters; one in three off the burning hull.
      const HullFire& spot = HULL_FIRES[k % HULL_FIRE_COUNT];
      ImVec2 tail = k % 3 == 0 ? shipPoint(depth, spot.along, spot.side)
                               : shipTail(depth);
      ImVec2 offset((tail.x - SHIP_TARGET.x) * depth,
                    (tail.y - SHIP_TARGET.y) * depth);
      // Blown back toward the camera, and spreading.
      float angle = hash(k * 13U + 5U) * 6.2831853f;
      float spread = (50.0f + 90.0f * burn) * age * (0.4f + hash(k * 3U + 2U));
      offset.x += std::cos(angle) * spread + tailDir.x * 160.0f * age;
      offset.y += std::sin(angle) * spread + tailDir.y * 160.0f * age;
      float near = depth * (1.0f - 0.12f * std::min(age, 1.0f));
      ImVec2 at = project(offset, near);
      float size = (34.0f + 80.0f * age) * (0.6f + 0.8f * hash(k * 11U + 9U)) *
                   (1.0f + 1.6f * burn) / near;
      float t = age / lifetime;
      if (t < 1.0f) {
        fire.push_back({at, std::max(size, 2.2f), t, 1.0f - t});
      }
      if (t > 0.35f) {
        float s = (t - 0.35f) / 1.25f;
        smoke.push_back({at, std::max(size * (1.4f + 1.6f * s), 2.5f),
                         std::min(s, 1.0f), 0.0f});
      }
    }
    for (const Puff& p : smoke) {
      float a = 0.32f * (1.0f - p.life) * smoothstep(0.0f, 0.15f, p.life);
      dl->AddImage(tex(glow), ImVec2(p.at.x - p.radius, p.at.y - p.radius),
                   ImVec2(p.at.x + p.radius, p.at.y + p.radius), ImVec2(0, 0),
                   ImVec2(1, 1), rgba(0.10f, 0.09f, 0.11f, a));
    }

    // The ship, until it hits.
    if (ship && e < I::SHIP_IMPACT) {
      float depth = shipDepth(e);
      int frame = shipFrame(depth);
      ImVec2 centre = project(ImVec2(SHIP_NEAR.x - SHIP_TARGET.x,
                                     SHIP_NEAR.y - SHIP_TARGET.y),
                              depth);
      float size = SHIP_SIZE / depth;
      // Tail along tailDir (the cell's up), tumbling a little.
      float wobble = 0.07f * std::sin(e * 2.3f) + 0.04f * std::sin(e * 5.1f);
      float c = std::cos(wobble), s = std::sin(wobble);
      ImVec2 up(tailDir.x * c - tailDir.y * s, tailDir.x * s + tailDir.y * c);
      ImVec2 right(-up.y, up.x);
      float h = size * 0.5f;
      auto corner = [&](float rx, float uy) {
        return ImVec2(centre.x + (right.x * rx + up.x * uy) * h,
                      centre.y + (right.y * rx + up.y * uy) * h);
      };
      int rows = (SHIP_FRAMES + SHIP_COLUMNS - 1) / SHIP_COLUMNS;
      float cw = 1.0f / SHIP_COLUMNS, ch = 1.0f / rows;
      float u0 = (frame % SHIP_COLUMNS) * cw, v0 = (frame / SHIP_COLUMNS) * ch;
      // A warm glow behind the hull: the fire lighting the smoke.
      float hot = smoothstep(I::SHIP_ENTER + 0.45f * (I::SHIP_IMPACT - I::SHIP_ENTER),
                             I::SHIP_IMPACT, e);
      float g = size * (0.75f + 0.6f * hot);
      additive(dl);
      dl->AddImage(tex(glow), ImVec2(centre.x - g, centre.y - g),
                   ImVec2(centre.x + g, centre.y + g), ImVec2(0, 0),
                   ImVec2(1, 1), rgba(1.0f, 0.4f, 0.12f, 0.18f + 0.4f * hot));
      normalBlend(dl);
      // The quad's corners: top of the cell is the tail.
      dl->AddImageQuad(tex(ship->getID()), corner(-1, 1), corner(1, 1),
                       corner(1, -1), corner(-1, -1), ImVec2(u0, v0),
                       ImVec2(u0 + cw, v0), ImVec2(u0 + cw, v0 + ch),
                       ImVec2(u0, v0 + ch), IM_COL32(255, 255, 255, 255));
      // The hull is on fire: flickering tongues at each burning spot, blown
      // back toward the tail.
      additive(dl);
      for (int j = 0; j < HULL_FIRE_COUNT; j++) {
        const HullFire& spot = HULL_FIRES[j];
        // A tongue: a hot core, then smaller, redder puffs trailing back.
        for (int t = 0; t < 6; t++) {
          float flicker = 0.7f + 0.3f * std::sin(e * 23.0f + j * 2.1f + t * 1.7f);
          float back = (0.02f + 0.032f * t) * spot.size * flicker;
          float wave = 0.006f * std::sin(e * 31.0f + j + t * 0.9f) * t;
          ImVec2 at = shipPoint(depth, spot.along + back, spot.side + wave);
          float r = size * 0.06f * spot.size * (1.0f - 0.13f * t) * flicker;
          r = std::max(r, 1.2f);
          float heat = 1.0f - t / 6.0f;
          dl->AddImage(tex(glow), ImVec2(at.x - r, at.y - r),
                       ImVec2(at.x + r, at.y + r), ImVec2(0, 0), ImVec2(1, 1),
                       rgba(1.0f, 0.3f + 0.6f * heat * heat, 0.1f + 0.4f * heat * heat * heat,
                            0.55f + 0.35f * heat));
        }
      }
      normalBlend(dl);
      // Heating up in the air: the hull glows over.
      if (hot > 0.0f) {
        additive(dl);
        float r = size * 0.45f;
        dl->AddImage(tex(glow), ImVec2(centre.x - r, centre.y - r),
                     ImVec2(centre.x + r, centre.y + r), ImVec2(0, 0),
                     ImVec2(1, 1), rgba(1.0f, 0.6f, 0.3f, 0.8f * hot));
        normalBlend(dl);
      }
    }

    additive(dl);
    for (const Puff& p : fire) {
      // White-hot, to orange, to a dull red.
      float t = p.life;
      float r = 1.0f;
      float gr = t < 0.3f ? 0.95f - 1.5f * t : std::max(0.15f, 0.5f - (t - 0.3f));
      float b = t < 0.2f ? 0.75f - 3.0f * t : 0.1f;
      float a = (1.0f - t) * (t < 0.08f ? t / 0.08f : 1.0f);
      dl->AddImage(tex(glow), ImVec2(p.at.x - p.radius, p.at.y - p.radius),
                   ImVec2(p.at.x + p.radius, p.at.y + p.radius), ImVec2(0, 0),
                   ImVec2(1, 1), rgba(r, gr, b, a));
    }
    // The impact: a flash where it went in, swelling until the cut.
    if (e >= I::SHIP_IMPACT - 0.05f) {
      float f = smoothstep(I::SHIP_IMPACT - 0.05f, I::CRASH, e);
      float r = 20.0f + 260.0f * f;
      dl->AddImage(tex(glow), ImVec2(SHIP_TARGET.x - r, SHIP_TARGET.y - r),
                   ImVec2(SHIP_TARGET.x + r, SHIP_TARGET.y + r), ImVec2(0, 0),
                   ImVec2(1, 1), rgba(1.0f, 0.85f, 0.6f, 0.9f));
      float c = r * 0.35f;
      dl->AddImage(tex(glow), ImVec2(SHIP_TARGET.x - c, SHIP_TARGET.y - c),
                   ImVec2(SHIP_TARGET.x + c, SHIP_TARGET.y + c), ImVec2(0, 0),
                   ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
    }
    normalBlend(dl);
  }

  // Fade up from black at the start.
  float black = 1.0f - smoothstep(0.0f, I::FADE_UP, e);
  if (black > 0.0f) {
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(SCREEN_W, SCREEN_H),
                      rgba(0, 0, 0, black));
  }
  // Letterbox: this is a cutscene, not play.
  dl->AddRectFilled(ImVec2(0, 0), ImVec2(SCREEN_W, 54.0f),
                    IM_COL32(0, 0, 0, 255));
  dl->AddRectFilled(ImVec2(0, SCREEN_H - 54.0f), ImVec2(SCREEN_W, SCREEN_H),
                    IM_COL32(0, 0, 0, 255));

  // The camera: a slow push in on the planet, a jolt as the ship tears
  // past, and a rumble building to the crash.
  float zoom = 1.0f + 0.07f * smoothstep(0.0f, I::CRASH, e);
  float jolt = 9.0f * std::exp(-std::pow((e - I::SHIP_PASS) / 0.22f, 2.0f));
  float rumble = 4.0f * smoothstep(I::SHIP_IMPACT - 1.8f, I::CRASH, e);
  float amount = jolt + rumble;
  ImVec2 shake(amount * std::sin(e * 61.0f) * std::cos(e * 23.0f),
               amount * std::sin(e * 47.0f + 1.3f));
  const float cx = SCREEN_W * 0.5f, cy = SCREEN_H * 0.5f;
  // The letterbox bars are the last 8 vertices: they stay put.
  const int bars = dl->VtxBuffer.Size - 8;
  for (int i = firstVertex; i < bars; i++) {
    ImDrawVert& v = dl->VtxBuffer[i];
    v.pos.x = cx + (v.pos.x - cx) * zoom + shake.x;
    v.pos.y = cy + (v.pos.y - cy) * zoom + shake.y;
  }
}

void ArenaView::drawIntroSkip(ImDrawList* dl, float e, float dt) {
  if (e >= I::SKIP_TO) return;
  // Hold Space to skip, for everyone (MatchDirector::onReady).
  bool held = ImGui::IsKeyDown(ImGuiKey_Space) && !ImGui::GetIO().WantTextInput;
  introSkipHeld = held ? introSkipHeld + dt : 0.0f;
  if (introSkipHeld >= SKIP_HOLD && !introSkipSent) {
    PlayerReadyPacket packet;
    packet.ready = 1;
    prediction->sendToServer(serialize(packet));
    introSkipSent = true;
  }
  float show = smoothstep(2.0f, 3.0f, e) * (held ? 1.0f : 0.55f);
  if (show <= 0.0f) return;
  const char* label = "Hold SPACE to skip";
  ImFont* font = ImGui::GetFont();
  const float size = 16.0f;
  ImVec2 extent = font->CalcTextSizeA(size, 1000.0f, 0.0f, label);
  ImVec2 at(SCREEN_W - extent.x - 28.0f, SCREEN_H - 38.0f);
  dl->AddText(font, size, at, rgba(0.85f, 0.88f, 0.95f, show), label);
  float fill = std::min(1.0f, introSkipHeld / SKIP_HOLD);
  ImVec2 bar(at.x, at.y + extent.y + 4.0f);
  dl->AddRectFilled(bar, ImVec2(bar.x + extent.x, bar.y + 2.0f),
                    rgba(1, 1, 1, 0.15f * show));
  if (fill > 0.0f) {
    dl->AddRectFilled(bar, ImVec2(bar.x + extent.x * fill, bar.y + 2.0f),
                      rgba(1.0f, 0.7f, 0.35f, show));
  }
}
