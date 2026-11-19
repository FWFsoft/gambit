#include "SfxSystem.h"

#include <algorithm>
#include <cassert>
#include <string>

#include "ClientPrediction.h"
#include "EventBus.h"
#include "Logger.h"
#include "NetworkProtocol.h"

using HitSounds::Sound;

SfxSystem::SfxSystem(const ClientPrediction *prediction)
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

  // assets/sfx/: see docs/design/SFX.md. Three pitch variants each.
  load(Sound::EnemyHit, "hit", 3);
  load(Sound::EnemyCrit, "crit", 3);
  load(Sound::LocalHurt, "hurt", 3);
  ready = true;

  EventBus::instance().subscribe<NetworkPacketReceivedEvent>(
      [this](const NetworkPacketReceivedEvent &e) { onPacket(e); });
  EventBus::instance().subscribe<UpdateEvent>(
      [this](const UpdateEvent &e) { timeMs += e.deltaTime; });
  EventBus::instance().subscribe<ToggleMuteEvent>(
      [this](const ToggleMuteEvent &) { muted = !muted; });
  Logger::info("SfxSystem initialized");
}

SfxSystem::~SfxSystem() {
  Mix_HaltChannel(-1);
  for (SoundBank &bank : banks) {
    for (Mix_Chunk *chunk : bank.variants) Mix_FreeChunk(chunk);
    bank.variants.clear();
  }
}

void SfxSystem::load(Sound sound, const char *stem, int variants) {
  SoundBank &bank = banks[static_cast<size_t>(sound)];
  for (int i = 1; i <= variants; i++) {
    std::string path =
        std::string("assets/sfx/") + stem + "_" + std::to_string(i) + ".ogg";
    Mix_Chunk *chunk = Mix_LoadWAV(path.c_str());
    if (!chunk) {
      Logger::error("SfxSystem: failed to load " + path + ": " +
                    Mix_GetError());
      continue;
    }
    bank.variants.push_back(chunk);
  }
}

void SfxSystem::onPacket(const NetworkPacketReceivedEvent &e) {
  // In embedded mode the server shares this EventBus; packets travelling
  // client -> server carry a non-zero client id and are not for us.
  if (!ready || muted || e.size == 0 || e.clientId != 0) return;
  if (static_cast<PacketType>(e.data[0]) != PacketType::GameEvent) return;

  GameEventPacket packet = deserializeGameEvent(e.data, e.size);
  const Player &local = clientPrediction->getLocalPlayer();
  play(HitSounds::choose(packet, clientPrediction->getLocalPlayerId(), local.x,
                         local.y));
}

void SfxSystem::play(const HitSounds::Cue &cue) {
  if (cue.sound == Sound::None) return;
  SoundBank &bank = banks[static_cast<size_t>(cue.sound)];
  if (bank.variants.empty()) return;

  HitSounds::Limits limits = HitSounds::limitsFor(cue.sound);
  if (timeMs - bank.lastPlayMs < limits.minGapMs) return;
  bank.channels.erase(
      std::remove_if(bank.channels.begin(), bank.channels.end(),
                     [](int ch) { return Mix_Playing(ch) == 0; }),
      bank.channels.end());
  if (static_cast<int>(bank.channels.size()) >= limits.maxVoices) return;

  int count = static_cast<int>(bank.variants.size());
  int pick = std::uniform_int_distribution<int>(0, count - 1)(rng);
  if (count > 1 && pick == bank.lastVariant) pick = (pick + 1) % count;

  int channel = Mix_PlayChannel(-1, bank.variants[pick], 0);
  if (channel < 0) return;  // every channel busy: drop it
  bank.lastVariant = pick;
  bank.lastPlayMs = timeMs;
  bank.channels.push_back(channel);

  Mix_Volume(channel, static_cast<int>(cue.volume * MASTER_VOLUME *
                                       SDL_MIXER_MAX_VOLUME));
  // Constant-power-ish pan: the far side drops, the near side stays full.
  float left = cue.pan > 0.0f ? 1.0f - 0.7f * cue.pan : 1.0f;
  float right = cue.pan < 0.0f ? 1.0f + 0.7f * cue.pan : 1.0f;
  Mix_SetPanning(channel, static_cast<Uint8>(left * 255.0f),
                 static_cast<Uint8>(right * 255.0f));
}
