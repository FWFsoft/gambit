#include "MusicDirector.h"

#include <cassert>
#include <utility>

const char* moodKey(MusicMood mood) {
  switch (mood) {
    case MusicMood::Confident:
      return "confident";
    case MusicMood::Weakened:
      return "weakened";
    case MusicMood::Alone:
      return "alone";
  }
  assert(false && "unknown mood");
  return "confident";
}

MusicMood moodFor(int partySize, int alive) {
  assert(partySize >= 0 && alive >= 0);
  // Solo play is always Confident; otherwise everyone standing is
  // Confident, the last one standing is Alone, anything between Weakened.
  if (partySize <= 1 || alive >= partySize) return MusicMood::Confident;
  if (alive <= 1) return MusicMood::Alone;
  return MusicMood::Weakened;
}

MusicDirector::MusicDirector(
    std::function<bool(const std::string&)> hasPlaylist)
    : has(std::move(hasPlaylist)) {
  assert(has);
}

bool MusicDirector::hasPlaceMusic(const std::string& place) const {
  if (place.empty()) return false;
  return has(place + ".confident") || has(place + ".weakened") ||
         has(place + ".alone");
}

std::string MusicDirector::placePlaylist(const std::string& place,
                                         MusicMood mood) const {
  // The mood's own playlist, else the nearest one the place has.
  static const MusicMood order[3][3] = {
      {MusicMood::Confident, MusicMood::Weakened, MusicMood::Alone},
      {MusicMood::Weakened, MusicMood::Alone, MusicMood::Confident},
      {MusicMood::Alone, MusicMood::Weakened, MusicMood::Confident}};
  for (MusicMood m : order[static_cast<int>(mood)]) {
    std::string key = place + "." + moodKey(m);
    if (has(key)) return key;
  }
  return "";
}

void MusicDirector::track(const Inputs& in, float dt) {
  std::string place;
  const float landingReach =
      settledPlace == "landing" ? LANDING_EXIT_RADIUS : LANDING_RADIUS;
  if (in.distToShip <= landingReach && hasPlaceMusic("landing")) {
    place = "landing";
  } else if (hasPlaceMusic(in.biome)) {
    place = in.biome;
  }
  // Somewhere without music of its own (a biome not scored yet): keep
  // what plays.
  MusicMood mood = moodFor(in.partySize, in.alive);

  if (!settled) {
    settledPlace = place.empty() ? "landing" : place;
    settledMood = mood;
    pendingPlace.clear();
    placeTimer = moodTimer = 0.0f;
    settled = true;
    return;
  }

  if (!place.empty() && place != settledPlace) {
    if (place == pendingPlace) {
      placeTimer += dt;
    } else {
      pendingPlace = place;
      placeTimer = dt;
    }
    if (placeTimer >= ZONE_HOLD_S) {
      settledPlace = place;
      pendingPlace.clear();
      placeTimer = 0.0f;
    }
  } else {
    pendingPlace.clear();
    placeTimer = 0.0f;
  }

  if (mood != settledMood) {
    if (mood == pendingMood) {
      moodTimer += dt;
    } else {
      pendingMood = mood;
      moodTimer = dt;
    }
    bool worse = static_cast<int>(mood) > static_cast<int>(settledMood);
    if (moodTimer >= (worse ? WORSEN_HOLD_S : IMPROVE_HOLD_S)) {
      settledMood = mood;
      moodTimer = 0.0f;
    }
  } else {
    pendingMood = mood;
    moodTimer = 0.0f;
  }
}

void MusicDirector::reset() {
  firstContactPlaying = firstContactDone = settled = false;
  settledPlace.clear();
  pendingPlace.clear();
  placeTimer = moodTimer = 0.0f;
  settledMood = pendingMood = MusicMood::Confident;
  current.clear();
}

std::string MusicDirector::update(const Inputs& in, float dt) {
  assert(dt >= 0.0f);
  using Arena::MatchPhase;
  std::string want;

  if (in.runEnded()) {
    reset();
    return "";
  }

  if (in.characterSelect || (in.hasMatch && in.phase == MatchPhase::Lobby)) {
    // Before a match: the next landing starts with First Contact again.
    firstContactPlaying = firstContactDone = false;
    settled = false;
    want = has("lobby") ? "lobby" : "";
  } else if (!in.hasMatch) {
    want = "";
  } else if (in.phase == MatchPhase::Intro) {
    // The crash intro: First Contact starts with it and plays on into
    // round 1 unbroken (the Round branch keeps asking for the same track).
    if (firstContactPlaying && in.trackEnded && current == "first_contact") {
      firstContactDone = true;
      firstContactPlaying = false;
    } else if (!firstContactDone && has("first_contact")) {
      firstContactPlaying = true;
    }
    want = firstContactPlaying ? "first_contact" : "";
  } else if (in.phase == MatchPhase::Intermission) {
    // Back at the ship. First Contact, if still going, is over for good.
    if (firstContactPlaying) firstContactDone = true;
    firstContactPlaying = false;
    settled = false;  // the next round settles where it starts
    want = has("lobby") ? "lobby" : "";
  } else if (in.phase == MatchPhase::Round) {
    track(in, dt);
    if (in.round > 1) firstContactDone = true;
    if (!firstContactDone && has("first_contact")) {
      if (firstContactPlaying && in.trackEnded && current == "first_contact") {
        firstContactDone = true;
        firstContactPlaying = false;
      } else {
        firstContactPlaying = true;
      }
    }
    want = firstContactPlaying ? "first_contact"
                               : placePlaylist(settledPlace, settledMood);
  } else if (in.phase == MatchPhase::BossFight) {
    firstContactPlaying = false;
    firstContactDone = true;
    if (!in.shipBiome.empty() && has(in.shipBiome + ".boss")) {
      want = in.shipBiome + ".boss";
    } else {
      want = has("boss") ? "boss" : "";
    }
  } else {
    want = "";
  }

  if (!want.empty()) current = want;
  return want;
}
