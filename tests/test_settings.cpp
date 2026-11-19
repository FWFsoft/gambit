// Audio settings (Settings): the slider curve, master x channel, mute, and
// a hand-edited settings file clamped into range. See docs/design/SFX.md.

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>

#include "Settings.h"

namespace {

bool near(float a, float b) { return std::abs(a - b) < 1e-4f; }

void fullByDefault() {
  Settings s;
  assert(near(s.musicGain(), 1.0f));
  assert(near(s.sfxGain(), 1.0f));
}

void slidersFollowAPerceptualCurve() {
  // Half way sounds about half as loud: a quarter of the amplitude.
  assert(near(Settings::perceived(0.5f), 0.25f));
  assert(near(Settings::perceived(0.0f), 0.0f));
  assert(near(Settings::perceived(1.0f), 1.0f));
  assert(near(Settings::perceived(1.5f), 1.0f));  // clamped
}

void masterScalesBothChannels() {
  Settings s;
  s.masterVolume = 0.5f;
  s.musicVolume = 1.0f;
  s.sfxVolume = 0.5f;
  assert(near(s.musicGain(), 0.25f));
  assert(near(s.sfxGain(), 0.0625f));
  s.sfxVolume = 0.0f;
  assert(near(s.sfxGain(), 0.0f));
  assert(near(s.musicGain(), 0.25f));  // music unaffected
}

void muteSilencesEverything() {
  Settings s;
  s.muted = true;
  assert(s.musicGain() == 0.0f && s.sfxGain() == 0.0f);
}

void loadClampsAndRoundTrips() {
  const char* path = "test_settings_tmp.ini";
  {
    std::ofstream f(path);
    f << "masterVolume=3\nmusicVolume=-1\nsfxVolume=0.4\nmuted=false\n";
  }
  Settings s;
  assert(s.load(path));
  assert(s.masterVolume == 1.0f && s.musicVolume == 0.0f);
  assert(near(s.sfxVolume, 0.4f));
  s.muted = true;
  assert(s.save(path));
  Settings back;
  assert(back.load(path));
  assert(back.muted && near(back.sfxVolume, 0.4f));
  std::remove(path);
}

}  // namespace

int main() {
  fullByDefault();
  slidersFollowAPerceptualCurve();
  masterScalesBothChannels();
  muteSilencesEverything();
  loadClampsAndRoundTrips();
  std::cout << "test_settings: all passed\n";
  return 0;
}
