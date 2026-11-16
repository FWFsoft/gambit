#pragma once

#include <cmath>
#include <cstdint>

#include "ArenaData.h"
#include "NetworkProtocol.h"

// Controller aiming that depends on the character: how far the reticle
// reaches, and which abilities pick an ally instead of a spot.
//
// These numbers mirror the kits in src/CharacterKits.cpp. A kit whose reach
// changes there needs the same change here, or the reticle will stop short
// of (or overshoot) where the ability lands.
namespace PadAim {

// Reach for a kit with no ability that lands on a chosen spot.
constexpr float DEFAULT_REACH = 320.0f;

// How far the reticle goes at full tilt: the longest reach of any ability
// in the kit that lands where you point.
inline float reach(uint32_t characterId) {
  switch (characterId) {
    case Arena::CharacterId::Suds:
      return 420.0f;  // Slip 'n Slide, Bubble Trouble
    case Arena::CharacterId::Isaac:
      return 520.0f;  // Lifeline
    case Arena::CharacterId::Zephyr:
      return 460.0f;  // Cyclone
    case Arena::CharacterId::Mina:
      return 580.0f;  // charged lob
    case Arena::CharacterId::Presidente:
      return 460.0f;
    case Arena::CharacterId::Nyx:
      return 560.0f;  // sending the swarm
    default:
      return DEFAULT_REACH;
  }
}

// Abilities that pick an ally. On a controller the ally nearest the way you
// aim is chosen, instead of the one nearest the reticle.
struct AllyTargeting {
  uint8_t buttons = 0;  // InputButtons that snap the aim onto the ally
  float range = 0.0f;   // an ally farther than this is not picked
};

inline AllyTargeting allyTargeting(uint32_t characterId) {
  switch (characterId) {
    case Arena::CharacterId::Isaac:
      // Lifeline (reaches 520) and The Blessing of Isaac.
      return {InputButtons::Secondary | InputButtons::Special, 520.0f};
    case Arena::CharacterId::Zephyr:
      return {InputButtons::Special, 700.0f};  // Gale's Embrace
    default:
      return {};
  }
}

// An ally counts as aimed at within this angle of the stick: cos(35 deg).
constexpr float ALLY_CONE_COS = 0.8192f;

struct Point {
  float x, y;
};

// The ally the stick is pointing at: the one closest in angle to the aim
// direction, inside the cone and the range. -1 when there is none, which
// leaves the aim where the reticle is (and the ability on yourself, for
// those that fall back to the caster).
inline int pickAlly(float selfX, float selfY, float aimDirX, float aimDirY,
                    const Point* allies, int count, float range) {
  const float aimLength = std::hypot(aimDirX, aimDirY);
  if (aimLength <= 0.0f) return -1;
  int best = -1;
  float bestCos = ALLY_CONE_COS;
  for (int i = 0; i < count; i++) {
    const float dx = allies[i].x - selfX;
    const float dy = allies[i].y - selfY;
    const float distance = std::hypot(dx, dy);
    if (distance > range) continue;
    // Standing on top of you: any direction points at them.
    const float cosine = distance < 1.0f
                             ? 1.0f
                             : (dx * aimDirX + dy * aimDirY) /
                                   (distance * aimLength);
    if (cosine >= bestCos) {
      bestCos = cosine;
      best = i;
    }
  }
  return best;
}

}  // namespace PadAim
