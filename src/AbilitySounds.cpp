#include "AbilitySounds.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace AbilitySounds {

namespace {

// When a cast draws several shapes, the one that says most about it
// decides the sound: a beam with a ring at the muzzle is a beam.
constexpr Shape PRIORITY[] = {Shape::Beam,   Shape::Projectile, Shape::Slash,
                              Shape::Cone,   Shape::Lob,        Shape::Burst,
                              Shape::Zone,   Shape::Tether,     Shape::Dash,
                              Shape::Shield, Shape::Ring};

int rank(Shape shape) {
  for (int i = 0; i < static_cast<int>(sizeof(PRIORITY) / sizeof(PRIORITY[0]));
       i++) {
    if (PRIORITY[i] == shape) return i;
  }
  return 1000;
}

float distance(float ax, float ay, float bx, float by) {
  float dx = ax - bx;
  float dy = ay - by;
  return std::sqrt(dx * dx + dy * dy);
}

bool near(const GameEventPacket& e, const GameEventPacket& cast) {
  auto close = [&](float x, float y) {
    return distance(x, y, cast.x, cast.y) <= GROUP_RADIUS ||
           distance(x, y, cast.x2, cast.y2) <= GROUP_RADIUS;
  };
  if (close(e.x, e.y)) return true;
  // Beams, tethers and dash trails run from x,y to x2,y2.
  bool line = e.kind == GameEventKind::Beam ||
              e.kind == GameEventKind::Tether ||
              e.kind == GameEventKind::DashTrail;
  return line && close(e.x2, e.y2);
}

VfxTheme themeOf(const GameEventPacket& e) {
  if (e.kind == GameEventKind::ShieldBubble) return VfxTheme::Neutral;
  return static_cast<VfxTheme>(e.param2);
}

}  // namespace

Shape shapeOf(const GameEventPacket& e) {
  switch (e.kind) {
    case GameEventKind::Slash:
      return Shape::Slash;
    case GameEventKind::Cone:
      return Shape::Cone;
    case GameEventKind::Burst:
      return Shape::Burst;
    case GameEventKind::Ring:
      return Shape::Ring;
    case GameEventKind::Beam:
      return Shape::Beam;
    case GameEventKind::ProjectileSpawn:
      return e.sourceIsEnemy ? Shape::None : Shape::Projectile;
    case GameEventKind::Lob:
      return Shape::Lob;
    case GameEventKind::ZoneSpawn:
      return e.sourceIsEnemy ? Shape::None : Shape::Zone;
    case GameEventKind::DashTrail:
      return Shape::Dash;
    case GameEventKind::Tether:
      return Shape::Tether;
    case GameEventKind::ShieldBubble:
      return Shape::Shield;
    default:
      return Shape::None;
  }
}

bool belongsTo(const GameEventPacket& e, const GameEventPacket& cast) {
  if (shapeOf(e) == Shape::None) return false;
  if (static_cast<VfxTheme>(e.param2) == VfxTheme::Danger &&
      e.kind != GameEventKind::ShieldBubble) {
    return false;
  }
  switch (e.kind) {
    case GameEventKind::ProjectileSpawn:
    case GameEventKind::ZoneSpawn:
      return e.sourceId == cast.sourceId;  // owned (enemy ones are None)
    case GameEventKind::Slash:
      if (e.sourceId != 0) return e.sourceId == cast.sourceId;
      return near(e, cast);
    case GameEventKind::ShieldBubble:
      // On the caster, or on an ally the caster shielded nearby.
      return e.sourceId == cast.sourceId || near(e, cast);
    default:
      return near(e, cast);
  }
}

bool hasTint(VfxTheme theme) { return tintName(theme) != nullptr; }

Cue chooseCast(const GameEventPacket& cast,
               const std::vector<GameEventPacket>& group,
               uint32_t localPlayerId, float listenerX, float listenerY) {
  Cue cue;
  if (cast.kind != GameEventKind::AbilityCast) return cue;

  bool local = cast.sourceId == localPlayerId;
  float dx = cast.x - listenerX;
  float dy = cast.y - listenerY;
  float d = std::sqrt(dx * dx + dy * dy);
  if (!local && d > HEARING_RADIUS) return cue;

  const GameEventPacket* primary = nullptr;
  for (const GameEventPacket& e : group) {
    if (!belongsTo(e, cast)) continue;
    if (!primary || rank(shapeOf(e)) < rank(shapeOf(*primary))) primary = &e;
  }
  cue.shape = primary ? shapeOf(*primary) : Shape::Generic;

  // The primary's theme; if it has no tint, the first shape that has one.
  cue.theme = primary ? themeOf(*primary) : VfxTheme::Neutral;
  if (!hasTint(cue.theme)) {
    for (const GameEventPacket& e : group) {
      if (belongsTo(e, cast) && hasTint(themeOf(e))) {
        cue.theme = themeOf(e);
        break;
      }
    }
  }

  bool primarySlot =
      cast.param == static_cast<uint8_t>(Arena::AbilitySlot::Primary);
  if (local) {
    cue.volume = primarySlot ? LOCAL_PRIMARY_VOLUME : LOCAL_ABILITY_VOLUME;
    cue.pan = 0.0f;
    return cue;
  }
  float nearness = 1.0f;
  if (d > FULL_VOLUME_RADIUS) {
    nearness =
        1.0f - (d - FULL_VOLUME_RADIUS) / (HEARING_RADIUS - FULL_VOLUME_RADIUS);
  }
  float slotScale = primarySlot ? LOCAL_PRIMARY_VOLUME : LOCAL_ABILITY_VOLUME;
  cue.volume = TEAMMATE_VOLUME * slotScale * (0.3f + 0.7f * nearness);
  cue.pan = std::clamp(dx / FULL_PAN_DISTANCE, -1.0f, 1.0f);
  assert(cue.volume > 0.0f && cue.volume <= 1.0f);
  return cue;
}

uint64_t CastGrouper::key(const GameEventPacket& cast) {
  return (static_cast<uint64_t>(cast.sourceId) << 8) | cast.param;
}

std::vector<Cue> CastGrouper::onEvent(const GameEventPacket& event, float nowMs,
                                      uint32_t localPlayerId, float listenerX,
                                      float listenerY) {
  std::vector<Cue> out;
  shapes.erase(std::remove_if(shapes.begin(), shapes.end(),
                              [&](const Timed& t) {
                                return nowMs - t.receivedMs > GROUP_WINDOW_MS;
                              }),
               shapes.end());
  auto keep = [&](const Cue& cue) {
    if (cue.shape != Shape::None) out.push_back(cue);
  };

  if (shapeOf(event) != Shape::None) {
    // The first shape after a waiting cast is that cast's (oldest first).
    for (auto it = casts.begin(); it != casts.end(); ++it) {
      if (!belongsTo(event, it->event)) continue;
      shapeless.erase(key(it->event));
      keep(chooseCast(it->event, {event}, localPlayerId, listenerX, listenerY));
      casts.erase(it);
      return out;
    }
    shapes.push_back({nowMs, event});
    return out;
  }

  if (event.kind != GameEventKind::AbilityCast) return out;
  std::vector<GameEventPacket> group;
  for (const Timed& t : shapes) {
    if (belongsTo(t.event, event)) group.push_back(t.event);
  }
  if (group.empty()) {
    if (shapeless.count(key(event)) != 0) {
      keep(chooseCast(event, {}, localPlayerId, listenerX, listenerY));
    } else {
      casts.push_back({nowMs, event});
    }
    return out;
  }
  shapeless.erase(key(event));
  // The shapes this cast drew are spent; others may be a teammate's.
  shapes.erase(
      std::remove_if(shapes.begin(), shapes.end(),
                     [&](const Timed& t) { return belongsTo(t.event, event); }),
      shapes.end());
  keep(chooseCast(event, group, localPlayerId, listenerX, listenerY));
  return out;
}

std::vector<Cue> CastGrouper::onTick(float nowMs, uint32_t localPlayerId,
                                     float listenerX, float listenerY) {
  std::vector<Cue> out;
  while (!casts.empty() && nowMs - casts.front().receivedMs > CAST_WAIT_MS) {
    shapeless.insert(key(casts.front().event));
    Cue cue = chooseCast(casts.front().event, {}, localPlayerId, listenerX,
                         listenerY);
    if (cue.shape != Shape::None) out.push_back(cue);
    casts.erase(casts.begin());
  }
  return out;
}

Limits limitsFor(Shape shape) {
  switch (shape) {
    case Shape::Slash:
    case Shape::Projectile:
    case Shape::Generic:
      return {40.0f, 3};
    case Shape::Cone:
    case Shape::Lob:
    case Shape::Dash:
    case Shape::Ring:
      return {60.0f, 2};
    case Shape::Burst:
    case Shape::Beam:
    case Shape::Zone:
    case Shape::Tether:
    case Shape::Shield:
      return {90.0f, 2};
    default:
      assert(false && "no limits for this shape");
      return {0.0f, 0};
  }
}

Limits tintLimits() { return {70.0f, 2}; }

const char* shapeName(Shape shape) {
  switch (shape) {
    case Shape::Generic:
      return "generic";
    case Shape::Slash:
      return "slash";
    case Shape::Cone:
      return "cone";
    case Shape::Burst:
      return "burst";
    case Shape::Ring:
      return "ring";
    case Shape::Beam:
      return "beam";
    case Shape::Projectile:
      return "projectile";
    case Shape::Lob:
      return "lob";
    case Shape::Zone:
      return "zone";
    case Shape::Dash:
      return "dash";
    case Shape::Tether:
      return "tether";
    case Shape::Shield:
      return "shield";
    default:
      assert(false && "no file for this shape");
      return "";
  }
}

const char* tintName(VfxTheme theme) {
  switch (theme) {
    case VfxTheme::Holy:
      return "holy";
    case VfxTheme::Steel:
      return "steel";
    case VfxTheme::Brew:
      return "brew";
    case VfxTheme::Optic:
      return "optic";
    case VfxTheme::Nanite:
      return "nanite";
    case VfxTheme::Biomass:
      return "biomass";
    case VfxTheme::Blood:
      return "blood";
    case VfxTheme::Shadow:
      return "shadow";
    case VfxTheme::Fire:
      return "fire";
    case VfxTheme::Heal:
      return "heal";
    default:
      return nullptr;  // Neutral, Wind and themes no kit uses yet
  }
}

}  // namespace AbilitySounds
