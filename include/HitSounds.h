#pragma once

#include <cstdint>

#include "NetworkProtocol.h"

// Which hit sound a server Damage event makes, how loud, and where in the
// stereo field. Pure: no audio here, so it is unit tested
// (tests/test_hit_sounds.cpp). SfxSystem plays what it says.
//
// The rules (docs/design/SFX.md):
// - An enemy hit: a short punch. A crit: a heavier impact.
// - The local hero hit: a hurt grunt, always full and centred.
// - Teammates hit and damage-over-time ticks: silent, so the mix stays
//   readable in a four-player fight.
// - Bigger hits are louder; hits fade with distance from the local hero
//   and pan left or right with where they are on screen.
namespace HitSounds {

enum class Sound : uint8_t { None = 0, EnemyHit, EnemyCrit, LocalHurt, Count };

struct Cue {
  Sound sound = Sound::None;
  float volume = 0.0f;  // 0..1
  float pan = 0.0f;     // -1 left .. +1 right
};

// Beyond this distance from the local hero a hit is silent (about a screen
// and a half at default zoom).
constexpr float HEARING_RADIUS = 1400.0f;
// Within this distance a hit plays at full volume.
constexpr float FULL_VOLUME_RADIUS = 250.0f;
// Hits this far to the side pan fully.
constexpr float FULL_PAN_DISTANCE = 900.0f;
// Damage at which an enemy hit reaches full loudness.
constexpr float LOUD_HIT_DAMAGE = 40.0f;
// The quietest a hit that is heard at all plays (near the hearing edge or
// a tiny hit).
constexpr float MIN_VOLUME = 0.25f;

Cue choose(const GameEventPacket &event, uint32_t localPlayerId,
           float listenerX, float listenerY);

// How often one sound may start and how many copies may sound at once,
// so a crowd of hits does not turn into noise.
struct Limits {
  float minGapMs;
  int maxVoices;
};
Limits limitsFor(Sound sound);

}  // namespace HitSounds
