#pragma once

#include <functional>
#include <string>

#include "ArenaData.h"

// Decides which playlist should be playing; MusicSystem plays it. Pure
// logic, no audio, so the rules are testable (tests/test_music_director.cpp).
// The rules are the designer's (docs/design/MUSIC.md):
//
//   Character select, lobby, intermission    "lobby" (loops)
//   The crash intro and the first round       "first_contact" until it ends
//   In the ship's home ring                   "landing.<mood>"
//   Elsewhere                                 "<biome>.<mood>"
//   Boss fight                                "<ship biome>.boss", else "boss"
//
// Mood follows the party: Confident while everyone is alive (always, solo),
// Alone when one is left, Weakened in between.
//
// Borders and deaths are not followed instantly, so music never flips on a
// quick trip home or a revive: a new place takes ZONE_HOLD_S of standing in
// it, a worse mood WORSEN_HOLD_S, a better one IMPROVE_HOLD_S. A biome with
// no music of its own keeps whatever was playing.
enum class MusicMood : uint8_t { Confident = 0, Weakened = 1, Alone = 2 };

// The radio: every place's playlist ("<place>.<mood>") is a channel that
// plays on whether anyone is listening. Coming back to one picks up where it
// would be now, not from the start (MusicSystem keeps the clock).
inline bool isRadioPlaylist(const std::string& key) {
  const size_t dot = key.find('.');
  return dot != std::string::npos && key.compare(dot, 5, ".boss") != 0;
}
// Where a channel's track is, `now` seconds into the match, if it started
// at `startedAt` and lasts `duration` (<= 0: unknown). `onAir` is false
// when the track is over (or too near its end to bother): play the next.
struct RadioTune {
  bool onAir;
  double position;
};
inline RadioTune radioTune(double startedAt, double now, double duration) {
  constexpr double TAIL_S = 4.0;  // closer to the end than this: next track
  const double position = now - startedAt;
  if (position < 0.0) return {true, 0.0};
  if (duration <= 0.0) return {true, 0.0};  // cannot seek: from the top
  if (position >= duration - TAIL_S) return {false, 0.0};
  return {true, position};
}

const char* moodKey(MusicMood mood);

// The mood for a party of `partySize` heroes with `alive` standing.
MusicMood moodFor(int partySize, int alive);

class MusicDirector {
 public:
  // The crash site (the map's ash ground, generate_planet.py
  // CRASH_SITE_RADIUS): its music starts inside LANDING_RADIUS and holds out
  // to LANDING_EXIT_RADIUS, so walking its edge never flips the music.
  static constexpr float LANDING_RADIUS = 1300.0f;  // world px from the ship
  static constexpr float LANDING_EXIT_RADIUS = 1700.0f;
  static constexpr float ZONE_HOLD_S = 5.0f;
  static constexpr float WORSEN_HOLD_S = 1.5f;
  static constexpr float IMPROVE_HOLD_S = 6.0f;

  struct Inputs {
    bool characterSelect = false;  // the character select screen
    bool hasMatch = false;
    Arena::MatchPhase phase = Arena::MatchPhase::Lobby;
    int round = 0;            // 1-based
    int partySize = 0;        // heroes in the match
    int alive = 0;            // of them, standing
    float distToShip = 0.0f;  // the local hero's, world px
    std::string biome;        // biome key under the local hero ("" = none)
    std::string shipBiome;    // biome key the ship landed in
    bool trackEnded = false;  // the playing track just finished

    bool runEnded() const {
      // Character select can precede the server's next lobby snapshot.
      return !characterSelect && hasMatch &&
             (phase == Arena::MatchPhase::Victory ||
              phase == Arena::MatchPhase::Defeat);
    }
  };

  // `hasPlaylist(key)` says whether a playlist exists (music.txt).
  explicit MusicDirector(std::function<bool(const std::string&)> hasPlaylist);

  // Forget a completed run, including First Contact and mood hysteresis.
  void reset();

  // The playlist that should play now, or "" to keep whatever plays.
  // runEnded() instead means silence: the audio owner must fade out.
  std::string update(const Inputs& in, float dtSeconds);

  MusicMood mood() const { return settledMood; }
  const std::string& place() const { return settledPlace; }

 private:
  std::function<bool(const std::string&)> has;
  bool firstContactPlaying = false;
  bool firstContactDone = false;
  bool settled = false;
  std::string settledPlace;  // "landing" or a biome key with music
  std::string pendingPlace;
  float placeTimer = 0.0f;
  MusicMood settledMood = MusicMood::Confident;
  MusicMood pendingMood = MusicMood::Confident;
  float moodTimer = 0.0f;
  std::string current;  // the last playlist asked for

  bool hasPlaceMusic(const std::string& place) const;
  std::string placePlaylist(const std::string& place, MusicMood mood) const;
  void track(const Inputs& in, float dt);
};
