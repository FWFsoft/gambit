#pragma once

#include <functional>
#include <string>

#include "ArenaData.h"

// Decides which playlist should be playing; MusicSystem plays it. Pure
// logic, no audio, so the rules are testable (tests/test_music_director.cpp).
// The rules are the designer's (docs/design/MUSIC.md):
//
//   Character select, lobby, intermission    "lobby" (loops)
//   The first round, from landing             "first_contact" until it ends
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

const char* moodKey(MusicMood mood);

// The mood for a party of `partySize` heroes with `alive` standing.
MusicMood moodFor(int partySize, int alive);

class MusicDirector {
 public:
  static constexpr float LANDING_RADIUS = 2000.0f;  // world px from the ship
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
  };

  // `hasPlaylist(key)` says whether a playlist exists (music.txt).
  explicit MusicDirector(std::function<bool(const std::string&)> hasPlaylist);

  // The playlist that should play now, or "" to keep whatever plays.
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
