#pragma once

#include <SDL_mixer.h>

#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "MusicDirector.h"
#include "MusicZone.h"

// Forward declarations
class ArenaClient;
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

  // Crossfade between playlists: the old track fades out, then the new one
  // fades in.
  static constexpr int PLAYLIST_FADE_MS = 1500;
  static constexpr const char* PLAYLIST_FILE = "assets/music/music.txt";

  MusicSystem(const ClientPrediction* prediction, const TiledMap* map);
  ~MusicSystem();

  // The match as the client sees it. With it and a playlist file, music
  // follows the match (MusicDirector); without, the map's music zones.
  void setArena(const ArenaClient* arena) { arenaClient = arena; }
  bool hasPlaylists() const { return !playlists.empty(); }

  // Manually trigger music change (for future event-based system)
  void playTrack(const std::string& trackName, int fadeMs = DEFAULT_FADE_MS);
  void stopMusic(int fadeMs = DEFAULT_FADE_MS);
  void setVolume(float volume);  // 0.0-1.0

 private:
  const ClientPrediction* clientPrediction;
  const TiledMap* tiledMap;

  const ArenaClient* arenaClient = nullptr;

  std::unordered_map<std::string, Mix_Music*> loadedTracks;
  std::string currentTrack;
  std::string currentZoneName;
  float volume;
  // The player's master x music setting (AudioSettingsChangedEvent).
  float settingsGain = 1.0f;

  // Playlists (assets/music/music.txt): key -> files under assets/music/.
  std::unordered_map<std::string, std::vector<std::string>> playlists;
  MusicDirector director;
  std::string currentPlaylist;  // what our music is playing ("" = none)
  std::string pendingPlaylist;  // waiting for a fade out to finish
  std::mt19937 shuffle{std::random_device{}()};

  void onUpdate(const UpdateEvent& e);
  void applyVolume();  // volume x settingsGain to SDL_mixer
  void checkZoneTransition();
  void loadPlaylists();
  void followMatch(float dtSeconds);
  MusicDirector::Inputs readMatch() const;
  void startPlaylist(const std::string& key);
  Mix_Music* loadMusic(const std::string& filename);
};
