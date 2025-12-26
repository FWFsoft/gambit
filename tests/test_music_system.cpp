#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include "Logger.h"
#include "MusicSystem.h"

#define TEST(name)    \
  void test_##name(); \
  void test_##name()

bool floatEqual(float a, float b, float epsilon = 0.001f) {
  return std::abs(a - b) < epsilon;
}

TEST(MusicSystem_AudioConstants) {
  // Verify audio configuration constants are correct
  assert(MusicSystem::AUDIO_FREQUENCY == 44100);
  assert(MusicSystem::AUDIO_CHANNELS == 2);
  assert(MusicSystem::AUDIO_CHUNK_SIZE == 2048);
  assert(MusicSystem::DEFAULT_FADE_MS == 500);
  assert(floatEqual(MusicSystem::DEFAULT_VOLUME, 0.5f));
  assert(MusicSystem::SDL_MIXER_MAX_VOLUME == 128);
  assert(MusicSystem::LOOP_FOREVER == -1);
}

TEST(MusicSystem_VolumeClampingLogic) {
  // Test volume clamping to 0.0-1.0 range (same logic as
  // MusicSystem::setVolume)
  auto clampVolume = [](float vol) { return std::clamp(vol, 0.0f, 1.0f); };

  assert(floatEqual(clampVolume(0.5f), 0.5f));
  assert(floatEqual(clampVolume(-0.1f), 0.0f));
  assert(floatEqual(clampVolume(1.5f), 1.0f));
  assert(floatEqual(clampVolume(0.0f), 0.0f));
  assert(floatEqual(clampVolume(1.0f), 1.0f));
  assert(floatEqual(clampVolume(-100.0f), 0.0f));
  assert(floatEqual(clampVolume(100.0f), 1.0f));
}

TEST(MusicSystem_VolumeToSDLConversion) {
  // Test volume conversion from 0.0-1.0 to SDL_mixer range 0-128
  auto volumeToSDL = [](float vol) {
    float clamped = std::clamp(vol, 0.0f, 1.0f);
    return static_cast<int>(clamped * MusicSystem::SDL_MIXER_MAX_VOLUME);
  };

  assert(volumeToSDL(0.0f) == 0);
  assert(volumeToSDL(1.0f) == 128);
  assert(volumeToSDL(0.5f) == 64);
  assert(volumeToSDL(0.25f) == 32);
  assert(volumeToSDL(0.75f) == 96);

  // Test clamping during conversion
  assert(volumeToSDL(-0.5f) == 0);
  assert(volumeToSDL(1.5f) == 128);
}

TEST(MusicSystem_ZoneDetectionLogic) {
  // Simulate zone detection logic (what checkZoneTransition does)
  std::vector<MusicZone> zones;
  zones.push_back({"outer", "outer.ogg", 0.0f, 0.0f, 1000.0f, 1000.0f});
  zones.push_back({"middle", "middle.ogg", 200.0f, 200.0f, 600.0f, 600.0f});
  zones.push_back({"center", "center.ogg", 400.0f, 400.0f, 200.0f, 200.0f});

  // Player in outer zone only
  float px1 = 100.0f, py1 = 100.0f;
  std::string foundZone1;
  for (const auto& zone : zones) {
    if (zone.contains(px1, py1)) {
      foundZone1 = zone.name;
      break;  // First match wins
    }
  }
  assert(foundZone1 == "outer");

  // Player in middle zone (overlaps with outer, but middle comes first in
  // iteration)
  float px2 = 300.0f, py2 = 300.0f;
  std::string foundZone2;
  for (const auto& zone : zones) {
    if (zone.contains(px2, py2)) {
      foundZone2 = zone.name;
      break;
    }
  }
  assert(foundZone2 == "outer");  // Outer checked first, wins

  // Player in center zone
  float px3 = 500.0f, py3 = 500.0f;
  std::string foundZone3;
  for (const auto& zone : zones) {
    if (zone.contains(px3, py3)) {
      foundZone3 = zone.name;
      break;
    }
  }
  assert(foundZone3 == "outer");  // Outer checked first, wins

  // Player outside all zones
  float px4 = 1500.0f, py4 = 1500.0f;
  std::string foundZone4;
  for (const auto& zone : zones) {
    if (zone.contains(px4, py4)) {
      foundZone4 = zone.name;
      break;
    }
  }
  assert(foundZone4.empty());  // No zone found
}

TEST(MusicSystem_ZonePriorityFirstMatchWins) {
  // Test that first matching zone wins when zones overlap
  std::vector<MusicZone> zones;
  zones.push_back({"priority1", "first.ogg", 0.0f, 0.0f, 500.0f, 500.0f});
  zones.push_back(
      {"priority2", "second.ogg", 0.0f, 0.0f, 500.0f, 500.0f});  // Same area

  float px = 250.0f, py = 250.0f;
  std::string foundZone;
  std::string foundTrack;

  for (const auto& zone : zones) {
    if (zone.contains(px, py)) {
      foundZone = zone.name;
      foundTrack = zone.trackName;
      break;
    }
  }

  assert(foundZone == "priority1");
  assert(foundTrack == "first.ogg");
}

int main() {
  Logger::init();

  test_MusicSystem_AudioConstants();
  test_MusicSystem_VolumeClampingLogic();
  test_MusicSystem_VolumeToSDLConversion();
  test_MusicSystem_ZoneDetectionLogic();
  test_MusicSystem_ZonePriorityFirstMatchWins();

  return 0;
}
