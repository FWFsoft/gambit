#pragma once

#include <algorithm>
#include <cassert>
#include <string>

struct Settings {
  // Audio settings
  float masterVolume = 1.0f;
  float musicVolume = 1.0f;
  float sfxVolume = 1.0f;
  bool muted = false;

  // Graphics settings
  int windowWidth = 1280;
  int windowHeight = 960;
  bool fullscreen = false;
  bool vsync = true;

  // What the audio systems apply (0..1): the sliders through a perceptual
  // curve, master times the channel, 0 when muted. docs/design/SFX.md.
  float musicGain() const { return gain(musicVolume); }
  float sfxGain() const { return gain(sfxVolume); }

  // A slider position (0..1) to an amplitude. Squared, so the middle of the
  // slider sounds about half as loud (-12 dB) rather than barely quieter.
  static float perceived(float slider) {
    float s = std::clamp(slider, 0.0f, 1.0f);
    return s * s;
  }

  // Save/Load
  bool load(const std::string& filename);
  bool save(const std::string& filename) const;

  // Default filename
  static constexpr const char* DEFAULT_FILENAME = "settings.ini";

 private:
  float gain(float channel) const {
    if (muted) return 0.0f;
    float g = perceived(masterVolume) * perceived(channel);
    assert(g >= 0.0f && g <= 1.0f);
    return g;
  }
};
