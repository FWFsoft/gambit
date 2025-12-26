# Music System Decisions

## Overview

Zone-based music system with dynamic transitions using SDL_mixer. Music changes based on player position within rectangular zones defined in Tiled map editor.

## Architecture

### Event-Driven Pattern
- **MusicSystem** subscribes to `UpdateEvent` (60 FPS)
- Checks player position every frame against music zones
- Follows same pattern as AnimationSystem for consistency
- Client-side only - zero network overhead

### Zone Detection
- Uses simple AABB (Axis-Aligned Bounding Box) point-in-zone tests
- First matching zone wins when zones overlap
- Cost: ~10 AABB tests per frame = negligible (~40 float operations)

### Data Flow
```
Tiled Map (TMX)
  → TiledMap::load() extracts music zones from "Music" object layer
  → MusicZone structs stored in TiledMap
  → MusicSystem queries zones each frame
  → Triggers music transitions when zone changes
```

## Technology Choices

### Audio Library: SDL_mixer
**Why SDL_mixer:**
- Already using SDL2 for windowing
- Lightweight, well-maintained
- Auto-detects audio formats (OGG, WAV, MP3)
- Built-in fade effects via `Mix_FadeInMusic()`

**Limitations:**
- Only one music track plays at a time (no true crossfade)
- Cannot overlap two music streams
- Fade is "fade to silence, then fade in" not "crossfade"

**Alternatives Considered:**
- `Mix_Chunk` for sound effects - loads entire file into memory (too heavy for music)
- Custom multi-stream solution - over-engineered for POC
- Different audio library - unnecessary complexity

### Audio Format: OGG Vorbis (Recommended)
- Best SDL_mixer support
- Good compression
- Open format
- WAV and MP3 also supported

## Implementation Details

### Named Constants (No Magic Numbers)
All configuration exposed as `static constexpr` in MusicSystem.h:
```cpp
static constexpr int AUDIO_FREQUENCY = 44100;     // CD quality
static constexpr int AUDIO_CHANNELS = 2;          // Stereo
static constexpr int AUDIO_CHUNK_SIZE = 2048;     // Sample buffer
static constexpr int DEFAULT_FADE_MS = 500;       // Fade duration
static constexpr float DEFAULT_VOLUME = 0.5f;     // 50% volume
static constexpr int SDL_MIXER_MAX_VOLUME = 128;  // SDL_mixer range
static constexpr int LOOP_FOREVER = -1;           // Infinite loop
```

### Fade Behavior
**Original Plan:** 500ms crossfade
**Actual Implementation:** 250ms fade-in (50% of DEFAULT_FADE_MS)

**Why reduced:**
- SDL_mixer limitation: cannot overlap music tracks
- Calling `Mix_FadeOutMusic()` + `Mix_FadeInMusic()` caused lag/conflicts
- `Mix_FadeInMusic()` automatically stops current track
- Shorter fade (250ms) feels smoother for zone transitions

**Code:**
```cpp
int actualFadeMs = fadeMs / 2;  // Halve for quicker transitions
Mix_FadeInMusic(music, LOOP_FOREVER, actualFadeMs);
```

### Music Loading Strategy
**Lazy Loading with Caching:**
- Music files loaded on-demand when zone entered
- Cached in `std::unordered_map<std::string, Mix_Music*>`
- O(1) lookup for subsequent plays
- Memory efficient - only loads tracks that are used

**File Path Convention:**
- All music files in `assets/music/` directory
- Zone specifies filename only (e.g., "ambient_outer.ogg")
- System prepends "assets/music/" automatically

## Tiled Map Integration

### Zone Definition
**Object Layer Name:** "Music" or "MusicZones"

**Zone Properties:**
- Shape: Rectangle only (for POC)
- Custom property: `music_track` (String) - filename in assets/music/
- Name: Zone identifier for debugging

**Example TMX:**
```xml
<objectgroup name="Music">
  <object name="forest_zone" type="zone" x="0" y="0" width="500" height="500">
    <properties>
      <property name="music_track" value="forest_ambience.ogg"/>
    </properties>
  </object>
</objectgroup>
```

### Collision System Integration
**Critical Fix:** Music zones must NOT be treated as collision shapes

**Solution in TiledMap::extractCollisionShapes():**
```cpp
// Skip Music/MusicZones layers (they're for music zones, not collision)
if (objectLayer.getName() == "Music" ||
    objectLayer.getName() == "MusicZones") {
  continue;
}
```

**Bug History:** Initially forgot this check, resulting in entire map being collidable.

## Debug Visualization

### F1 Toggle (Combined Debug)
- Shows both collision shapes AND music zones
- User simplified F2 → F1 to show everything at once

### MusicZoneDebugRenderer
- Semi-transparent colored overlays (80 alpha)
- Different color per zone (purple, cyan, orange, green, pink, yellow-green)
- Uses OpenGL `GL_TRIANGLE_FAN` for filled rectangles
- Renders UNDER collision debug (so collision outlines are visible on top)

**Rendering Order:**
1. Tiles + Players
2. Music zones (if enabled)
3. Collision shapes (if enabled)

## SDL Integration

### Audio Subsystem Initialization
**Critical Fix:** Window.cpp must initialize SDL audio

**Before (broken):**
```cpp
SDL_Init(SDL_INIT_VIDEO);
```

**After (working):**
```cpp
SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
```

**Impact:** Without `SDL_INIT_AUDIO`, SDL_mixer fails silently or causes blocking.

## Future Extensions

### Event-Based Music (Architecture Ready)
The current implementation supports future event-based triggers:

**Time-Based:**
```cpp
if (gameTime > 300.0f && gameTime < 600.0f) {
  playTrack("boss_battle.ogg");
  return;
}
checkZoneTransition();  // Fall back to zones
```

**Event-Based:**
```cpp
EventBus::instance().subscribe<BossFightStartEvent>(
  [this](const BossFightStartEvent& e) {
    playTrack("boss_music.ogg", 1000);  // 1 second fade
  }
);
```

**Priority System:**
```cpp
enum class MusicPriority {
  Zone = 0,      // Background
  Combat = 2,    // Combat encounters
  Boss = 3,      // Boss fights
  Cutscene = 4   // Highest priority
};
```

### Multi-Track Crossfade (If Needed Later)
Current SDL_mixer limitation could be overcome by:
1. Using `Mix_Chunk` + manual streaming
2. Switching to OpenAL or FMOD
3. Custom mixer with multiple SDL_RWops streams

**Decision:** Not worth the complexity for POC. Simple fade-in works well.

## Testing

### Unit Tests
- `test_music_zone.cpp` - MusicZone::contains() edge cases
- `test_music_system.cpp` - Constants, volume clamping, zone detection logic

### Manual Testing
- 4 quadrant map for easy zone transitions
- F1 debug overlay to visualize zones
- Different tone frequencies per zone (220Hz, 440Hz, 880Hz) for clear audio feedback

## Performance

### CPU Cost
- Zone checking: ~10 AABB tests per frame @ 60 FPS
- Per test: 4 float comparisons
- Total: ~40 float ops/frame = negligible

### Memory
- Lazy loading: only loaded tracks consume memory
- Each OGG file: ~10-50KB compressed, SDL_mixer streams from disk
- Cache overhead: `std::unordered_map` with pointers

### Network
- Zero network traffic - client-side only
- Each client detects zones independently
- No music zone synchronization needed

## Known Issues & Limitations

1. **No True Crossfade:** SDL_mixer limitation - tracks don't overlap
2. **Rectangle Zones Only:** Polygons not implemented (future enhancement)
3. **Client Prediction Required:** Depends on ClientPrediction for player position
4. **Single Music Track:** Cannot layer background + ambient + stingers

## Success Criteria (All Met)

✅ Player enters zone → music changes to zone's track
✅ Music fades smoothly (250ms, no lag)
✅ Supports OGG, WAV, MP3 formats
✅ Multiple zones work correctly
✅ Logs show zone transitions
✅ No music when outside all zones
✅ No crashes with missing music files
✅ Architecture supports future event-based triggers
✅ F1 debug visualization shows zones clearly
✅ Zero network overhead

## Files Modified/Created

### New Files (6)
- `include/MusicZone.h` - Zone data structure
- `include/MusicSystem.h` - Music system class
- `src/MusicSystem.cpp` - Implementation
- `include/MusicZoneDebugRenderer.h` - Debug visualization
- `src/MusicZoneDebugRenderer.cpp` - Debug rendering
- `assets/music/` - Music file directory (3 test OGG files)

### Modified Files (9)
- `vcpkg.json` - Added sdl2-mixer dependency
- `CMakeLists.txt` - Link SDL2_mixer, add MusicSystem sources
- `include/TiledMap.h` - Add musicZones member + getter
- `src/TiledMap.cpp` - Extract music zones, skip in collision extraction
- `src/client_main.cpp` - Initialize MusicSystem + MusicZoneDebugRenderer
- `include/InputSystem.h` - Add musicZoneDebugRenderer parameter
- `src/InputSystem.cpp` - F1 toggles both debug renderers
- `include/RenderSystem.h` - Add musicZoneDebugRenderer parameter
- `src/RenderSystem.cpp` - Render music zone overlay
- `src/Window.cpp` - Add SDL_INIT_AUDIO flag
- `assets/test_map.tmx` - Add Music object layer with 4 quadrant zones

## Lessons Learned

1. **Always check what object layers are processed where** - Music zones were initially loaded as collision shapes
2. **SDL subsystem initialization matters** - Missing SDL_INIT_AUDIO caused silent failures
3. **SDL_mixer has limitations** - True crossfade not possible without custom solution
4. **Shorter fades feel better** - 250ms > 500ms for zone transitions
5. **Named constants > magic numbers** - Makes code self-documenting
6. **Tiger Style assertions** - Caught null pointer bugs during development

## References

- SDL_mixer Documentation: https://www.libsdl.org/projects/SDL_mixer/docs/SDL_mixer.html
- Tiled Map Editor: https://www.mapeditor.org/
- Plan: `old_plans/music-system-implementation.md`
