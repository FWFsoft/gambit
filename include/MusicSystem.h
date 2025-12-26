#pragma once

#include <SDL_mixer.h>

#include <string>
#include <unordered_map>

#include "MusicZone.h"

// Forward declarations
class ClientPrediction;
class TiledMap;
struct UpdateEvent;

class MusicSystem {
 public:
  // Audio configuration constants
  static constexpr int AUDIO_FREQUENCY = 44100;  // CD quality audio
  static constexpr int AUDIO_CHANNELS = 2;       // Stereo
  static constexpr int AUDIO_CHUNK_SIZE = 2048;  // Sample buffer size
  static constexpr int DEFAULT_FADE_MS = 500;    // Default crossfade duration
  static constexpr float DEFAULT_VOLUME = 0.5f;  // 50% volume
  static constexpr int SDL_MIXER_MAX_VOLUME = 128;  // SDL_mixer volume range
  static constexpr int LOOP_FOREVER = -1;           // SDL_mixer loop infinite

  MusicSystem(const ClientPrediction* prediction, const TiledMap* map);
  ~MusicSystem();

  // Manually trigger music change (for future event-based system)
  void playTrack(const std::string& trackName, int fadeMs = DEFAULT_FADE_MS);
  void stopMusic(int fadeMs = DEFAULT_FADE_MS);
  void setVolume(float volume);  // 0.0-1.0

 private:
  const ClientPrediction* clientPrediction;
  const TiledMap* tiledMap;

  std::unordered_map<std::string, Mix_Music*> loadedTracks;
  std::string currentTrack;
  std::string currentZoneName;
  float volume;

  void onUpdate(const UpdateEvent& e);
  void checkZoneTransition();
  Mix_Music* loadMusic(const std::string& filename);
};
