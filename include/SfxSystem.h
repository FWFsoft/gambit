#pragma once

#include <SDL_mixer.h>

#include <array>
#include <random>
#include <vector>

#include "HitSounds.h"

class ClientPrediction;
struct NetworkPacketReceivedEvent;
struct UpdateEvent;
struct ToggleMuteEvent;

// Plays sound effects on SDL_mixer channels beside the music. For now only
// hit feedback (HitSounds decides what plays). Construct after MusicSystem,
// which opens the audio device.
class SfxSystem {
 public:
  static constexpr int CHANNEL_COUNT = 24;
  static constexpr float MASTER_VOLUME = 0.7f;
  static constexpr int SDL_MIXER_MAX_VOLUME = 128;

  explicit SfxSystem(const ClientPrediction *prediction);
  ~SfxSystem();

 private:
  struct SoundBank {
    std::vector<Mix_Chunk *>
        variants;  // picked at random, never twice in a row
    int lastVariant = -1;
    float lastPlayMs = -1e9f;
    std::vector<int> channels;  // channels it may still be playing on
  };

  const ClientPrediction *clientPrediction;
  std::array<SoundBank, static_cast<size_t>(HitSounds::Sound::Count)> banks;
  float timeMs = 0.0f;
  bool muted = false;
  bool ready = false;
  std::mt19937 rng{std::random_device{}()};

  void load(HitSounds::Sound sound, const char *stem, int variants);
  void onPacket(const NetworkPacketReceivedEvent &e);
  void play(const HitSounds::Cue &cue);
};
