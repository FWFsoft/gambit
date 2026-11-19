#include "HitSounds.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace HitSounds {

Cue choose(const GameEventPacket &e, uint32_t localPlayerId, float listenerX,
           float listenerY) {
  Cue cue;
  if (e.kind != GameEventKind::Damage) return cue;
  if (e.value < 0.5f) return cue;  // the same cut-off as the damage numbers

  bool crit = (e.param & 1) != 0;
  bool onPlayer = (e.param & 2) != 0;
  bool dot = (e.param & 4) != 0;
  if (dot) return cue;

  if (onPlayer || !e.sourceIsEnemy) {
    if (e.sourceIsEnemy || e.sourceId != localPlayerId) return cue;
    cue.sound = Sound::LocalHurt;
    cue.volume = 1.0f;
    cue.pan = 0.0f;
    return cue;
  }

  float dx = e.x - listenerX;
  float dy = e.y - listenerY;
  float distance = std::sqrt(dx * dx + dy * dy);
  if (distance > HEARING_RADIUS) return cue;

  float nearness = 1.0f;
  if (distance > FULL_VOLUME_RADIUS) {
    nearness = 1.0f - (distance - FULL_VOLUME_RADIUS) /
                          (HEARING_RADIUS - FULL_VOLUME_RADIUS);
  }
  float weight =
      crit ? 1.0f : std::clamp(e.value / LOUD_HIT_DAMAGE, 0.0f, 1.0f);
  float loudness = 0.6f + 0.4f * weight;

  cue.sound = crit ? Sound::EnemyCrit : Sound::EnemyHit;
  cue.volume = std::clamp(
      MIN_VOLUME + (1.0f - MIN_VOLUME) * nearness * loudness, MIN_VOLUME, 1.0f);
  cue.pan = std::clamp(dx / FULL_PAN_DISTANCE, -1.0f, 1.0f);
  assert(cue.volume > 0.0f && cue.volume <= 1.0f);
  return cue;
}

Limits limitsFor(Sound sound) {
  switch (sound) {
    case Sound::EnemyHit:
      return {35.0f, 4};
    case Sound::EnemyCrit:
      return {60.0f, 2};
    case Sound::LocalHurt:
      // Long enough that a swarm reads as "I'm getting hit", not a drone.
      return {250.0f, 1};
    default:
      assert(false && "no limits for this sound");
      return {0.0f, 0};
  }
}

}  // namespace HitSounds
