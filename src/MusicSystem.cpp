#include "MusicSystem.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <sstream>

#include "ArenaClient.h"
#include "Biome.h"
#include "ClientPrediction.h"
#include "EventBus.h"
#include "GameStateManager.h"
#include "Logger.h"
#include "NetworkProtocol.h"
#include "Player.h"
#include "TiledMap.h"

MusicSystem::MusicSystem(const ClientPrediction* prediction,
                         const TiledMap* map)
    : clientPrediction(prediction),
      tiledMap(map),
      volume(DEFAULT_VOLUME),
      director(
          [this](const std::string& key) { return playlists.count(key) > 0; }) {
  loadPlaylists();

  // Initialize SDL_mixer
  if (Mix_OpenAudio(AUDIO_FREQUENCY, MIX_DEFAULT_FORMAT, AUDIO_CHANNELS,
                    AUDIO_CHUNK_SIZE) < 0) {
    Logger::error("SDL_mixer init failed: " + std::string(Mix_GetError()));
    return;
  }

  applyVolume();

  Logger::info("MusicSystem initialized");

  // Subscribe to UpdateEvent
  EventBus::instance().subscribe<UpdateEvent>(
      [this](const UpdateEvent& e) { onUpdate(e); });

  // The player's audio settings (UISystem): master x music, and mute.
  EventBus::instance().subscribe<AudioSettingsChangedEvent>(
      [this](const AudioSettingsChangedEvent& e) {
        settingsGain = e.musicGain;
        applyVolume();
      });
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

void MusicSystem::onUpdate(const UpdateEvent& e) {
  if (arenaClient && hasPlaylists()) {
    followMatch(e.deltaTime / 1000.0f);  // deltaTime is in milliseconds
  } else {
    checkZoneTransition();
  }
}

void MusicSystem::loadPlaylists() {
  std::ifstream file(PLAYLIST_FILE);
  if (!file) {
    Logger::info(std::string("No playlists (") + PLAYLIST_FILE +
                 "): music follows the map's music zones");
    return;
  }
  std::string line;
  int tracks = 0;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line);
    std::string key, path;
    row >> key;
    std::getline(row >> std::ws, path);
    if (key.empty() || path.empty()) continue;
    playlists[key].push_back(path);
    tracks++;
  }
  // "boss": every biome's boss music, for a biome without its own.
  if (!playlists.count("boss")) {
    std::vector<std::string> all;
    for (const auto& [key, files] : playlists) {
      if (key.size() > 5 && key.compare(key.size() - 5, 5, ".boss") == 0) {
        all.insert(all.end(), files.begin(), files.end());
      }
    }
    if (!all.empty()) playlists["boss"] = all;
  }
  Logger::info("Loaded " + std::to_string(playlists.size()) + " playlists (" +
               std::to_string(tracks) + " tracks)");
}

MusicDirector::Inputs MusicSystem::readMatch() const {
  MusicDirector::Inputs in;
  if (!arenaClient || !arenaClient->hasMatch()) return in;
  const MatchStatePacket& match = arenaClient->match();
  in.hasMatch = true;
  in.phase = static_cast<Arena::MatchPhase>(match.phase);
  in.round = match.round;
  const uint32_t me = clientPrediction->getLocalPlayerId();
  const Player& local = clientPrediction->getLocalPlayer();
  for (const MatchPlayerState& p : match.players) {
    if (p.characterId == 0) continue;  // not playing (still picking)
    in.partySize++;
    bool alive = true;
    if (p.playerId == me) {
      alive = !local.isDead();
    } else {
      auto it = arenaClient->players().find(p.playerId);
      if (it != arenaClient->players().end()) {
        alive = it->second.state.health > 0.0f;
      }
    }
    if (alive) in.alive++;
  }
  if (in.partySize == 0) in.partySize = in.alive = 1;
  in.distToShip = std::hypot(local.x - match.shipX, local.y - match.shipY);
  const TerrainGrid& grid = tiledMap->terrain();
  in.biome = biomeKey(biomeFromId(grid.biomeAt(local.x, local.y)));
  in.shipBiome = biomeKey(biomeFromId(grid.biomeAt(match.shipX, match.shipY)));
  return in;
}

void MusicSystem::followMatch(float dtSeconds) {
  GameState state = GameStateManager::instance().getCurrentState();
  if (state == GameState::TitleScreen || state == GameState::MainMenu) {
    // The title screen plays its own music (UISystem).
    currentPlaylist.clear();
    pendingPlaylist.clear();
    return;
  }
  // Our track finished (each plays once; the playlist moves on).
  const bool ended = !currentPlaylist.empty() && pendingPlaylist.empty() &&
                     !Mix_PlayingMusic();

  MusicDirector::Inputs in = readMatch();
  in.characterSelect = state == GameState::CharacterSelect;
  in.trackEnded = ended;
  std::string want = director.update(in, dtSeconds);
  // The radio's clock runs through the match; a new match starts it again.
  if (in.characterSelect ||
      (in.hasMatch && in.phase == Arena::MatchPhase::Lobby)) {
    channels.clear();
    radioClock = 0.0;
  } else {
    radioClock += dtSeconds;
  }

  if (!want.empty() && (want != currentPlaylist || !pendingPlaylist.empty())) {
    if (want != pendingPlaylist) {
      pendingPlaylist = want;
      Logger::info(
          "Music: " +
          (currentPlaylist.empty() ? std::string("-") : currentPlaylist) +
          " -> " + want);
    }
    if (Mix_PlayingMusic() && Mix_FadingMusic() != MIX_FADING_OUT) {
      bool dial = isRadioPlaylist(currentPlaylist) && isRadioPlaylist(want);
      Mix_FadeOutMusic(dial ? RADIO_FADE_MS : PLAYLIST_FADE_MS);
    }
  }
  if (!pendingPlaylist.empty()) {
    // Never start over a fading track: Mix_FadeInMusic would block on it.
    if (!Mix_PlayingMusic()) {
      std::string next = pendingPlaylist;
      pendingPlaylist.clear();
      startPlaylist(next);
    }
    return;
  }
  if (ended) {
    // Next track, same playlist. On the radio the channel moves on too.
    auto channel = channels.find(currentPlaylist);
    auto files = playlists.find(currentPlaylist);
    if (channel != channels.end() && files != playlists.end()) {
      channel->second = {pickTrack(files->second, currentTrack), radioClock};
    }
    startPlaylist(currentPlaylist);
  }
}

std::string MusicSystem::pickTrack(const std::vector<std::string>& files,
                                   const std::string& avoid) {
  assert(!files.empty());
  // Shuffle, but never the same track twice in a row.
  std::string file = files[std::uniform_int_distribution<size_t>(
      0, files.size() - 1)(shuffle)];
  if (files.size() > 1) {
    while (file == avoid) {
      file = files[std::uniform_int_distribution<size_t>(
          0, files.size() - 1)(shuffle)];
    }
  }
  return file;
}

void MusicSystem::startPlaylist(const std::string& key) {
  auto it = playlists.find(key);
  if (it == playlists.end() || it->second.empty()) return;
  const std::vector<std::string>& files = it->second;
  const bool radio = isRadioPlaylist(key);

  std::string file;
  double position = 0.0;
  if (!radio) {
    file = pickTrack(files, currentTrack);
  } else {
    auto channel = channels.find(key);
    if (channel == channels.end()) {
      // A channel first tuned in mid-match has been playing all along:
      // join a track somewhere in its first two thirds.
      file = pickTrack(files, "");
      Mix_Music* probe = loadMusic(file);
      double duration = probe ? musicDuration(probe) : 0.0;
      position = duration > 0.0
                     ? std::uniform_real_distribution<double>(
                           0.0, duration * 0.66)(shuffle)
                     : 0.0;
      channels[key] = {file, radioClock - position};
    } else {
      Mix_Music* playing = loadMusic(channel->second.file);
      double duration = playing ? musicDuration(playing) : 0.0;
      RadioTune tune =
          radioTune(channel->second.startedAt, radioClock, duration);
      if (tune.onAir) {
        file = channel->second.file;
        position = tune.position;
      } else {
        // That track ended while we were away: the channel's next one,
        // from its start.
        file = pickTrack(files, channel->second.file);
        channel->second = {file, radioClock};
      }
    }
  }

  Mix_Music* music = loadMusic(file);
  if (!music) return;
  const bool dial = radio && isRadioPlaylist(currentPlaylist);
  // Play once (0 and 1 both mean once across SDL_mixer versions): the end
  // of a track is when the playlist moves on.
  if (Mix_FadeInMusicPos(music, 1, dial ? RADIO_FADE_MS : PLAYLIST_FADE_MS,
                         position) == -1) {
    Logger::error("Failed to play music: " + std::string(Mix_GetError()));
    return;
  }
  if (radio && position > 0.0) {
    Logger::info("Music: " + key + " picks up " + file + " at " +
                 std::to_string(static_cast<int>(position)) + "s");
  }
  currentTrack = file;
  currentPlaylist = key;
}

void MusicSystem::checkZoneTransition() {
  // Only check zone transitions when actively playing the game
  // Don't interfere with title screen or menu music
  GameState currentState = GameStateManager::instance().getCurrentState();
  if (currentState != GameState::Playing) {
    return;
  }

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
  applyVolume();
}

void MusicSystem::applyVolume() {
  float gain = volume * settingsGain;
  assert(gain >= 0.0f && gain <= 1.0f);
  Mix_VolumeMusic(static_cast<int>(gain * SDL_MIXER_MAX_VOLUME));
}

double MusicSystem::musicDuration(Mix_Music* music) {
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
  return Mix_MusicDuration(music);
#else
  (void)music;
  return 0.0;  // no seeking on old SDL_mixer: channels restart
#endif
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
