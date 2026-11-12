#include "EffectVisuals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using ArenaDraw::PI;
using ArenaDraw::Rgb;
using ArenaDraw::rgba;

namespace {

Rgb mix(Rgb a, Rgb b, float t) {
  t = std::max(0.0f, std::min(1.0f, t));
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
Rgb dark(Rgb c, float f) { return {c.r * f, c.g * f, c.b * f}; }
const Rgb WHITE{1.0f, 1.0f, 1.0f};

float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
float easeOut(float t) {
  t = clamp01(t);
  return 1.0f - (1.0f - t) * (1.0f - t);
}

// Small, stable pseudo-random numbers so particles need no state.
uint32_t hash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}
float rand01(uint32_t seed) { return (hash(seed) & 0xFFFF) / 65535.0f; }

uint64_t trackKey(uint32_t id, bool isEnemy, EffectType type) {
  return (static_cast<uint64_t>(isEnemy) << 63) |
         (static_cast<uint64_t>(id) << 8) | static_cast<uint8_t>(type);
}

// A pixel: snapped square so particles match the sprite art.
void pixel(ImDrawList *dl, float x, float y, float size, ImU32 color) {
  x = std::floor(x);
  y = std::floor(y);
  dl->AddRectFilled(ImVec2(x, y), ImVec2(x + size, y + size), color);
}

ImVec2 bezier(ImVec2 a, ImVec2 c, ImVec2 b, float t) {
  float u = 1.0f - t;
  return ImVec2(u * u * a.x + 2 * u * t * c.x + t * t * b.x,
                u * u * a.y + 2 * u * t * c.y + t * t * b.y);
}

const char *shortName(EffectType type) { return EffectRegistry::getName(type); }

constexpr float BODY_MID_ENEMY = 20.0f;
constexpr float BODY_MID_PLAYER = 36.0f;

} // namespace

// ---------------------------------------------------------------------------
// The look of each effect
// ---------------------------------------------------------------------------

EffectVisuals::Look EffectVisuals::look(EffectType type) {
  switch (type) {
  case EffectType::Poison:
    return {{0.46f, 0.88f, 0.26f},
            {0.85f, 1.0f, 0.45f},
            Motif::Bubbles,
            Glyph::Droplet,
            30.0f};
  case EffectType::Wound:
    return {{0.86f, 0.18f, 0.22f},
            {1.0f, 0.55f, 0.5f},
            Motif::Drips,
            Glyph::Droplet,
            10.0f};
  case EffectType::Mend:
    return {{0.40f, 0.95f, 0.66f},
            {0.80f, 1.0f, 0.88f},
            Motif::Motes,
            Glyph::Cross,
            10.0f};
  case EffectType::Empowered:
    return {{1.0f, 0.74f, 0.18f},
            {1.0f, 0.95f, 0.62f},
            Motif::Sparks,
            Glyph::ArrowUp,
            5.0f};
  case EffectType::Weakened:
    return {{0.58f, 0.34f, 0.92f},
            {0.84f, 0.66f, 1.0f},
            Motif::Wisps,
            Glyph::ArrowDown,
            5.0f};
  case EffectType::Fortified:
    return {{0.52f, 0.72f, 1.0f},
            {0.86f, 0.93f, 1.0f},
            Motif::Plates,
            Glyph::Shield,
            5.0f};
  case EffectType::Vulnerable:
    return {{0.95f, 0.32f, 0.58f},
            {1.0f, 0.72f, 0.84f},
            Motif::Wisps,
            Glyph::CrackedShield,
            5.0f};
  case EffectType::Haste:
    return {{0.36f, 0.90f, 1.0f},
            {0.80f, 1.0f, 1.0f},
            Motif::Streaks,
            Glyph::Chevrons,
            4.0f};
  case EffectType::Slow:
    return {{0.62f, 0.80f, 1.0f},
            {0.92f, 0.97f, 1.0f},
            Motif::Frost,
            Glyph::Snowflake,
            4.0f};
  case EffectType::Stunned:
  case EffectType::Snared:
    return {{1.0f, 0.86f, 0.30f},
            {1.0f, 1.0f, 0.75f},
            Motif::None,
            Glyph::Star,
            1.0f};
  case EffectType::Confused:
    return {{0.95f, 0.40f, 0.95f},
            {1.0f, 0.75f, 1.0f},
            Motif::None,
            Glyph::Letter,
            1.0f};
  case EffectType::Guard:
  case EffectType::Blessed:
    return {{0.95f, 0.90f, 0.70f},
            {1.0f, 1.0f, 0.9f},
            Motif::Plates,
            Glyph::Shield,
            5.0f};
  default:
    break;
  }
  bool buff = EffectRegistry::getCategory(type) == EffectCategory::Buff;
  return buff ? Look{{0.95f, 0.85f, 0.45f},
                     {1.0f, 0.97f, 0.8f},
                     Motif::None,
                     Glyph::Letter,
                     5.0f}
              : Look{{0.78f, 0.55f, 0.95f},
                     {0.92f, 0.82f, 1.0f},
                     Motif::None,
                     Glyph::Letter,
                     5.0f};
}

float EffectVisuals::intensity(EffectType type, float stacks) const {
  float full = look(type).fullStacks;
  if (stacks <= 0.0f)
    return 0.0f;
  return clamp01(std::log(1.0f + stacks) / std::log(1.0f + full));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

EffectVisuals::EffectVisuals() {
  EventBus::instance().subscribe<NetworkPacketReceivedEvent>(
      [this](const NetworkPacketReceivedEvent &e) { onPacket(e); });
  EventBus::instance().subscribe<UpdateEvent>(
      [this](const UpdateEvent &e) { onUpdate(e.deltaTime); });
}

void EffectVisuals::beginFrame(float timeSeconds) {
  frameDt = now > 0.0f ? std::min(0.1f, timeSeconds - now) : 0.0f;
  now = timeSeconds;
  // Forget tracks for entities we have not drawn for a while.
  for (auto it = tracks.begin(); it != tracks.end();) {
    if (now - it->second.seenAt > 2.0f) {
      it = tracks.erase(it);
    } else {
      ++it;
    }
  }
}

void EffectVisuals::onPacket(const NetworkPacketReceivedEvent &e) {
  if (e.size == 0 || e.clientId != 0)
    return;
  if (static_cast<PacketType>(e.data[0]) != PacketType::GameEvent)
    return;
  GameEventPacket p = deserializeGameEvent(e.data, e.size);
  if (p.kind == GameEventKind::EffectVerb)
    addVerb(p);
}

void EffectVisuals::onUpdate(float dtMs) {
  for (Verb &v : verbs)
    v.ageMs += dtMs;
  verbs.erase(std::remove_if(
                  verbs.begin(), verbs.end(),
                  [](const Verb &v) { return v.ageMs > v.delayMs + v.lifeMs; }),
              verbs.end());
  for (Callout &c : callouts)
    c.ageMs += dtMs;
  callouts.erase(std::remove_if(callouts.begin(), callouts.end(),
                                [](const Callout &c) {
                                  return c.ageMs > c.delayMs + c.lifeMs;
                                }),
                 callouts.end());
  for (Mark &m : marks)
    m.ageMs += dtMs;
  marks.erase(std::remove_if(marks.begin(), marks.end(),
                             [](const Mark &m) { return m.ageMs > m.lifeMs; }),
              marks.end());
  for (auto &[key, t] : tracks) {
    t.applyMs = std::max(0.0f, t.applyMs - dtMs);
    t.popMs = std::max(0.0f, t.popMs - dtMs);
  }
  shakeMs = std::max(0.0f, shakeMs - dtMs);
}

ImVec2 EffectVisuals::shakeOffset() const {
  if (shakeMs <= 0.0f)
    return ImVec2(0, 0);
  float a = shakeAmp * (shakeMs / 220.0f);
  return ImVec2(std::floor(std::sin(now * 91.0f) * a),
                std::floor(std::cos(now * 77.0f) * a * 0.7f));
}

EffectVisuals::Callout &EffectVisuals::callout(uint64_t key, float x, float y) {
  for (Callout &c : callouts) {
    if (c.key == key && c.ageMs < c.delayMs + c.lifeMs * 0.7f) {
      c.x = x;
      c.y = y;
      return c;
    }
  }
  Callout c;
  c.key = key;
  c.x = x;
  c.y = y;
  callouts.push_back(c);
  return callouts.back();
}

void EffectVisuals::addVerb(const GameEventPacket &p) {
  Verb v;
  v.verb = static_cast<EffectVerb>(p.param);
  v.type = static_cast<EffectType>(p.param2);
  v.target = p.sourceId;
  v.isEnemy = p.sourceIsEnemy;
  v.x = p.x;
  v.y = p.y;
  v.x2 = p.x2;
  v.y2 = p.y2;
  v.before = p.value;
  v.after = p.value2;
  v.seed = hash(p.sourceId * 2654435761U + static_cast<uint32_t>(verbs.size()));
  Look lk = look(v.type);
  char line[96];

  switch (v.verb) {
  case EffectVerb::Apply: {
    v.from = p.extraId;
    float d = std::hypot(v.x - v.x2, v.y - v.y2);
    v.lifeMs = 160.0f + d * 0.25f + 450.0f;
    break;
  }
  case EffectVerb::Amplify: {
    v.lifeMs = 950.0f;
    Callout &c =
        callout(trackKey(v.target, v.isEnemy, v.type) ^ 0xA0, v.x, v.y);
    std::snprintf(line, sizeof(line), "x%d  >>  x%d",
                  static_cast<int>(v.before), static_cast<int>(v.after));
    c.title = line;
    c.color = lk.hot;
    c.size = 15.0f + 6.0f * intensity(v.type, v.after);
    c.lifeMs = 1000.0f;
    c.delayMs = 120.0f;
    break;
  }
  case EffectVerb::Spread: {
    v.from = p.extraId;
    float d = std::hypot(v.x - v.x2, v.y - v.y2);
    // Ripple outward: the farther the target, the later it arrives.
    v.delayMs = d * 0.9f;
    v.lifeMs = 170.0f + 420.0f;
    Callout &c =
        callout((static_cast<uint64_t>(v.from) << 8) ^ 0x5B, v.x2, v.y2);
    c.count++;
    std::snprintf(line, sizeof(line), "SPREAD x%d", c.count);
    c.title = line;
    std::snprintf(line, sizeof(line), "%s x%d", shortName(v.type),
                  static_cast<int>(v.after - v.before));
    c.detail = line;
    c.color = lk.hot;
    c.detailColor = lk.base;
    c.size = std::min(26.0f, 15.0f + c.count * 0.6f);
    c.lifeMs = 1300.0f;
    break;
  }
  case EffectVerb::Detonate: {
    v.lifeMs = 760.0f;
    float radius = p.value2;
    marks.push_back(
        {v.x, v.y, radius * 0.75f, 0.0f, 1800.0f, dark(lk.base, 0.45f)});
    float amp = std::min(7.0f, 1.5f + v.before * 0.12f);
    if (amp >= shakeAmp * (shakeMs / 220.0f)) {
      shakeAmp = amp;
      shakeMs = 220.0f;
    }
    break;
  }
  case EffectVerb::Corrupt:
  case EffectVerb::Purify: {
    v.was = static_cast<EffectType>(p.extraId);
    v.lifeMs = 1150.0f;
    bool corrupt = v.verb == EffectVerb::Corrupt;
    Callout &c = callout((static_cast<uint64_t>(v.target) << 9) ^
                             (corrupt ? 0xC0u : 0xD0u),
                         v.x, v.y);
    c.title = corrupt ? "CORRUPTED" : "PURIFIED";
    c.color = lk.hot;
    std::snprintf(line, sizeof(line), "%s%s x%d > %s",
                  c.detail.empty() ? "" : "\n", shortName(v.was),
                  static_cast<int>(v.before), shortName(v.type));
    c.detail += line;
    c.detailColor = mix(lk.base, WHITE, 0.3f);
    c.size = 17.0f + 2.0f * std::min(5.0f, v.before);
    c.lifeMs = 1500.0f;
    c.delayMs = 220.0f;
    if (corrupt) {
      shakeAmp = std::max(shakeAmp, 2.5f + v.before * 0.4f);
      shakeMs = 200.0f;
    }
    break;
  }
  }
  verbs.push_back(v);
}

EffectVisuals::Track &EffectVisuals::track(uint32_t id, bool isEnemy,
                                           EffectType type) {
  auto [it, fresh] = tracks.try_emplace(trackKey(id, isEnemy, type));
  Track &t = it->second;
  if (fresh)
    t.applyMs = 420.0f;
  t.seenAt = now;
  return t;
}

// ---------------------------------------------------------------------------
// Steady state: auras, tint, particles
// ---------------------------------------------------------------------------

ImU32 EffectVisuals::bodyTint(
    const std::vector<EffectInstance> &effects) const {
  // The strongest debuff colours the body; buffs show as glow instead.
  float best = 0.0f;
  Rgb colour = WHITE;
  for (const EffectInstance &e : effects) {
    if (EffectRegistry::getCategory(e.type) != EffectCategory::Debuff)
      continue;
    Look lk = look(e.type);
    if (lk.motif == Motif::None)
      continue;
    float i = intensity(e.type, e.stacks);
    if (i > best) {
      best = i;
      colour = mix(WHITE, mix(lk.base, WHITE, 0.45f), 0.12f + 0.38f * i);
    }
  }
  return rgba(colour, 1.0f);
}

void EffectVisuals::drawUnder(ImDrawList *dl, uint32_t id, bool isEnemy,
                              ImVec2 feet, float radius,
                              const std::vector<EffectInstance> &effects) {
  // At most one aura per side, so a pack stays readable.
  const EffectInstance *top[2] = {nullptr, nullptr}; // [0] debuff, [1] buff
  float topI[2] = {0.0f, 0.0f};
  for (const EffectInstance &e : effects) {
    Track &t = track(id, isEnemy, e.type);
    // Displayed stacks ease toward the real count so jumps read as growth.
    float target = static_cast<float>(e.stacks);
    t.shown += (target - t.shown) * std::min(1.0f, frameDt * 7.0f);
    if (std::abs(target - t.shown) < 0.05f)
      t.shown = target;
    static const int THRESHOLDS[] = {5, 10, 20, 40};
    for (int th : THRESHOLDS) {
      if (t.last < th && e.stacks >= th)
        t.popMs = 520.0f;
    }
    t.last = e.stacks;

    Look lk = look(e.type);
    if (lk.motif == Motif::None)
      continue;
    int side =
        EffectRegistry::getCategory(e.type) == EffectCategory::Buff ? 1 : 0;
    float i = intensity(e.type, t.shown);
    if (i > topI[side]) {
      topI[side] = i;
      top[side] = &e;
    }
  }

  for (int side = 0; side < 2; side++) {
    if (!top[side])
      continue;
    EffectType type = top[side]->type;
    Look lk = look(type);
    Track &t = track(id, isEnemy, type);
    float i = topI[side];
    float pulse =
        0.5f + 0.5f * std::sin(now * (2.0f + 4.0f * i) + (id % 7) * 0.9f);
    float r = radius * (1.15f + 0.9f * i) + pulse * 2.0f * i;
    if (t.popMs > 0.0f)
      r += 10.0f * std::sin(t.popMs / 520.0f * PI);
    ImVec2 size(r, r * 0.42f);
    dl->AddEllipseFilled(feet, size, rgba(lk.base, 0.10f + 0.24f * i), 0.0f,
                         28);
    if (side == 1) {
      // Buffs: a turning ring of bright dashes.
      int dashes = 10;
      for (int k = 0; k < dashes; k++) {
        float a0 = now * 1.4f + k * 2.0f * PI / dashes;
        float a1 = a0 + PI / dashes;
        dl->PathClear();
        for (int s = 0; s <= 4; s++) {
          float a = a0 + (a1 - a0) * s / 4.0f;
          dl->PathLineTo(ImVec2(feet.x + std::cos(a) * size.x,
                                feet.y + std::sin(a) * size.y));
        }
        dl->PathStroke(rgba(lk.hot, 0.35f + 0.5f * i), 0, 1.5f + 1.5f * i);
      }
    } else {
      // Debuffs: a wobbling, uneven ring.
      dl->PathClear();
      for (int s = 0; s <= 28; s++) {
        float a = s * 2.0f * PI / 28.0f;
        float wob = 1.0f + 0.08f * i * std::sin(a * 5.0f + now * 6.0f);
        dl->PathLineTo(ImVec2(feet.x + std::cos(a) * size.x * wob,
                              feet.y + std::sin(a) * size.y * wob));
      }
      dl->PathStroke(rgba(lk.base, 0.30f + 0.5f * i), 0, 1.5f + 1.5f * i);
    }
  }

  // Sigils for anything that just landed, and pops at thresholds.
  for (const EffectInstance &e : effects) {
    Track &t = track(id, isEnemy, e.type);
    Look lk = look(e.type);
    if (t.applyMs > 0.0f) {
      float k = 1.0f - t.applyMs / 420.0f;
      float r = radius * (2.2f - 1.0f * easeOut(k));
      for (int s = 0; s < 6; s++) {
        float a = s * PI / 3.0f + k * 2.0f;
        pixel(dl, feet.x + std::cos(a) * r - 1.5f,
              feet.y + std::sin(a) * r * 0.42f - 1.5f, 3.0f,
              rgba(lk.hot, 1.0f - k));
      }
      dl->AddEllipse(feet, ImVec2(r, r * 0.42f), rgba(lk.base, 0.8f * (1 - k)),
                     0.0f, 24, 1.5f);
    }
    if (t.popMs > 0.0f) {
      float k = 1.0f - t.popMs / 520.0f;
      float r = radius * (1.2f + 2.4f * easeOut(k));
      dl->AddEllipse(feet, ImVec2(r, r * 0.42f), rgba(lk.hot, 1.0f - k), 0.0f,
                     32, 3.0f * (1.0f - k) + 1.0f);
    }
  }
}

void EffectVisuals::drawOver(ImDrawList *dl, uint32_t id, bool isEnemy,
                             ImVec2 feet, float bodyHeight,
                             const std::vector<EffectInstance> &effects) {
  for (const EffectInstance &e : effects) {
    Look lk = look(e.type);
    if (lk.motif == Motif::None)
      continue;
    auto it = tracks.find(trackKey(id, isEnemy, e.type));
    float shown = it != tracks.end() ? it->second.shown : e.stacks;
    float i = intensity(e.type, shown);
    int count = 2 + static_cast<int>(10.0f * i);
    float w = bodyHeight * 0.55f;
    uint32_t base = id * 977u + static_cast<uint32_t>(e.type) * 131u +
                    (isEnemy ? 0x9E37u : 0u);
    for (int k = 0; k < count; k++) {
      uint32_t s = base + k * 7919u;
      float speed = 0.6f + rand01(s + 1) * 0.8f + i * 0.6f;
      float phase = std::fmod(now * speed + rand01(s + 2), 1.0f);
      float fade = std::sin(phase * PI);
      float ox = (rand01(s + 3) - 0.5f) * w;
      float px = feet.x + ox, py = feet.y;
      float size = 2.0f + (i > 0.6f && (k % 3 == 0) ? 1.0f : 0.0f);
      ImU32 colour = rgba(k % 3 == 0 ? lk.hot : lk.base, 0.25f + 0.75f * fade);
      switch (lk.motif) {
      case Motif::Bubbles: {
        // Rise from the body and pop at the top.
        py = feet.y - bodyHeight * (0.2f + 0.95f * phase);
        px += std::sin(now * 3.0f + k) * 2.0f;
        if (phase > 0.86f) {
          dl->AddCircle(ImVec2(px, py), 2.0f + (phase - 0.86f) * 30.0f,
                        rgba(lk.hot, (1.0f - phase) * 6.0f), 8, 1.0f);
        } else {
          dl->AddCircle(ImVec2(px, py), size * 0.9f, colour, 8, 1.2f);
        }
        continue;
      }
      case Motif::Drips:
        py = feet.y - bodyHeight * (0.65f - 0.7f * phase * phase);
        break;
      case Motif::Sparks:
        py = feet.y - bodyHeight * (0.1f + 1.2f * phase);
        px += std::sin(phase * 9.0f + k) * 3.0f;
        break;
      case Motif::Wisps:
        py = feet.y - bodyHeight * (1.05f - 0.9f * phase);
        px += std::sin(phase * 5.0f + k) * 5.0f;
        colour = rgba(lk.base, 0.15f + 0.6f * fade);
        break;
      case Motif::Frost: {
        float a = now * 1.2f + k * 2.0f * PI / count;
        px = feet.x + std::cos(a) * w * 0.8f;
        py = feet.y - 6.0f + std::sin(a) * w * 0.3f;
        break;
      }
      case Motif::Streaks: {
        float len = 6.0f + 10.0f * i;
        py = feet.y - bodyHeight * (0.2f + 0.6f * rand01(s + 4));
        dl->AddLine(ImVec2(px - len * phase, py),
                    ImVec2(px - len * phase - len, py), colour, 1.5f);
        continue;
      }
      case Motif::Motes:
        py = feet.y - bodyHeight * (0.1f + 1.0f * phase);
        break;
      case Motif::Plates: {
        float a = now * 1.8f + k * 2.0f * PI / count;
        px = feet.x + std::cos(a) * w * 0.7f;
        py = feet.y - bodyHeight * 0.5f + std::sin(a) * w * 0.25f;
        size = 3.0f;
        colour = rgba(lk.hot, 0.35f + 0.5f * i);
        break;
      }
      case Motif::None:
        continue;
      }
      pixel(dl, px, py, size, colour);
    }
    // Strong buffs: a soft glow around the chest.
    if (EffectRegistry::getCategory(e.type) == EffectCategory::Buff &&
        i > 0.3f) {
      float glow = 0.5f + 0.5f * std::sin(now * 4.0f);
      dl->AddCircle(ImVec2(feet.x, feet.y - bodyHeight * 0.55f),
                    bodyHeight * (0.35f + 0.1f * glow),
                    rgba(lk.hot, 0.12f + 0.25f * i), 24, 2.0f + 2.0f * i);
    }
  }
}

// ---------------------------------------------------------------------------
// Badges
// ---------------------------------------------------------------------------

namespace {

void drawGlyph(ImDrawList *dl, EffectVisuals::Glyph glyph, ImVec2 c, float s,
               ImU32 col, EffectType type) {
  using G = EffectVisuals::Glyph;
  switch (glyph) {
  case G::Droplet:
    dl->AddCircleFilled(ImVec2(c.x, c.y + s * 0.25f), s * 0.5f, col, 10);
    dl->AddTriangleFilled(ImVec2(c.x, c.y - s * 0.75f),
                          ImVec2(c.x - s * 0.48f, c.y + s * 0.1f),
                          ImVec2(c.x + s * 0.48f, c.y + s * 0.1f), col);
    break;
  case G::ArrowUp:
  case G::ArrowDown: {
    float d = glyph == G::ArrowUp ? -1.0f : 1.0f;
    dl->AddTriangleFilled(ImVec2(c.x, c.y + d * s * 0.8f),
                          ImVec2(c.x - s * 0.7f, c.y),
                          ImVec2(c.x + s * 0.7f, c.y), col);
    dl->AddRectFilled(ImVec2(c.x - s * 0.25f, c.y - (d > 0 ? s * 0.7f : 0)),
                      ImVec2(c.x + s * 0.25f, c.y + (d > 0 ? 0 : s * 0.7f)),
                      col);
    break;
  }
  case G::Shield:
  case G::CrackedShield: {
    ImVec2 pts[5] = {ImVec2(c.x - s * 0.65f, c.y - s * 0.7f),
                     ImVec2(c.x + s * 0.65f, c.y - s * 0.7f),
                     ImVec2(c.x + s * 0.6f, c.y + s * 0.05f),
                     ImVec2(c.x, c.y + s * 0.8f),
                     ImVec2(c.x - s * 0.6f, c.y + s * 0.05f)};
    dl->AddConvexPolyFilled(pts, 5, col);
    if (glyph == G::CrackedShield) {
      dl->AddLine(ImVec2(c.x - s * 0.1f, c.y - s * 0.7f),
                  ImVec2(c.x + s * 0.15f, c.y), IM_COL32(20, 10, 20, 255),
                  1.5f);
      dl->AddLine(ImVec2(c.x + s * 0.15f, c.y),
                  ImVec2(c.x - s * 0.1f, c.y + s * 0.6f),
                  IM_COL32(20, 10, 20, 255), 1.5f);
    }
    break;
  }
  case G::Cross:
    dl->AddRectFilled(ImVec2(c.x - s * 0.22f, c.y - s * 0.7f),
                      ImVec2(c.x + s * 0.22f, c.y + s * 0.7f), col);
    dl->AddRectFilled(ImVec2(c.x - s * 0.7f, c.y - s * 0.22f),
                      ImVec2(c.x + s * 0.7f, c.y + s * 0.22f), col);
    break;
  case G::Snowflake:
    for (int k = 0; k < 3; k++) {
      float a = k * PI / 3.0f + PI / 2.0f;
      dl->AddLine(
          ImVec2(c.x - std::cos(a) * s * 0.8f, c.y - std::sin(a) * s * 0.8f),
          ImVec2(c.x + std::cos(a) * s * 0.8f, c.y + std::sin(a) * s * 0.8f),
          col, 1.5f);
    }
    break;
  case G::Chevrons:
    for (int k = 0; k < 2; k++) {
      float ox = -s * 0.35f + k * s * 0.55f;
      dl->AddLine(ImVec2(c.x + ox - s * 0.3f, c.y - s * 0.6f),
                  ImVec2(c.x + ox + s * 0.2f, c.y), col, 2.0f);
      dl->AddLine(ImVec2(c.x + ox + s * 0.2f, c.y),
                  ImVec2(c.x + ox - s * 0.3f, c.y + s * 0.6f), col, 2.0f);
    }
    break;
  case G::Star:
    for (int k = 0; k < 4; k++) {
      float a = k * PI / 4.0f;
      dl->AddLine(
          ImVec2(c.x - std::cos(a) * s * 0.8f, c.y - std::sin(a) * s * 0.8f),
          ImVec2(c.x + std::cos(a) * s * 0.8f, c.y + std::sin(a) * s * 0.8f),
          col, 1.5f);
    }
    break;
  case G::Letter: {
    char letter[2] = {EffectRegistry::getName(type)[0], 0};
    ArenaDraw::text(dl, ImVec2(c.x, c.y - 6.0f), col, 11.0f, letter);
    break;
  }
  }
}

} // namespace

void EffectVisuals::drawPips(ImDrawList *dl, ImVec2 centre,
                             const std::vector<EffectInstance> &effects,
                             int limit) {
  // One small square per effect, strongest first: enough to say "this one
  // is poisoned and weakened" without burying a pack in icons.
  std::vector<const EffectInstance *> order;
  for (const EffectInstance &e : effects) {
    if (e.type == EffectType::Grappled || e.type == EffectType::Resonance) {
      continue;
    }
    order.push_back(&e);
  }
  std::sort(order.begin(), order.end(),
            [this](const EffectInstance *a, const EffectInstance *b) {
              return intensity(a->type, a->stacks) >
                     intensity(b->type, b->stacks);
            });
  int shown = std::min<int>(limit, static_cast<int>(order.size()));
  const float size = 5.0f, gap = 2.0f;
  float x = std::floor(centre.x - (shown * size + (shown - 1) * gap) * 0.5f);
  for (int k = 0; k < shown; k++) {
    Look lk = look(order[k]->type);
    float i = intensity(order[k]->type, order[k]->stacks);
    dl->AddRectFilled(ImVec2(x - 1, centre.y - 1),
                      ImVec2(x + size + 1, centre.y + size + 1),
                      IM_COL32(0, 0, 0, 200));
    dl->AddRectFilled(ImVec2(x, centre.y), ImVec2(x + size, centre.y + size),
                      rgba(mix(lk.base, lk.hot, i), 1.0f));
    x += size + gap;
  }
}

void EffectVisuals::drawBadges(ImDrawList *dl, uint32_t id, bool isEnemy,
                               ImVec2 centre,
                               const std::vector<EffectInstance> &effects,
                               int limit) {
  if (effects.empty())
    return;
  std::vector<const EffectInstance *> order;
  for (const EffectInstance &e : effects) {
    if (e.type == EffectType::Grappled || e.type == EffectType::Resonance) {
      continue;
    }
    order.push_back(&e);
  }
  std::sort(order.begin(), order.end(),
            [this](const EffectInstance *a, const EffectInstance *b) {
              return intensity(a->type, a->stacks) >
                     intensity(b->type, b->stacks);
            });
  int shown = std::min<int>(limit, static_cast<int>(order.size()));
  const float size = 15.0f, gap = 3.0f;
  float width = shown * size + (shown - 1) * gap;
  float x = std::floor(centre.x - width * 0.5f);
  for (int k = 0; k < shown; k++) {
    const EffectInstance &e = *order[k];
    Look lk = look(e.type);
    float scale = 1.0f;
    auto it = tracks.find(trackKey(id, isEnemy, e.type));
    if (it != tracks.end()) {
      if (it->second.applyMs > 0.0f) {
        scale += 0.6f * std::sin(it->second.applyMs / 420.0f * PI);
      }
      if (it->second.popMs > 0.0f) {
        scale += 0.5f * std::sin(it->second.popMs / 520.0f * PI);
      }
    }
    float half = size * 0.5f * scale;
    ImVec2 c(x + size * 0.5f, centre.y);
    float i = intensity(e.type, e.stacks);
    bool buff = EffectRegistry::getCategory(e.type) == EffectCategory::Buff;
    dl->AddRectFilled(ImVec2(c.x - half, c.y - half),
                      ImVec2(c.x + half, c.y + half),
                      rgba(dark(lk.base, 0.28f), 0.92f), buff ? 4.0f : 1.0f);
    dl->AddRect(ImVec2(c.x - half, c.y - half), ImVec2(c.x + half, c.y + half),
                rgba(mix(lk.base, lk.hot, i), 1.0f), buff ? 4.0f : 1.0f, 0,
                1.0f + i);
    drawGlyph(dl, lk.glyph, c, half * 0.62f, rgba(lk.hot, 1.0f), e.type);
    if (e.stacks > 1) {
      char n[8];
      std::snprintf(n, sizeof(n), "%d", e.stacks);
      ArenaDraw::text(dl, ImVec2(c.x + half - 1.0f, c.y + half - 9.0f),
                      IM_COL32(255, 255, 255, 255), 10.0f, n);
    }
    x += size + gap;
  }
  if (static_cast<int>(order.size()) > shown) {
    char more[8];
    std::snprintf(more, sizeof(more), "+%d",
                  static_cast<int>(order.size()) - shown);
    ArenaDraw::text(dl, ImVec2(x + 4.0f, centre.y - 6.0f),
                    IM_COL32(220, 220, 220, 220), 10.0f, more);
  }
}

// ---------------------------------------------------------------------------
// Verb moments
// ---------------------------------------------------------------------------

void EffectVisuals::drawGround(ImDrawList *dl, const ScreenFn &toScreen) {
  for (const Mark &m : marks) {
    float t = m.ageMs / m.lifeMs;
    ImVec2 c = toScreen(m.x, m.y);
    float a = 0.38f * (1.0f - t * t);
    dl->AddEllipseFilled(c, ImVec2(m.radius, m.radius * 0.42f),
                         rgba(m.color, a), 0.0f, 28);
  }
}

void EffectVisuals::drawVerbs(ImDrawList *dl, const ScreenFn &toScreen,
                              const PositionFn &positionOf) {
  for (const Verb &v : verbs) {
    if (v.ageMs < v.delayMs && v.verb != EffectVerb::Spread)
      continue;
    Look lk = look(v.type);
    float mid = v.isEnemy ? BODY_MID_ENEMY : BODY_MID_PLAYER;
    float tx = v.x, ty = v.y;
    positionOf(v.target, v.isEnemy, tx, ty);
    ImVec2 at = toScreen(tx, ty);
    ImVec2 body(at.x, at.y - mid);
    float age = v.ageMs - v.delayMs;

    switch (v.verb) {
    case EffectVerb::Apply: {
      // A bolt from the caster, then the sigil (drawn by the track).
      float fx = v.x2, fy = v.y2;
      positionOf(v.from, false, fx, fy);
      ImVec2 from = toScreen(fx, fy - BODY_MID_PLAYER);
      float d = std::hypot(body.x - from.x, body.y - from.y);
      float travel = 160.0f + d * 0.25f;
      if (age < travel) {
        float t = age / travel;
        ImVec2 ctrl((from.x + body.x) * 0.5f,
                    std::min(from.y, body.y) - d * 0.3f);
        ImVec2 head = bezier(from, ctrl, body, t);
        ImVec2 tail = bezier(from, ctrl, body, std::max(0.0f, t - 0.25f));
        dl->AddLine(tail, head, rgba(lk.base, 0.7f), 3.0f);
        dl->AddCircleFilled(head, 4.0f, rgba(lk.hot, 1.0f), 10);
      } else {
        float k = (age - travel) / 450.0f;
        dl->AddCircleFilled(body, 14.0f * (1.0f - k) + 2.0f,
                            rgba(lk.hot, 0.6f * (1.0f - k)), 16);
      }
      break;
    }

    case EffectVerb::Amplify: {
      if (age < 180.0f) {
        // Charge in: rings converge on the target.
        float k = age / 180.0f;
        for (int r = 0; r < 3; r++) {
          float rr = (60.0f - r * 14.0f) * (1.0f - k) + 6.0f;
          dl->AddCircle(body, rr, rgba(lk.hot, 0.35f + 0.5f * k), 24, 2.0f);
        }
      } else {
        // Then swell: the aura blooms outward.
        float k = (age - 180.0f) / (v.lifeMs - 180.0f);
        float e = easeOut(k);
        float r =
            12.0f + 58.0f * e * (0.6f + 0.4f * intensity(v.type, v.after));
        dl->AddCircleFilled(body, r, rgba(lk.base, 0.28f * (1.0f - k)), 28);
        dl->AddCircle(body, r, rgba(lk.hot, 0.9f * (1.0f - k)), 32,
                      4.0f * (1.0f - k) + 1.0f);
        // A vertical flare through the body.
        float h = 70.0f * (1.0f - k);
        dl->AddRectFilled(ImVec2(body.x - 2.0f, body.y - h),
                          ImVec2(body.x + 2.0f, body.y + h * 0.4f),
                          rgba(lk.hot, 0.8f * (1.0f - k)));
      }
      break;
    }

    case EffectVerb::Spread: {
      float sx = v.x2, sy = v.y2;
      positionOf(v.from, v.isEnemy, sx, sy);
      ImVec2 from = toScreen(sx, sy - mid);
      float d = std::hypot(body.x - from.x, body.y - from.y);
      if (v.ageMs < 1.0f)
        break;
      // The tendril leaves at once and reaches far targets later.
      float travel = v.delayMs + 170.0f;
      float t = clamp01(v.ageMs / travel);
      ImVec2 ctrl((from.x + body.x) * 0.5f +
                      ((v.seed & 1) ? 1 : -1) * d * 0.18f,
                  std::min(from.y, body.y) - d * 0.35f - 12.0f);
      float fadeK = v.ageMs > travel ? (v.ageMs - travel) / 420.0f : 0.0f;
      float alpha = 1.0f - clamp01(fadeK);
      const int SEG = 14;
      float t0 = std::max(0.0f, t - 0.55f);
      for (int s = 0; s < SEG; s++) {
        float a = t0 + (t - t0) * s / SEG;
        float b = t0 + (t - t0) * (s + 1) / SEG;
        float thick = 1.0f + 3.5f * (static_cast<float>(s + 1) / SEG);
        dl->AddLine(bezier(from, ctrl, body, a), bezier(from, ctrl, body, b),
                    rgba(mix(lk.base, lk.hot, static_cast<float>(s) / SEG),
                         0.85f * alpha),
                    thick);
      }
      if (t < 1.0f) {
        ImVec2 head = bezier(from, ctrl, body, t);
        dl->AddCircleFilled(head, 4.5f, rgba(lk.hot, 1.0f), 10);
        dl->AddCircle(head, 7.0f, rgba(lk.base, 0.6f), 10, 1.5f);
      } else {
        // Arrival: a splash on the new carrier.
        float k = clamp01(fadeK);
        dl->AddCircle(body, 6.0f + 24.0f * easeOut(k), rgba(lk.hot, 1.0f - k),
                      20, 3.0f * (1.0f - k) + 1.0f);
        for (int p = 0; p < 6; p++) {
          float a = p * PI / 3.0f + (v.seed % 10) * 0.3f;
          float r = 6.0f + 22.0f * easeOut(k);
          pixel(dl, body.x + std::cos(a) * r, body.y + std::sin(a) * r * 0.8f,
                3.0f, rgba(lk.base, 1.0f - k));
        }
      }
      break;
    }

    case EffectVerb::Detonate: {
      float radius = v.after;
      if (age < 110.0f) {
        // Implode: the poison is sucked into the carrier...
        float k = age / 110.0f;
        float r = radius * 0.45f * (1.0f - k) + 3.0f;
        dl->AddEllipse(at, ImVec2(r, r * 0.5f), rgba(lk.hot, 0.5f + 0.5f * k),
                       0.0f, 24, 3.0f);
        dl->AddCircleFilled(body, 4.0f + 10.0f * k,
                            rgba(WHITE, 0.4f + 0.6f * k), 16);
      } else {
        // ...then bursts out.
        float k = (age - 110.0f) / (v.lifeMs - 110.0f);
        float e = easeOut(k);
        float r = radius * e;
        dl->AddEllipseFilled(at, ImVec2(r, r * 0.5f),
                             rgba(lk.base, 0.45f * (1.0f - k)), 0.0f, 32);
        dl->AddEllipse(at, ImVec2(r, r * 0.5f), rgba(lk.hot, 1.0f - k), 0.0f,
                       36, 4.0f * (1.0f - k) + 1.0f);
        if (k < 0.35f) {
          dl->AddCircleFilled(body, 18.0f * (1.0f - k / 0.35f) + 6.0f,
                              rgba(WHITE, 0.8f * (1.0f - k / 0.35f)), 16);
        }
        // Chunks fly out in an arc.
        int chunks = 10 + static_cast<int>(std::min(20.0f, v.before * 0.4f));
        for (int p = 0; p < chunks; p++) {
          uint32_t s = v.seed + p * 131u;
          float a = rand01(s) * 2.0f * PI;
          float dist = radius * (0.5f + 0.6f * rand01(s + 1)) * e;
          float lift = std::sin(k * PI) * (10.0f + 20.0f * rand01(s + 2));
          pixel(dl, at.x + std::cos(a) * dist,
                at.y + std::sin(a) * dist * 0.5f - lift - mid * 0.5f,
                p % 3 == 0 ? 4.0f : 3.0f,
                rgba(p % 2 ? lk.hot : lk.base, 1.0f - k * k));
        }
      }
      break;
    }

    case EffectVerb::Corrupt: {
      Look was = look(v.was);
      float s = std::min(5.0f, v.before);
      float scale = 0.8f + 0.12f * s;
      if (age < 220.0f) {
        // The buff glows hotter and cracks.
        float k = age / 220.0f;
        dl->AddCircle(body, 26.0f * scale, rgba(was.hot, 0.5f + 0.5f * k), 28,
                      2.0f + 2.0f * k);
        int cracks = 6;
        for (int c = 0; c < cracks; c++) {
          float a = c * 2.0f * PI / cracks + (v.seed % 7) * 0.2f;
          float len = 26.0f * scale * k;
          ImVec2 p1(body.x + std::cos(a) * len * 0.5f,
                    body.y + std::sin(a) * len * 0.5f);
          ImVec2 p2(body.x + std::cos(a + 0.35f) * len,
                    body.y + std::sin(a + 0.35f) * len);
          dl->AddLine(body, p1, rgba(WHITE, 0.9f), 1.5f);
          dl->AddLine(p1, p2, rgba(WHITE, 0.9f), 1.5f);
        }
      } else {
        float k = (age - 220.0f) / (v.lifeMs - 220.0f);
        if (age < 290.0f) {
          dl->AddCircleFilled(body, 34.0f * scale, rgba(WHITE, 0.85f), 24);
        }
        // Shards of the old buff fly off, turning into the new colour.
        int shards = 8 + static_cast<int>(s * 2.0f);
        for (int p = 0; p < shards; p++) {
          uint32_t sd = v.seed + p * 977u;
          float a = p * 2.0f * PI / shards + rand01(sd) * 0.4f;
          float dist =
              (20.0f + 60.0f * scale * rand01(sd + 1)) * easeOut(k * 1.6f);
          ImVec2 c(body.x + std::cos(a) * dist,
                   body.y + std::sin(a) * dist + 30.0f * k * k);
          float rot = k * 9.0f + p;
          float sz = 5.0f * (1.0f - k) + 1.5f;
          Rgb col = mix(was.hot, lk.base, std::min(1.0f, k * 2.0f));
          dl->AddTriangleFilled(
              ImVec2(c.x + std::cos(rot) * sz, c.y + std::sin(rot) * sz),
              ImVec2(c.x + std::cos(rot + 2.2f) * sz,
                     c.y + std::sin(rot + 2.2f) * sz),
              ImVec2(c.x + std::cos(rot + 4.1f) * sz * 0.6f,
                     c.y + std::sin(rot + 4.1f) * sz * 0.6f),
              rgba(col, 1.0f - k));
        }
        // The debuff blooms in its place.
        float e = easeOut(std::min(1.0f, k * 1.4f));
        float r = (12.0f + 50.0f * e) * scale;
        dl->AddCircleFilled(body, r, rgba(lk.base, 0.32f * (1.0f - k)), 28);
        dl->AddCircle(body, r, rgba(lk.hot, 0.9f * (1.0f - k)), 32, 3.0f);
        dl->AddEllipse(at, ImVec2(r * 1.2f, r * 0.5f),
                       rgba(lk.base, 0.6f * (1.0f - k)), 0.0f, 28, 2.0f);
      }
      break;
    }

    case EffectVerb::Purify: {
      Look was = look(v.was);
      float k = age / v.lifeMs;
      // The old debuff boils off upward...
      if (k < 0.6f) {
        float kk = k / 0.6f;
        for (int p = 0; p < 12; p++) {
          uint32_t sd = v.seed + p * 313u;
          float a = rand01(sd) * 2.0f * PI + kk * 3.0f;
          float r = 16.0f * (1.0f - kk) + 4.0f;
          pixel(dl, body.x + std::cos(a) * r,
                body.y + 10.0f - kk * 70.0f * (0.6f + 0.6f * rand01(sd + 1)),
                3.0f, rgba(was.base, 1.0f - kk));
        }
      }
      // ...while a column of the new buff's light pours down.
      float grow = easeOut(clamp01((k - 0.15f) / 0.4f));
      float fade = 1.0f - clamp01((k - 0.6f) / 0.4f);
      float h = 120.0f * grow;
      float w = 14.0f + 6.0f * std::sin(now * 12.0f);
      dl->AddRectFilledMultiColor(
          ImVec2(body.x - w, at.y - h), ImVec2(body.x + w, at.y),
          rgba(lk.hot, 0.0f), rgba(lk.hot, 0.0f), rgba(lk.base, 0.55f * fade),
          rgba(lk.base, 0.55f * fade));
      dl->AddEllipse(at, ImVec2(24.0f * grow + 6.0f, 10.0f * grow + 3.0f),
                     rgba(lk.hot, 0.9f * fade), 0.0f, 24, 2.5f);
      break;
    }
    }
  }
}

void EffectVisuals::drawCallouts(ImDrawList *dl,
                                 const ScreenFn &toZoomedScreen) {
  for (const Callout &c : callouts) {
    if (c.ageMs < c.delayMs || c.title.empty())
      continue;
    float t = (c.ageMs - c.delayMs) / c.lifeMs;
    float rise = 26.0f * easeOut(t);
    float pop = t < 0.12f ? 1.0f + 0.5f * std::sin(t / 0.12f * PI) : 1.0f;
    float alpha = t < 0.75f ? 1.0f : 1.0f - (t - 0.75f) / 0.25f;
    ImVec2 p = toZoomedScreen(c.x, c.y);
    p.y -= 95.0f + rise;
    ArenaDraw::text(dl, p, rgba(c.color, alpha), c.size * pop, c.title.c_str());
    if (!c.detail.empty()) {
      float y = p.y + c.size * pop + 1.0f;
      size_t start = 0;
      while (start <= c.detail.size()) {
        size_t end = c.detail.find('\n', start);
        std::string line = c.detail.substr(start, end - start);
        ArenaDraw::text(dl, ImVec2(p.x, y), rgba(c.detailColor, alpha), 12.0f,
                        line.c_str());
        y += 13.0f;
        if (end == std::string::npos)
          break;
        start = end + 1;
      }
    }
  }
}
