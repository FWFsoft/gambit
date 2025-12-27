#include "MusicSystem.h"

#include <cassert>

#include "ClientPrediction.h"
#include "EventBus.h"
#include "Logger.h"
#include "Player.h"
#include "TiledMap.h"

MusicSystem::MusicSystem(const ClientPrediction* prediction,
                         const TiledMap* map)
    : clientPrediction(prediction), tiledMap(map), volume(DEFAULT_VOLUME),
      muted(false) {
  // Initialize SDL_mixer
  if (Mix_OpenAudio(AUDIO_FREQUENCY, MIX_DEFAULT_FORMAT, AUDIO_CHANNELS,
                    AUDIO_CHUNK_SIZE) < 0) {
    Logger::error("SDL_mixer init failed: " + std::string(Mix_GetError()));
    return;
  }

  Mix_VolumeMusic(static_cast<int>(volume * SDL_MIXER_MAX_VOLUME));

  Logger::info("MusicSystem initialized");

  // Subscribe to UpdateEvent
  EventBus::instance().subscribe<UpdateEvent>(
      [this](const UpdateEvent& e) { onUpdate(e); });

  // Subscribe to ToggleMuteEvent
  EventBus::instance().subscribe<ToggleMuteEvent>(
      [this](const ToggleMuteEvent& e) { onToggleMute(e); });
}

MusicSystem::~MusicSystem() {
  // Free all loaded music
  for (auto& [name, music] : loadedTracks) {
    if (music) {
      Mix_FreeMusic(music);
    }
  }
  loadedTracks.clear();

  Mix_CloseAudio();
}

void MusicSystem::onUpdate(const UpdateEvent& e) { checkZoneTransition(); }

void MusicSystem::checkZoneTransition() {
  assert(clientPrediction != nullptr && "ClientPrediction is null");
  assert(tiledMap != nullptr && "TiledMap is null");

  const Player& player = clientPrediction->getLocalPlayer();
  const auto& zones = tiledMap->getMusicZones();

  // Find which zone player is in (if any)
  std::string newZoneName;
  std::string newTrackName;

  for (const auto& zone : zones) {
    if (zone.contains(player.x, player.y)) {
      newZoneName = zone.name;
      newTrackName = zone.trackName;
      break;  // Use first matching zone
    }
  }

  // Check if zone changed
  if (newZoneName != currentZoneName) {
    currentZoneName = newZoneName;

    if (newTrackName.empty()) {
      // Left all zones - fade out music
      stopMusic();
      Logger::info("Left music zone, fading out");
    } else {
      // Entered new zone - change track
      playTrack(newTrackName);
      Logger::info("Entered zone '" + newZoneName +
                   "', playing: " + newTrackName);
    }
  }
}

void MusicSystem::playTrack(const std::string& trackName, int fadeMs) {
  if (trackName == currentTrack) {
    return;  // Already playing this track
  }

  // Load music if not already loaded
  Mix_Music* music = loadMusic(trackName);
  if (!music) {
    Logger::error("Failed to load music: " + trackName);
    return;
  }

  // Mix_FadeInMusic will automatically stop any currently playing music
  // Use a shorter fade for smoother transitions (200ms instead of 500ms)
  int actualFadeMs = fadeMs / 2;  // Halve the fade time for quicker transitions

  if (Mix_FadeInMusic(music, LOOP_FOREVER, actualFadeMs) == -1) {
    Logger::error("Failed to play music: " + std::string(Mix_GetError()));
    return;
  }

  currentTrack = trackName;
}

void MusicSystem::stopMusic(int fadeMs) {
  if (!currentTrack.empty() && Mix_PlayingMusic()) {
    Mix_FadeOutMusic(fadeMs);
    currentTrack.clear();
  }
}

void MusicSystem::setVolume(float vol) {
  assert(vol >= 0.0f && vol <= 1.0f && "Volume out of range");
  volume = std::clamp(vol, 0.0f, 1.0f);
  Mix_VolumeMusic(static_cast<int>(volume * SDL_MIXER_MAX_VOLUME));
}

Mix_Music* MusicSystem::loadMusic(const std::string& filename) {
  assert(!filename.empty() && "Music filename is empty");

  // Check cache first
  auto it = loadedTracks.find(filename);
  if (it != loadedTracks.end()) {
    return it->second;
  }

  // Load from assets/music/
  std::string filepath = "assets/music/" + filename;
  Mix_Music* music = Mix_LoadMUS(filepath.c_str());

  if (!music) {
    Logger::error("Mix_LoadMUS failed for " + filepath + ": " +
                  std::string(Mix_GetError()));
    return nullptr;
  }

  loadedTracks[filename] = music;
  Logger::info("Loaded music: " + filepath);
  return music;
}

void MusicSystem::onToggleMute(const ToggleMuteEvent& e) {
  muted = !muted;

  if (muted) {
    Mix_VolumeMusic(0);
    Logger::info("Music muted");
  } else {
    Mix_VolumeMusic(static_cast<int>(volume * SDL_MIXER_MAX_VOLUME));
    Logger::info("Music unmuted");
  }
}
