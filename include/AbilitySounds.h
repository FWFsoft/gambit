#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "NetworkProtocol.h"

// Which sound an ability cast makes. Pure: no audio here, so it is unit
// tested (tests/test_ability_sounds.cpp). SfxSystem plays what it says.
//
// Every ability sounds without per-ability work: the sound is built from
// what the ability does on screen. The server sends a cast's shape events
// (slash, cone, burst, beam, projectile...) and then its AbilityCast event,
// in the same tick. The client groups them: the shape picks the main sound
// and the theme (Holy, Optic, Fire...) adds a tint on top, so a theme
// always sounds the same, as it always has the same colour
// (docs/design/EFFECT_VISUALS.md). Rules: docs/design/SFX.md.
namespace AbilitySounds {

enum class Shape : uint8_t {
  None = 0,
  Generic,  // a cast with no shape (buffs, stances)
  Slash,
  Cone,
  Burst,
  Ring,
  Beam,
  Projectile,
  Lob,
  Zone,
  Dash,
  Tether,
  Shield,
  Count
};

// The shape a game event draws, or None if it is not one a cast makes
// (damage numbers, telegraphs, announcements, enemy-owned shots...).
Shape shapeOf(const GameEventPacket& event);

// Whether this theme has a tint sound. Wind and Neutral do not.
bool hasTint(VfxTheme theme);

struct Cue {
  Shape shape = Shape::None;
  VfxTheme theme = VfxTheme::Neutral;  // tint to play, if hasTint(theme)
  float volume = 0.0f;                 // 0..1, of the shape; the tint is
                                       // TINT_VOLUME of that
  float pan = 0.0f;                    // -1 left .. +1 right
};

// Shape events arriving this close in time before a cast belong to it.
constexpr float GROUP_WINDOW_MS = 40.0f;
// A cast with no shape yet waits this long for one (wind-ups: Gravon's
// swing lands 370 ms after the press) and sounds when it appears, in step
// with what you see. Past this it plays the generic cast sound, and that
// player's slot is remembered as shapeless, so next time it plays at once.
constexpr float CAST_WAIT_MS = 450.0f;
// ...and unowned ones (cone, burst, beam...) must be this near the caster
// or the aim point, so an enemy's burst in the same tick is not taken for
// the cast.
constexpr float GROUP_RADIUS = 700.0f;

// Your own casts: the basic attack is used constantly, so it is quieter.
constexpr float LOCAL_PRIMARY_VOLUME = 0.6f;
constexpr float LOCAL_ABILITY_VOLUME = 0.9f;
// Teammates' casts sit behind yours and fade with distance.
constexpr float TEAMMATE_VOLUME = 0.4f;
constexpr float HEARING_RADIUS = 1400.0f;
constexpr float FULL_VOLUME_RADIUS = 250.0f;
constexpr float FULL_PAN_DISTANCE = 900.0f;
// The tint plays under the shape at this fraction of its volume.
constexpr float TINT_VOLUME = 0.55f;

// Whether a shape event received with this cast was drawn by it: owned by
// the caster, or (for unowned shapes) within GROUP_RADIUS of the caster or
// the aim point, and not an enemy telegraph.
bool belongsTo(const GameEventPacket& shape, const GameEventPacket& cast);

// cast: an AbilityCast event. group: the shape events received just before
// it (SfxSystem filters by time; this filters by owner and place).
Cue chooseCast(const GameEventPacket& cast,
               const std::vector<GameEventPacket>& group,
               uint32_t localPlayerId, float listenerX, float listenerY);

// Groups a stream of game events into cast sounds: shapes wait for the
// cast that follows them (same tick), and a cast with no shape waits for
// the first one that follows it. Shapes no cast claims (enemy attacks,
// delayed blasts) stay silent: their hits make the sound.
class CastGrouper {
 public:
  // Feed every game event as it arrives; returns cues to play now.
  std::vector<Cue> onEvent(const GameEventPacket& event, float nowMs,
                           uint32_t localPlayerId, float listenerX,
                           float listenerY);
  // Call every frame; returns casts that waited out CAST_WAIT_MS.
  std::vector<Cue> onTick(float nowMs, uint32_t localPlayerId, float listenerX,
                          float listenerY);

 private:
  struct Timed {
    float receivedMs;
    GameEventPacket event;
  };
  std::vector<Timed> shapes;  // waiting for a cast
  std::vector<Timed> casts;   // waiting for a shape
  // (player id << 8 | slot) of casts that last turned out shapeless.
  std::unordered_set<uint64_t> shapeless;
  static uint64_t key(const GameEventPacket& cast);
};

struct Limits {
  float minGapMs;
  int maxVoices;
};
Limits limitsFor(Shape shape);
Limits tintLimits();

// File stems under assets/sfx/ability/ ("shape_<name>_<n>.ogg",
// "tint_<name>_<n>.ogg").
const char* shapeName(Shape shape);
const char* tintName(VfxTheme theme);  // nullptr if !hasTint(theme)

}  // namespace AbilitySounds
