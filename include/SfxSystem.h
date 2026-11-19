#pragma once

#include <SDL_mixer.h>

#include <array>
#include <random>
#include <string>
#include <vector>

#include "AbilitySounds.h"
#include "HitSounds.h"

class ClientPrediction;
struct NetworkPacketReceivedEvent;
struct UpdateEvent;

// Plays sound effects on SDL_mixer channels beside the music: hit feedback
// (HitSounds) and ability casts (AbilitySounds). Construct after
// MusicSystem, which opens the audio device. docs/design/SFX.md.
class SfxSystem {
 public:
  static constexpr int CHANNEL_COUNT = 32;
  static constexpr float MASTER_VOLUME = 0.7f;
  static constexpr int SDL_MIXER_MAX_VOLUME = 128;
  static constexpr int MAX_VARIANTS = 8;

  explicit SfxSystem(const ClientPrediction* prediction);
  ~SfxSystem();

 private:
  struct SoundBank {
    std::vector<Mix_Chunk*> variants;  // picked at random, never twice running
    int lastVariant = -1;
    float lastPlayMs = -1e9f;
    std::vector<int> channels;  // channels it may still be playing on
  };

  const ClientPrediction* clientPrediction;
  std::array<SoundBank, static_cast<size_t>(HitSounds::Sound::Count)> hits;
  std::array<SoundBank, static_cast<size_t>(AbilitySounds::Shape::Count)>
      shapes;
  std::array<SoundBank, 256> tints;  // by VfxTheme
  AbilitySounds::CastGrouper grouper;
  float timeMs = 0.0f;
  // The player's master x sound effects setting (AudioSettingsChangedEvent).
  float settingsGain = 1.0f;
  bool ready = false;
  std::mt19937 rng{std::random_device{}()};

  // Loads <stem>_1.ogg, <stem>_2.ogg ... until one is missing.
  void load(SoundBank& bank, const std::string& stem);
  void onPacket(const NetworkPacketReceivedEvent& e);
  void onGameEvent(const GameEventPacket& event);
  void onUpdate(const UpdateEvent& e);
  void playCast(const AbilitySounds::Cue& cue);
  void play(SoundBank& bank, float minGapMs, int maxVoices, float volume,
            float pan);
};
