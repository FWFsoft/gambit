// The designer's music rules (MusicDirector): which playlist plays for the
// match phase, the party's state and where the local hero stands.

#include <cassert>
#include <iostream>
#include <set>
#include <string>

#include "MusicDirector.h"

using Arena::MatchPhase;

namespace {

// The playlists that exist today: four scored biomes (Tidal has no boss
// track), the landing zone and the lobby. Plains and the rest have none.
const std::set<std::string> PLAYLISTS = {"lobby",
                                         "first_contact",
                                         "landing.confident",
                                         "landing.weakened",
                                         "landing.alone",
                                         "pnw.confident",
                                         "pnw.weakened",
                                         "pnw.alone",
                                         "pnw.boss",
                                         "desert.confident",
                                         "desert.weakened",
                                         "desert.alone",
                                         "desert.boss",
                                         "city.confident",
                                         "city.weakened",
                                         "city.alone",
                                         "city.boss",
                                         "tidal.confident",
                                         "tidal.weakened",
                                         "tidal.alone",
                                         "boss"};

MusicDirector director() {
  return MusicDirector(
      [](const std::string& key) { return PLAYLISTS.count(key) > 0; });
}

MusicDirector::Inputs round1(int party = 4, int alive = 4,
                             float distToShip = 0.0f,
                             const std::string& biome = "pnw") {
  MusicDirector::Inputs in;
  in.hasMatch = true;
  in.phase = MatchPhase::Round;
  in.round = 1;
  in.partySize = party;
  in.alive = alive;
  in.distToShip = distToShip;
  in.biome = biome;
  in.shipBiome = "desert";
  return in;
}

// Run `seconds` of frames at 60 FPS; returns the last answer.
std::string run(MusicDirector& d, MusicDirector::Inputs in, float seconds) {
  std::string out;
  int frames = static_cast<int>(seconds * 60.0f + 0.5f);
  for (int i = 0; i < frames; i++) {
    out = d.update(in, 1.0f / 60.0f);
    in.trackEnded = false;
  }
  return out;
}

// Skip First Contact: start the round, then report its track ending.
void pastFirstContact(MusicDirector& d, MusicDirector::Inputs in) {
  assert(d.update(in, 0.016f) == "first_contact");
  in.trackEnded = true;
  d.update(in, 0.016f);
}

void testMoods() {
  // The designer's table.
  assert(moodFor(1, 1) == MusicMood::Confident);  // solo: always
  assert(moodFor(1, 0) == MusicMood::Confident);
  assert(moodFor(2, 2) == MusicMood::Confident);
  assert(moodFor(2, 1) == MusicMood::Alone);
  assert(moodFor(3, 3) == MusicMood::Confident);
  assert(moodFor(3, 2) == MusicMood::Weakened);
  assert(moodFor(3, 1) == MusicMood::Alone);
  assert(moodFor(4, 4) == MusicMood::Confident);
  assert(moodFor(4, 3) == MusicMood::Weakened);  // decided 2026-11-18
  assert(moodFor(4, 2) == MusicMood::Weakened);
  assert(moodFor(4, 1) == MusicMood::Alone);
}

void testLobbyAndIntermission() {
  MusicDirector d = director();
  MusicDirector::Inputs in;
  in.characterSelect = true;
  assert(d.update(in, 0.016f) == "lobby");
  in = round1();
  in.phase = MatchPhase::Lobby;
  assert(d.update(in, 0.016f) == "lobby");
  in.phase = MatchPhase::Intermission;
  assert(d.update(in, 0.016f) == "lobby");
}

void testFirstContact() {
  MusicDirector d = director();
  MusicDirector::Inputs in = round1();
  // It plays from landing, wherever the crew goes, until it ends.
  assert(run(d, in, 30.0f) == "first_contact");
  in.distToShip = 6000.0f;
  assert(run(d, in, 30.0f) == "first_contact");
  in.trackEnded = true;
  // Then the place they are in: out in the PNW by now.
  assert(d.update(in, 0.016f) == "pnw.confident");

  // Cut short by the intermission: not replayed in round 2.
  MusicDirector e = director();
  MusicDirector::Inputs r = round1();
  assert(e.update(r, 0.016f) == "first_contact");
  r.phase = MatchPhase::Intermission;
  assert(e.update(r, 0.016f) == "lobby");
  r.phase = MatchPhase::Round;
  r.round = 2;
  assert(e.update(r, 0.016f) == "landing.confident");

  // A new match lands with First Contact again.
  r.phase = MatchPhase::Lobby;
  e.update(r, 0.016f);
  r.phase = MatchPhase::Round;
  r.round = 1;
  assert(e.update(r, 0.016f) == "first_contact");
}

void testPlaces() {
  MusicDirector d = director();
  MusicDirector::Inputs in = round1();
  pastFirstContact(d, in);
  assert(d.update(in, 0.016f) == "landing.confident");
  // Out of the home ring into the forest: only after ZONE_HOLD_S.
  in.distToShip = 3000.0f;
  assert(run(d, in, MusicDirector::ZONE_HOLD_S - 0.5f) == "landing.confident");
  assert(run(d, in, 1.0f) == "pnw.confident");
  // A quick trip home (hauling scrap) does not flip it back.
  in.distToShip = 500.0f;
  assert(run(d, in, 3.0f) == "pnw.confident");
  in.distToShip = 3000.0f;
  assert(run(d, in, 3.0f) == "pnw.confident");
  // Into the desert, and then the plains, which have no music yet: the
  // desert's keeps playing.
  in.biome = "desert";
  assert(run(d, in, MusicDirector::ZONE_HOLD_S + 0.5f) == "desert.confident");
  in.biome = "plains";
  assert(run(d, in, 20.0f) == "desert.confident");
}

void testRadio() {
  // Place playlists are channels; the lobby, First Contact and boss are not.
  assert(isRadioPlaylist("pnw.confident") && isRadioPlaylist("landing.alone"));
  assert(!isRadioPlaylist("lobby") && !isRadioPlaylist("first_contact"));
  assert(!isRadioPlaylist("desert.boss"));
  // A channel tuned back into picks up where it would be now.
  RadioTune t = radioTune(10.0, 70.0, 200.0);
  assert(t.onAir && t.position == 60.0);
  // A track that ended while away: the next one.
  assert(!radioTune(10.0, 300.0, 200.0).onAir);
  assert(!radioTune(10.0, 208.0, 200.0).onAir);  // too near the end
  // Unknown length: from the top.
  t = radioTune(10.0, 70.0, 0.0);
  assert(t.onAir && t.position == 0.0);
}

void testCrashSiteEdge() {
  MusicDirector d = director();
  MusicDirector::Inputs in = round1();
  pastFirstContact(d, in);
  assert(d.update(in, 0.016f) == "landing.confident");
  // Wandering just past the crash site's edge keeps its music...
  in.distToShip = MusicDirector::LANDING_RADIUS + 200.0f;
  assert(run(d, in, MusicDirector::ZONE_HOLD_S + 2.0f) == "landing.confident");
  // ... until well clear of it.
  in.distToShip = MusicDirector::LANDING_EXIT_RADIUS + 100.0f;
  assert(run(d, in, MusicDirector::ZONE_HOLD_S + 0.5f) == "pnw.confident");
  // Coming back, the edge band is not enough: inside the site it is.
  in.distToShip = MusicDirector::LANDING_RADIUS + 200.0f;
  assert(run(d, in, MusicDirector::ZONE_HOLD_S + 2.0f) == "pnw.confident");
  in.distToShip = MusicDirector::LANDING_RADIUS - 100.0f;
  assert(run(d, in, MusicDirector::ZONE_HOLD_S + 0.5f) == "landing.confident");
}

void testDeaths() {
  MusicDirector d = director();
  MusicDirector::Inputs in = round1(4, 4, 3000.0f, "city");
  pastFirstContact(d, in);
  assert(d.update(in, 0.016f) == "city.confident");
  // One goes down and is revived at once: no change.
  in.alive = 3;
  assert(run(d, in, 1.0f) == "city.confident");
  in.alive = 4;
  assert(run(d, in, 1.0f) == "city.confident");
  // Down for good: Weakened soon after, Alone soon after the next.
  in.alive = 3;
  assert(run(d, in, MusicDirector::WORSEN_HOLD_S + 0.2f) == "city.weakened");
  in.alive = 1;
  assert(run(d, in, MusicDirector::WORSEN_HOLD_S + 0.2f) == "city.alone");
  // Revived: back up slowly.
  in.alive = 4;
  assert(run(d, in, MusicDirector::IMPROVE_HOLD_S - 0.5f) == "city.alone");
  assert(run(d, in, 1.0f) == "city.confident");
  // At home, Weakened and Alone share the landing zone's second song.
  in.distToShip = 100.0f;
  in.alive = 1;
  assert(run(d, in, 10.0f) == "landing.alone");
  in.alive = 2;
  assert(run(d, in, 10.0f) == "landing.weakened");
}

void testSolo() {
  MusicDirector d = director();
  MusicDirector::Inputs in = round1(1, 1, 3000.0f, "tidal");
  pastFirstContact(d, in);
  in.alive = 0;  // down: still Confident in single player
  assert(run(d, in, 10.0f) == "tidal.confident");
}

void testBoss() {
  MusicDirector d = director();
  MusicDirector::Inputs in = round1();
  in.phase = MatchPhase::BossFight;
  in.shipBiome = "desert";
  assert(d.update(in, 0.016f) == "desert.boss");
  in.shipBiome = "tidal";  // no boss track of its own yet
  assert(d.update(in, 0.016f) == "boss");
  in.phase = MatchPhase::Victory;
  assert(d.update(in, 0.016f).empty());  // let it play out
}

}  // namespace

int main() {
  testMoods();
  testLobbyAndIntermission();
  testFirstContact();
  testPlaces();
  testCrashSiteEdge();
  testRadio();
  testDeaths();
  testSolo();
  testBoss();
  std::cout << "MusicDirector: all rules hold\n";
  return 0;
}
