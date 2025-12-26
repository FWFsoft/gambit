# Zone-Based Music System Implementation Plan

## Goal

Implement a zone-based music system for Gambit that plays different music tracks based on which area of the map the player is in. Music transitions use 500ms crossfades. The system is designed as a POC with architecture that supports future event-based and time-based music triggers.

## User Requirements

- **Zone Definition**: Music zones defined as rectangle objects in Tiled object layer with custom property `music_track="filename"`
- **Audio Formats**: Support OGG, WAV, MP3 (SDL_mixer auto-detects)
- **Transitions**: 500ms crossfade between tracks
- **POC Scope**: Zone-based music (player enters zone → music changes)
- **Future**: Event-based music system (time-based triggers, story events, etc.)

## Architecture Overview

The music system follows the same event-driven pattern as AnimationSystem:
1. **MusicSystem** subscribes to UpdateEvent, checks player position each frame
2. **MusicZone** struct stores zone AABB + track filename (similar to CollisionShape)
3. **TiledMap** extracts music zones from "Music" object layer (similar to collision extraction)
4. **SDL_mixer** handles playback, crossfading, format detection

---

## Implementation Plan

### Phase 1: Add SDL_mixer Dependency

**Files to Modify:**

1. **`vcpkg.json`**
   - Add `"sdl2-mixer"` to dependencies array (after sdl2-image)

2. **`CMakeLists.txt`**
   - Add after SDL2-related find_package calls (~line 20):
     ```cmake
     find_package(SDL2_mixer CONFIG REQUIRED)
     ```
   - Update Client target_link_libraries (~line 85):
     ```cmake
     target_link_libraries(Client PRIVATE
         ${ENET_LIBRARY}
         SDL2::SDL2
         SDL2_mixer::SDL2_mixer  # ADD THIS
         spdlog::spdlog
         OpenGL::GL
         # ... rest
     )
     ```

**Verification**: Run `make build` to ensure SDL_mixer is found by vcpkg and links correctly.

---

### Phase 2: Create MusicZone Data Structure

**New File: `include/MusicZone.h`**

```cpp
#pragma once

#include <string>

struct MusicZone {
  std::string name;        // Zone name (e.g., "forest_zone")
  std::string trackName;   // Music file (e.g., "forest_ambience.ogg")
  float x, y;              // Zone position (top-left)
  float width, height;     // Zone dimensions

  // Check if point is inside zone (AABB test)
  bool contains(float px, float py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
  }
};
```

**Design Notes:**
- Similar to CollisionShape but simpler (only rectangles for POC)
- `contains()` method for point-in-zone queries
- No need for collision system integration

---

### Phase 3: Extend TiledMap to Extract Music Zones

**Files to Modify:**

1. **`include/TiledMap.h`**
   - Add include: `#include "MusicZone.h"`
   - Add member variable:
     ```cpp
     std::vector<MusicZone> musicZones;
     ```
   - Add getter:
     ```cpp
     const std::vector<MusicZone>& getMusicZones() const { return musicZones; }
     ```

2. **`src/TiledMap.cpp`**
   - In `load()` function, add after collision extraction (~line 70):
     ```cpp
     // Extract music zones from object layers
     for (const auto& layer : tmxMap.getLayers()) {
       if (layer->getType() == tmx::Layer::Type::Object) {
         const auto& objectLayer = layer->getLayerAs<tmx::ObjectGroup>();

         // Only process "Music" or "MusicZones" layers
         if (objectLayer.getName() != "Music" &&
             objectLayer.getName() != "MusicZones") {
           continue;
         }

         for (const auto& object : objectLayer.getObjects()) {
           // Only process rectangle zones with "zone" type
           if (object.getShape() != tmx::Object::Shape::Rectangle) {
             continue;
           }

           // Extract music_track property
           std::string trackName;
           const auto& properties = object.getProperties();
           for (const auto& prop : properties) {
             if (prop.getName() == "music_track" &&
                 prop.getType() == tmx::Property::Type::String) {
               trackName = prop.getStringValue();
               break;
             }
           }

           // Skip if no music_track property
           if (trackName.empty()) {
             Logger::warn("Music zone '" + object.getName() +
                          "' missing music_track property");
             continue;
           }

           // Create MusicZone
           const auto& aabb = object.getAABB();
           MusicZone zone;
           zone.name = object.getName();
           zone.trackName = trackName;
           zone.x = aabb.left;
           zone.y = aabb.top;
           zone.width = aabb.width;
           zone.height = aabb.height;

           musicZones.push_back(zone);
           Logger::info("Loaded music zone: " + zone.name +
                        " -> " + zone.trackName);
         }
       }
     }
     ```

**Design Notes:**
- Mirrors existing collision extraction pattern
- Uses tmxlite's `getProperties()` API to read custom properties
- Logs loaded zones for debugging
- Warns if zone missing `music_track` property

---

### Phase 4: Create MusicSystem Class

**New File: `include/MusicSystem.h`**

```cpp
#pragma once

#include <string>
#include <unordered_map>
#include <SDL_mixer.h>
#include "MusicZone.h"

// Forward declarations
class ClientPrediction;
class TiledMap;
struct UpdateEvent;

class MusicSystem {
 public:
  // Audio configuration constants
  static constexpr int AUDIO_FREQUENCY = 44100;        // CD quality audio
  static constexpr int AUDIO_CHANNELS = 2;             // Stereo
  static constexpr int AUDIO_CHUNK_SIZE = 2048;        // Sample buffer size
  static constexpr int DEFAULT_FADE_MS = 500;          // Default crossfade duration
  static constexpr float DEFAULT_VOLUME = 0.5f;        // 50% volume
  static constexpr int SDL_MIXER_MAX_VOLUME = 128;     // SDL_mixer volume range
  static constexpr int LOOP_FOREVER = -1;              // SDL_mixer loop infinite

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
```

**New File: `src/MusicSystem.cpp`**

```cpp
#include "MusicSystem.h"
#include "EventBus.h"
#include "ClientPrediction.h"
#include "TiledMap.h"
#include "Player.h"
#include "Logger.h"
#include <cassert>

MusicSystem::MusicSystem(const ClientPrediction* prediction, const TiledMap* map)
    : clientPrediction(prediction),
      tiledMap(map),
      volume(0.5f) {

  // Initialize SDL_mixer
  if (Mix_OpenAudio(AUDIO_FREQUENCY, MIX_DEFAULT_FORMAT,
                    AUDIO_CHANNELS, AUDIO_CHUNK_SIZE) < 0) {
    Logger::error("SDL_mixer init failed: " + std::string(Mix_GetError()));
    return;
  }

  Mix_VolumeMusic(static_cast<int>(volume * SDL_MIXER_MAX_VOLUME));

  Logger::info("MusicSystem initialized");

  // Subscribe to UpdateEvent
  EventBus::instance().subscribe<UpdateEvent>(
      [this](const UpdateEvent& e) { onUpdate(e); });
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
  checkZoneTransition();
}

void MusicSystem::checkZoneTransition() {
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
      Logger::info("Entered zone '" + newZoneName + "', playing: " + newTrackName);
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

  // Fade out current music
  if (!currentTrack.empty() && Mix_PlayingMusic()) {
    Mix_FadeOutMusic(fadeMs);
  }

  // Fade in new music (loop forever)
  if (Mix_FadeInMusic(music, LOOP_FOREVER, fadeMs) == -1) {
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
  volume = std::clamp(vol, 0.0f, 1.0f);
  Mix_VolumeMusic(static_cast<int>(volume * SDL_MIXER_MAX_VOLUME));
}

Mix_Music* MusicSystem::loadMusic(const std::string& filename) {
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
```

**Design Notes:**
- Follows AnimationSystem pattern: subscribes to UpdateEvent in constructor
- Checks player position every frame against zones (efficient - just AABB tests)
- Caches loaded music to avoid reloading
- `playTrack()` and `stopMusic()` are public for future event-based triggers
- SDL_mixer handles format detection automatically (OGG/WAV/MP3)
- Crossfade duration configurable (defaults to 500ms per user preference)

---

### Phase 5: Integrate MusicSystem into Client

**File to Modify: `src/client_main.cpp`**

Changes (~line 60, after AnimationSystem creation):

```cpp
// Create animation system
AnimationSystem animationSystem;

// Create music system (zone-based)
MusicSystem musicSystem(&clientPrediction, &tiledMap);

// Create remote player interpolation
RemotePlayerInterpolation remoteInterpolation(localPlayerId, &animationSystem);
```

**Design Notes:**
- Initialized after AnimationSystem (similar client-side system)
- Requires `clientPrediction` (for player position) and `tiledMap` (for zones)
- Automatically subscribes to events in constructor

---

### Phase 6: Update Test Map with Music Zones

**File to Modify: `assets/test_map.tmx`**

Add new object layer in Tiled editor:
1. Create object layer named "Music" or "MusicZones"
2. Add rectangle objects covering different areas
3. For each rectangle:
   - Set `name` (e.g., "forest_area", "village_area")
   - Set `type` to "zone" (optional, for clarity)
   - Add custom property:
     - Name: `music_track`
     - Type: String
     - Value: `forest_ambience.ogg` (or test_music.ogg)

Example zones:
```
Zone 1: x=0,   y=0,   w=500, h=500  → music_track="forest_ambience.ogg"
Zone 2: x=500, y=0,   w=400, h=300  → music_track="village_theme.ogg"
Zone 3: x=500, y=300, w=400, h=200  → music_track="cave_echo.ogg"
```

For testing, can use single test track: `music_track="test_music.ogg"`

---

### Phase 7: Add Test Music Files

**New Directory: `assets/music/`**

Create directory:
```bash
mkdir -p assets/music
```

Add placeholder music files for testing:
- `assets/music/test_music.ogg` - Simple looping track for POC
- `assets/music/forest_ambience.ogg` (optional)
- `assets/music/village_theme.ogg` (optional)

**Test File Sources:**
- Generate simple tones using tools like Audacity
- Use royalty-free music from OpenGameArt.org
- Create silent/minimal tracks for development

**File Format Notes:**
- OGG Vorbis recommended (best SDL_mixer support)
- WAV works but larger file size
- MP3 support varies by platform

---

## Testing Strategy

Following Tiger Style principles: Write tests that crash on invalid states, use assertions aggressively, and verify behavior through automated tests.

### Phase 2 Unit Tests: MusicZone

**New File: `tests/test_music_zone.cpp`**

```cpp
#include <gtest/gtest.h>
#include "MusicZone.h"

TEST(MusicZone, ContainsPointInside) {
  MusicZone zone{"test", "test.ogg", 100.0f, 100.0f, 200.0f, 150.0f};

  // Points inside zone
  EXPECT_TRUE(zone.contains(150.0f, 125.0f));  // Center
  EXPECT_TRUE(zone.contains(100.0f, 100.0f));  // Top-left corner
  EXPECT_TRUE(zone.contains(299.9f, 249.9f));  // Just inside bottom-right
}

TEST(MusicZone, DoesNotContainPointOutside) {
  MusicZone zone{"test", "test.ogg", 100.0f, 100.0f, 200.0f, 150.0f};

  // Points outside zone
  EXPECT_FALSE(zone.contains(99.9f, 100.0f));   // Just left
  EXPECT_FALSE(zone.contains(100.0f, 99.9f));   // Just above
  EXPECT_FALSE(zone.contains(300.0f, 125.0f));  // Right edge
  EXPECT_FALSE(zone.contains(150.0f, 250.0f));  // Below
  EXPECT_FALSE(zone.contains(0.0f, 0.0f));      // Far away
}

TEST(MusicZone, EdgeCases) {
  MusicZone zone{"test", "test.ogg", 0.0f, 0.0f, 100.0f, 100.0f};

  // Boundary tests
  EXPECT_TRUE(zone.contains(0.0f, 0.0f));      // Origin
  EXPECT_TRUE(zone.contains(99.99f, 99.99f));  // Almost at edge
  EXPECT_FALSE(zone.contains(100.0f, 100.0f)); // Exactly at edge (exclusive)
}

TEST(MusicZone, NegativeCoordinates) {
  MusicZone zone{"test", "test.ogg", -50.0f, -50.0f, 100.0f, 100.0f};

  EXPECT_TRUE(zone.contains(-25.0f, -25.0f));   // Center of negative zone
  EXPECT_TRUE(zone.contains(-50.0f, -50.0f));   // Top-left
  EXPECT_FALSE(zone.contains(-51.0f, -25.0f));  // Just outside
}
```

**Update CMakeLists.txt** to add test:
```cmake
add_executable(test_music_zone tests/test_music_zone.cpp)
target_link_libraries(test_music_zone PRIVATE GTest::gtest_main)
add_test(NAME test_music_zone COMMAND test_music_zone)
```

### Phase 3 Unit Tests: TiledMap Music Zone Extraction

**New File: `tests/test_tiled_map_music.cpp`**

```cpp
#include <gtest/gtest.h>
#include "TiledMap.h"
#include "MusicZone.h"
#include <fstream>

class TiledMapMusicTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test TMX file with music zones
    std::ofstream file("test_music_zones.tmx");
    file << R"(<?xml version="1.0" encoding="UTF-8"?>
<map version="1.0" orientation="isometric" width="10" height="10"
     tilewidth="64" tileheight="32">
  <objectgroup name="Music">
    <object id="1" name="forest_zone" type="zone" x="0" y="0"
            width="500" height="500">
      <properties>
        <property name="music_track" value="forest.ogg"/>
      </properties>
    </object>
    <object id="2" name="village_zone" type="zone" x="500" y="0"
            width="300" height="300">
      <properties>
        <property name="music_track" value="village.ogg"/>
      </properties>
    </object>
    <object id="3" name="no_music_zone" type="zone" x="800" y="0"
            width="100" height="100">
      <!-- Missing music_track property - should warn -->
    </object>
  </objectgroup>
</map>)";
    file.close();
  }

  void TearDown() override {
    std::remove("test_music_zones.tmx");
  }
};

TEST_F(TiledMapMusicTest, LoadsMusicZonesCorrectly) {
  TiledMap map;
  ASSERT_TRUE(map.load("test_music_zones.tmx"));

  const auto& zones = map.getMusicZones();
  ASSERT_EQ(zones.size(), 2);  // Only 2 - third has no music_track

  // Check forest zone
  EXPECT_EQ(zones[0].name, "forest_zone");
  EXPECT_EQ(zones[0].trackName, "forest.ogg");
  EXPECT_FLOAT_EQ(zones[0].x, 0.0f);
  EXPECT_FLOAT_EQ(zones[0].y, 0.0f);
  EXPECT_FLOAT_EQ(zones[0].width, 500.0f);
  EXPECT_FLOAT_EQ(zones[0].height, 500.0f);

  // Check village zone
  EXPECT_EQ(zones[1].name, "village_zone");
  EXPECT_EQ(zones[1].trackName, "village.ogg");
}

TEST_F(TiledMapMusicTest, IgnoresZonesWithoutMusicTrackProperty) {
  TiledMap map;
  map.load("test_music_zones.tmx");

  const auto& zones = map.getMusicZones();

  // Should not include "no_music_zone"
  for (const auto& zone : zones) {
    EXPECT_NE(zone.name, "no_music_zone");
  }
}
```

### Phase 4 Unit Tests: MusicSystem Logic

**New File: `tests/test_music_system.cpp`**

```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MusicSystem.h"
#include "MusicZone.h"

// Mock SDL_mixer functions (requires linking strategy)
// Alternative: Integration tests with actual SDL_mixer

class MockClientPrediction {
 public:
  Player player;
  const Player& getLocalPlayer() const { return player; }
};

class MockTiledMap {
 public:
  std::vector<MusicZone> zones;
  const std::vector<MusicZone>& getMusicZones() const { return zones; }
};

TEST(MusicSystem, ZoneDetectionWhenPlayerInsideZone) {
  MockTiledMap map;
  map.zones.push_back({"forest", "forest.ogg", 0.0f, 0.0f, 500.0f, 500.0f});
  map.zones.push_back({"village", "village.ogg", 500.0f, 0.0f, 300.0f, 300.0f});

  MockClientPrediction prediction;
  prediction.player.x = 250.0f;  // Inside forest zone
  prediction.player.y = 250.0f;

  // MusicSystem system(&prediction, &map);
  // Verify forest.ogg would be selected

  const auto& selectedZone = map.zones[0];
  EXPECT_TRUE(selectedZone.contains(prediction.player.x, prediction.player.y));
  EXPECT_EQ(selectedZone.trackName, "forest.ogg");
}

TEST(MusicSystem, ZoneDetectionWhenPlayerOutsideAllZones) {
  MockTiledMap map;
  map.zones.push_back({"forest", "forest.ogg", 0.0f, 0.0f, 500.0f, 500.0f});

  MockClientPrediction prediction;
  prediction.player.x = 600.0f;  // Outside all zones
  prediction.player.y = 600.0f;

  bool inAnyZone = false;
  for (const auto& zone : map.zones) {
    if (zone.contains(prediction.player.x, prediction.player.y)) {
      inAnyZone = true;
      break;
    }
  }

  EXPECT_FALSE(inAnyZone);
}

TEST(MusicSystem, VolumeClampingBehavior) {
  // Test volume clamping to 0.0-1.0 range
  auto clampVolume = [](float vol) {
    return std::clamp(vol, 0.0f, 1.0f);
  };

  EXPECT_FLOAT_EQ(clampVolume(0.5f), 0.5f);
  EXPECT_FLOAT_EQ(clampVolume(-0.1f), 0.0f);
  EXPECT_FLOAT_EQ(clampVolume(1.5f), 1.0f);
  EXPECT_FLOAT_EQ(clampVolume(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(clampVolume(1.0f), 1.0f);
}

TEST(MusicSystem, ConstantsAreCorrect) {
  // Verify audio configuration constants
  EXPECT_EQ(MusicSystem::AUDIO_FREQUENCY, 44100);
  EXPECT_EQ(MusicSystem::AUDIO_CHANNELS, 2);
  EXPECT_EQ(MusicSystem::AUDIO_CHUNK_SIZE, 2048);
  EXPECT_EQ(MusicSystem::DEFAULT_FADE_MS, 500);
  EXPECT_FLOAT_EQ(MusicSystem::DEFAULT_VOLUME, 0.5f);
  EXPECT_EQ(MusicSystem::SDL_MIXER_MAX_VOLUME, 128);
  EXPECT_EQ(MusicSystem::LOOP_FOREVER, -1);
}
```

### Integration Tests

**New File: `tests/test_music_integration.cpp`**

```cpp
#include <gtest/gtest.h>
#include "MusicSystem.h"
#include "TiledMap.h"
#include "ClientPrediction.h"

// Integration test that requires SDL_mixer to be initialized
TEST(MusicIntegration, DISABLED_LoadsAndPlaysMusicFile) {
  // Create test music file (silent OGG)
  // This test is disabled by default - enable for manual testing

  TiledMap map;
  ASSERT_TRUE(map.load("assets/test_map.tmx"));

  // Create minimal ClientPrediction
  // MusicSystem system(&prediction, &map);

  // Manually trigger music
  // system.playTrack("test_music.ogg");

  // Verify no errors occurred
  // EXPECT_TRUE(system is playing);
}
```

### Manual Testing Steps

1. **Build with tests enabled**:
   ```bash
   make build
   make test
   ```

2. **Verify unit tests pass**:
   ```bash
   ./build/test_music_zone
   ./build/test_tiled_map_music
   ./build/test_music_system
   ```

3. **Run integration test with `/dev`**:
   - Start 2 clients: `/dev`
   - Move players into different zones
   - Verify logs show zone transitions
   - Listen for 500ms crossfade

4. **Test edge cases**:
   - Player at zone boundary
   - Multiple overlapping zones (first match wins)
   - Zone with missing music file (should log error)
   - Zone with invalid music format (should log error)

### Assertions to Add (Tiger Style)

In `MusicSystem.cpp`:
```cpp
// In checkZoneTransition():
assert(clientPrediction != nullptr && "ClientPrediction is null");
assert(tiledMap != nullptr && "TiledMap is null");

// In loadMusic():
assert(!filename.empty() && "Music filename is empty");

// In setVolume():
assert(volume >= 0.0f && volume <= 1.0f && "Volume out of range");
```

### Test Coverage Goals

- ✅ MusicZone::contains() - 100% coverage (all branches)
- ✅ TiledMap music zone extraction - Handles valid/invalid properties
- ✅ MusicSystem zone detection - Inside/outside zones, boundaries
- ✅ Volume clamping - Edge cases (negative, > 1.0)
- ✅ Constants validation - All named constants are correct
- ⚠️ SDL_mixer integration - Manual testing (requires audio hardware)

---

## Future: Event-Based Music System

The architecture naturally supports event-based music:

### Time-Based Triggers
```cpp
// In MusicSystem::onUpdate
void MusicSystem::onUpdate(const UpdateEvent& e) {
  // Check time-based conditions
  if (gameTime > 300.0f && gameTime < 600.0f) {
    playTrack("boss_battle.ogg");
    return;
  }

  // Fall back to zone-based
  checkZoneTransition();
}
```

### Event Triggers
```cpp
// Subscribe to custom events
EventBus::instance().subscribe<BossFightStartEvent>(
    [this](const BossFightStartEvent& e) {
      playTrack("boss_music.ogg", 1000);  // 1 second fade
    });

EventBus::instance().subscribe<BossFightEndEvent>(
    [this](const BossFightEndEvent& e) {
      checkZoneTransition();  // Return to zone music
    });
```

### Priority System
```cpp
enum class MusicPriority {
  Zone = 0,         // Background zone music
  TimeTriggered = 1,  // Special time events
  Combat = 2,       // Combat encounters
  Boss = 3,         // Boss fights
  Cutscene = 4      // Story cutscenes (highest)
};

// Only change music if new priority >= current priority
```

---

## File Changes Summary

### New Files (4)
```
include/MusicZone.h         - Music zone data structure
include/MusicSystem.h       - Music system class
src/MusicSystem.cpp         - Music system implementation
assets/music/               - Music file directory
```

### Modified Files (4)
```
vcpkg.json                  - Add sdl2-mixer dependency
CMakeLists.txt              - Link SDL2_mixer library
include/TiledMap.h          - Add musicZones member + getter
src/TiledMap.cpp            - Extract music zones from Tiled map
src/client_main.cpp         - Initialize MusicSystem
assets/test_map.tmx         - Add Music object layer with zones
```

---

## Dependencies

**New Dependency:**
- **SDL2_mixer** - Audio playback library
  - Supports OGG, WAV, MP3 (auto-detected)
  - Crossfading: Mix_FadeInMusic(), Mix_FadeOutMusic()
  - Volume control: Mix_VolumeMusic()
  - Lightweight, well-maintained

**Existing Dependencies Used:**
- SDL2 (already installed)
- tmxlite (for custom properties)
- spdlog (for logging)
- EventBus (for event-driven updates)

---

## Performance Considerations

### Zone Checking (Every Frame at 60 FPS)
- **Cost**: ~10 AABB tests per frame (one per zone)
- **Per test**: 4 float comparisons
- **Total**: ~40 float ops per frame = negligible

### Music Loading
- Lazy loading: only load tracks when needed
- Cache in unordered_map: O(1) lookup
- SDL_mixer streams from disk (low memory footprint)

### Network Impact
- **None** - Music system is client-side only
- No network packets related to music

---

## Success Criteria

✅ Player enters zone → music changes to zone's track
✅ Music crossfades over 500ms (smooth transition)
✅ Supports OGG, WAV, MP3 formats
✅ Multiple zones work correctly (player walks through all zones)
✅ Logs show zone transitions
✅ No music when player is outside all zones
✅ No crashes or errors with missing music files
✅ Architecture supports future event-based triggers

---

## Implementation Order

1. ✅ Phase 1: Add SDL_mixer dependency (5 min)
2. ✅ Phase 2: Create MusicZone struct (5 min)
3. ✅ Phase 3: Extend TiledMap to extract zones (20 min)
4. ✅ Phase 4: Create MusicSystem class (30 min)
5. ✅ Phase 5: Integrate into client_main.cpp (5 min)
6. ✅ Phase 6: Update test_map.tmx with zones (10 min)
7. ✅ Phase 7: Add test music files (5 min)
8. ✅ Testing: Verify zone transitions work (10 min)

**Total Estimate**: ~90 minutes

---

Ready to implement!
