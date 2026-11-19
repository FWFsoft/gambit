#include "SfxSystem.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <string>

#include "ClientPrediction.h"
#include "EventBus.h"
#include "Logger.h"
#include "NetworkProtocol.h"

using AbilitySounds::Shape;
using HitSounds::Sound;

SfxSystem::SfxSystem(const ClientPrediction* prediction)
    : clientPrediction(prediction) {
  assert(prediction != nullptr);
  int frequency = 0;
  Uint16 format = 0;
  int channels = 0;
  if (Mix_QuerySpec(&frequency, &format, &channels) == 0) {
    Logger::error("SfxSystem: audio device not open, no sound effects");
    return;
  }
  Mix_AllocateChannels(CHANNEL_COUNT);

  // assets/sfx/: see docs/design/SFX.md.
  load(hits[static_cast<size_t>(Sound::EnemyHit)], "assets/sfx/hit");
  load(hits[static_cast<size_t>(Sound::EnemyCrit)], "assets/sfx/crit");
  load(hits[static_cast<size_t>(Sound::LocalHurt)], "assets/sfx/hurt");
  for (int s = static_cast<int>(Shape::Generic);
       s < static_cast<int>(Shape::Count); s++) {
    Shape shape = static_cast<Shape>(s);
    load(shapes[s], std::string("assets/sfx/ability/shape_") +
                        AbilitySounds::shapeName(shape));
  }
  for (int t = 0; t < 256; t++) {
    const char* name = AbilitySounds::tintName(static_cast<VfxTheme>(t));
    if (name) load(tints[t], std::string("assets/sfx/ability/tint_") + name);
  }
  ready = true;

  EventBus::instance().subscribe<NetworkPacketReceivedEvent>(
      [this](const NetworkPacketReceivedEvent& e) { onPacket(e); });
  EventBus::instance().subscribe<UpdateEvent>(
      [this](const UpdateEvent& e) { onUpdate(e); });
  // The player's audio settings (UISystem): master x sound effects, and mute.
  EventBus::instance().subscribe<AudioSettingsChangedEvent>(
      [this](const AudioSettingsChangedEvent& e) { settingsGain = e.sfxGain; });
  Logger::info("SfxSystem initialized");
}

SfxSystem::~SfxSystem() {
  Mix_HaltChannel(-1);
  auto freeAll = [](auto& banks) {
    for (SoundBank& bank : banks) {
      for (Mix_Chunk* chunk : bank.variants) Mix_FreeChunk(chunk);
      bank.variants.clear();
    }
  };
  freeAll(hits);
  freeAll(shapes);
  freeAll(tints);
}

void SfxSystem::load(SoundBank& bank, const std::string& stem) {
  for (int i = 1; i <= MAX_VARIANTS; i++) {
    std::string path = stem + "_" + std::to_string(i) + ".ogg";
    if (!std::ifstream(path)) break;
    Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
    if (!chunk) {
      Logger::error("SfxSystem: failed to load " + path + ": " +
                    Mix_GetError());
      continue;
    }
    bank.variants.push_back(chunk);
  }
  if (bank.variants.empty()) {
    Logger::error("SfxSystem: no sounds for " + stem);
  }
}

void SfxSystem::onPacket(const NetworkPacketReceivedEvent& e) {
  // In embedded mode the server shares this EventBus; packets travelling
  // client -> server carry a non-zero client id and are not for us.
  if (!ready || e.size == 0 || e.clientId != 0) return;
  if (static_cast<PacketType>(e.data[0]) != PacketType::GameEvent) return;
  onGameEvent(deserializeGameEvent(e.data, e.size));
}

void SfxSystem::onGameEvent(const GameEventPacket& event) {
  const Player& local = clientPrediction->getLocalPlayer();
  uint32_t localId = clientPrediction->getLocalPlayerId();

  for (const AbilitySounds::Cue& cue :
       grouper.onEvent(event, timeMs, localId, local.x, local.y)) {
    playCast(cue);
  }

  HitSounds::Cue hit = HitSounds::choose(event, localId, local.x, local.y);
  if (hit.sound == Sound::None) return;
  HitSounds::Limits limits = HitSounds::limitsFor(hit.sound);
  play(hits[static_cast<size_t>(hit.sound)], limits.minGapMs, limits.maxVoices,
       hit.volume, hit.pan);
}

void SfxSystem::onUpdate(const UpdateEvent& e) {
  timeMs += e.deltaTime;
  if (!ready) return;
  const Player& local = clientPrediction->getLocalPlayer();
  for (const AbilitySounds::Cue& cue : grouper.onTick(
           timeMs, clientPrediction->getLocalPlayerId(), local.x, local.y)) {
    playCast(cue);
  }
}

void SfxSystem::playCast(const AbilitySounds::Cue& cue) {
  if (cue.shape == Shape::None) return;
  AbilitySounds::Limits limits = AbilitySounds::limitsFor(cue.shape);
  play(shapes[static_cast<size_t>(cue.shape)], limits.minGapMs,
       limits.maxVoices, cue.volume, cue.pan);
  if (AbilitySounds::hasTint(cue.theme)) {
    AbilitySounds::Limits tint = AbilitySounds::tintLimits();
    play(tints[static_cast<size_t>(cue.theme)], tint.minGapMs, tint.maxVoices,
         cue.volume * AbilitySounds::TINT_VOLUME, cue.pan);
  }
}

void SfxSystem::play(SoundBank& bank, float minGapMs, int maxVoices,
                     float volume, float pan) {
  float gain = volume * MASTER_VOLUME * settingsGain;
  assert(gain >= 0.0f && gain <= 1.0f);
  if (bank.variants.empty() || gain <= 0.0f) return;
  if (timeMs - bank.lastPlayMs < minGapMs) return;
  bank.channels.erase(
      std::remove_if(bank.channels.begin(), bank.channels.end(),
                     [](int ch) { return Mix_Playing(ch) == 0; }),
      bank.channels.end());
  if (static_cast<int>(bank.channels.size()) >= maxVoices) return;

  int count = static_cast<int>(bank.variants.size());
  int pick = std::uniform_int_distribution<int>(0, count - 1)(rng);
  if (count > 1 && pick == bank.lastVariant) pick = (pick + 1) % count;

  int channel = Mix_PlayChannel(-1, bank.variants[pick], 0);
  if (channel < 0) return;  // every channel busy: drop it
  bank.lastVariant = pick;
  bank.lastPlayMs = timeMs;
  bank.channels.push_back(channel);

  Mix_Volume(channel, static_cast<int>(gain * SDL_MIXER_MAX_VOLUME));
  // The far side drops, the near side stays full.
  float left = pan > 0.0f ? 1.0f - 0.7f * pan : 1.0f;
  float right = pan < 0.0f ? 1.0f + 0.7f * pan : 1.0f;
  Mix_SetPanning(channel, static_cast<Uint8>(left * 255.0f),
                 static_cast<Uint8>(right * 255.0f));
}
