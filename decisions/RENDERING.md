# Rendering System Decisions

**Date**: 2024-12-24
**Status**: Approved
**Context**: Upgrading from SDL2 2D renderer to OpenGL for UI and future graphics capabilities

---

## Decision Summary

We are upgrading Gambit's rendering from **SDL2 2D renderer** to **OpenGL 3.3 Core** with **Dear ImGui** for UI, enabling:
1. Immediate UI development (free)
2. Future Rive integration for polish (optional, $15/mo)
3. Advanced graphics capabilities (shaders, effects)
4. Cross-platform compatibility

---

## Background: Why Upgrade from SDL2 2D?

### Current Limitations
- **No UI framework**: Can't build menus, HUD, inventory, skill trees
- **No textures**: Only solid color rectangles
- **No advanced graphics**: No shaders, effects, or modern rendering
- **Can't integrate Rive**: Rive requires OpenGL/Vulkan

### What We Need
- Professional UI system for menus and game interfaces
- Texture loading for sprites and tiles
- Path to polished animations (Rive) when budget allows
- Maintainability and future-proofing

---

## UI Framework Evaluation

We evaluated 5 options for UI:

| Framework | Cost | Visual Editor | Best For | Decision |
|-----------|------|---------------|----------|----------|
| **Rive** | $15-100/mo | ✅ Professional | Polished games | Later upgrade |
| **Dear ImGui** | Free | ❌ Code-driven | Prototyping, dev tools | **Chosen** |
| **RmlUI** | Free | ❌ HTML/CSS | Web-familiar devs | Not chosen |
| **Custom** | Free (time) | ❌ DIY | Learning projects | Not chosen |
| **Nuklear** | Free | ❌ Minimal | Tiny dependencies | Not chosen |

### Why Dear ImGui (Now)

**Pros**:
- ✅ **Free & open source** (MIT license, commercial-friendly)
- ✅ **Fast integration**: ~2 days to working UI
- ✅ **Powerful**: Can build complex UIs programmatically
- ✅ **Perfect for prototyping**: Validate UI requirements before investing in Rive
- ✅ **OpenGL 3.3 backend included**: Works with our chosen graphics API
- ✅ **Great for dev tools**: Debug overlays, editors, profilers will stay ImGui

**Cons**:
- ❌ Looks like dev tools, not game UI (acceptable for now)
- ❌ No visual editor for designers (code-driven)
- ❌ Limited animation capabilities (fine for menus)

**Decision**: Start with ImGui to learn UI requirements, upgrade to Rive later if polish justifies $15/mo.

### Why Rive (Future Upgrade)

**When to Consider**:
- UI work becomes 30%+ of development time
- Need marketing materials (polished menu screenshots)
- Player feedback: "UI looks placeholder"
- Working with a dedicated UI/UX designer

**Advantages**:
- Professional animation timeline editor
- State machines for UI logic
- Designer can work independently
- Small file sizes (vector-based)
- Already have OpenGL ready (easy integration)

**Cost Analysis**:
- $15/month = $180/year
- Pays for itself if saves >12 hours/year vs coding in ImGui
- ~20 hours integration (OpenGL already done)

**Path**: OpenGL + ImGui now → Evaluate Rive after 3 months → Subscribe if polish worth it

---

## OpenGL Version Choice

### Comparison

| Version | Pros | Cons | Verdict |
|---------|------|------|---------|
| **OpenGL 3.3 Core** | Modern, Rive compatible, shaders | More complex | ✅ **Chosen** |
| OpenGL 2.1 Legacy | Simpler, fixed-function | Deprecated, no Rive | ❌ Not chosen |
| Vulkan | Cutting-edge performance | Extreme complexity | ❌ Overkill |
| DirectX | Windows-native | Platform-locked | ❌ Not cross-platform |
| Metal | macOS/iOS native | Platform-locked | ❌ Not cross-platform |

### Decision: OpenGL 3.3 Core Profile

**Rationale**:
1. **Cross-platform**: Windows, macOS, Linux (important for indie game)
2. **Mature ecosystem**: Libraries, tutorials, community support
3. **Rive compatible**: Rive requires modern OpenGL (3.0+)
4. **Future-proof**: Not deprecated, widely supported
5. **Shader-based**: Enables advanced effects (post-processing, particles)

**Trade-offs**:
- More complex than fixed-function OpenGL 2.1
- Requires shader knowledge (acceptable learning curve)
- Core profile = no legacy functions (cleaner code)

**Result**: Best balance of modern features, compatibility, and future readiness.

---

## Texture Loading Decision

### Options Evaluated

| Approach | Integration | Formats | Dependencies | Decision |
|----------|-------------|---------|--------------|----------|
| **SDL2_image** | Simple | PNG, JPG, BMP, TGA | SDL2 lib | ✅ **Chosen** |
| stb_image.h | Single-header | PNG, JPG, TGA, BMP | None | ❌ Not chosen |
| libPNG + libJPEG | Manual | PNG, JPG | 2 libraries | ❌ Too complex |

### Decision: SDL2_image

**Rationale**:
1. Already using SDL2 for windowing
2. Familiar API, less build complexity
3. Handles all common formats (PNG, JPG, BMP)
4. Available in vcpkg: `sdl2-image`

**Trade-off**: Slightly larger dependency than stb_image, but worth it for simplicity.

---

## Rendering Architecture Decisions

### 1. Rendering Pipeline

**Approach**: Single-pass rendering with UI on top

```
1. Clear screen (glClear)
2. Render game world (tiles, sprites, players)
3. Render debug overlays (if enabled)
4. Render UI (ImGui) - always on top
5. Swap buffers (SDL_GL_SwapWindow)
```

**Rationale**:
- Simple to implement and debug
- UI naturally sits on top of game
- No complex render-to-texture needed
- Matches event-driven architecture (RenderEvent)

**Alternative Considered**: Separate render passes (game to texture, then composite)
- **Rejected**: Overkill for 2D isometric game, adds complexity

### 2. Isometric Projection with OpenGL

**Approach**: Orthographic projection matrix

**Implementation**:
```cpp
// Convert isometric world coordinates to screen via matrix
glm::mat4 projection = glm::ortho(0.0f, screenWidth, screenHeight, 0.0f);
glm::mat4 view = glm::translate(identity, cameraOffset);
glm::mat4 isoTransform = ... // Isometric rotation matrix
```

**Rationale**:
- More GPU-efficient than CPU-side worldToScreen()
- Aligns with modern OpenGL practices
- Easier to add camera effects (zoom, shake)

**Migration**: Keep existing `Camera::worldToScreen()` for collision/input until OpenGL stable

### 3. Shader Strategy

**Approach**: Minimal shaders for 2D

**Shaders**:
1. **Sprite shader**: Textured quad with color tint
2. **Debug line shader**: Colored wireframes (collision)
3. **ImGui shader**: Provided by ImGui backend

**Rationale**:
- Simple shaders sufficient for 2D isometric
- Can add effects later (bloom, particles, etc.)
- Start minimal, add complexity as needed

---

## UI State Management

### Screen Stack Approach

**Design**:
```cpp
enum class UIState {
    MainMenu,
    InGame,
    PauseMenu,
    Settings,
    // Future: Inventory, SkillTree, CharacterSheet
};

class UISystem {
    std::stack<UIState> screenStack;  // Push pause over game, etc.
};
```

**Rationale**:
- Natural navigation (pause pushes over game, resume pops)
- Matches game feel (ESC = back one screen)
- Extensible for complex nested UIs (inventory → character → skill tree)

**Input Routing**:
- ImGui captures mouse/keyboard when over UI (`ImGui::GetIO().WantCaptureMouse`)
- Game input only processes when UI doesn't want it
- Prevents clicking through menus to move player

---

## Implementation Timeline

### Phase 1: OpenGL Foundation (~1 week)
- OpenGL 3.3 context creation
- GLAD loader integration
- Basic shader setup
- Empty window rendering

**Success Metric**: Window renders with 60 FPS, VSync working

### Phase 2: Texture System (~3-4 days)
- SDL2_image integration
- Texture class (load, bind, cache)
- TextureManager (asset caching)
- SpriteRenderer (draw textured quads)

**Success Metric**: Load test PNG, render as sprite

### Phase 3: Migration to OpenGL (~3-5 days)
- TileRenderer → OpenGL sprites
- Player rendering → OpenGL sprites
- CollisionDebugRenderer → OpenGL lines
- Camera → matrix-based projection

**Success Metric**: Game looks identical to before, all tests pass

### Phase 4: Dear ImGui (~2-3 days)
- ImGui integration (OpenGL3 + SDL2 backends)
- UISystem class
- First screens (main menu, pause, debug)
- Input routing

**Success Metric**: Clickable main menu, toggleable debug overlay

### Phase 5: UI State (~2 days)
- Screen stack implementation
- Event integration (ESC = pause, Play = start game)
- Menu navigation

**Success Metric**: Navigate menu → game → pause → menu

**Total Estimate**: ~2-3 weeks for full OpenGL + ImGui integration

---

## Future Considerations

### Rive Integration (Optional)

**When**: After 3 months of ImGui usage

**Evaluation Criteria**:
1. Is UI work >30% of development time?
2. Is $180/year justified by time savings?
3. Do we need polished animations for marketing?
4. Are we working with a UI designer?

**If Yes** → Subscribe to Rive, integrate `.riv` files
**If No** → Keep ImGui, style it better

**Integration Effort**: ~20 hours (OpenGL already ready, just add Rive renderer)

### Advanced Graphics

Future additions enabled by OpenGL 3.3:
- **Post-processing**: Bloom, color grading, screen shake
- **Particle systems**: Spell effects, explosions, weather
- **Lighting**: Dynamic shadows, fog of war
- **Batching**: Render hundreds of sprites efficiently

These can be added incrementally without major refactors.

---

## Dependencies Summary

### Added to `vcpkg.json`
```json
{
  "dependencies": [
    "sdl2",           // (existing) Windowing
    "enet",           // (existing) Networking
    "spdlog",         // (existing) Logging
    "tmxlite",        // (existing) Tiled maps
    "glad",           // NEW: OpenGL loader
    "glm",            // NEW: Math library (matrices, vectors)
    "sdl2-image",     // NEW: Texture loading (PNG, JPG)
    "imgui[opengl3-binding,sdl2-binding]"  // NEW: UI framework
  ]
}
```

### Why These Libraries?

- **GLAD**: Modern OpenGL loader (alternative: GLEW)
  - Loads OpenGL functions at runtime
  - Header-only after generation
  - More modern than GLEW

- **GLM**: OpenGL Mathematics
  - Header-only C++ math library
  - Designed for OpenGL (mat4, vec3, etc.)
  - Matches GLSL shader syntax

- **SDL2_image**: Image loading
  - Extends SDL2 with PNG/JPG support
  - Simple API, familiar to SDL2 users
  - Cross-platform, well-maintained

- **ImGui**: Immediate-mode GUI
  - Proven in game dev (used by AAA studios for tools)
  - MIT license (commercial-friendly)
  - Excellent documentation and examples

---

## Risk Mitigation

### Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| OpenGL migration breaks game | High | Incremental phases, test after each |
| Performance regression | Medium | Profile before/after, maintain 60 FPS |
| ImGui looks ugly | Low | Style later, functionality first |
| Texture memory issues | Low | Implement TextureManager caching |
| Input routing bugs (UI vs game) | Medium | Test with ImGui::WantCaptureMouse |

### Rollback Plan

If OpenGL upgrade fails catastrophically:
1. **Git branch**: All work done in `feature/opengl-imgui` branch
2. **Fallback**: Can revert to SDL2 2D renderer (main branch)
3. **Incremental testing**: Each phase verified before next

---

## Testing Strategy

### Unit Tests
- Texture loading (valid PNG, invalid file, missing file)
- TextureManager caching (no duplicate loads)
- Camera matrix transformations (world ↔ screen conversions)

### Integration Tests
- All existing tests must pass after migration
- Collision tests still work with OpenGL rendering
- Network prediction/reconciliation unaffected

### Visual Tests
- Game looks identical before/after OpenGL migration
- UI renders on top, doesn't flicker
- Debug overlay toggles cleanly
- FPS stays at 60 (VSync working)

### Manual Tests
- Navigate all menu screens (main → settings → back)
- Pause during gameplay (ESC), resume, quit
- Click UI buttons (no input bleeding to game)
- Toggle collision debug (F1) works with OpenGL

---

## Alternative Approaches Considered (and Rejected)

### 1. Stick with SDL2 2D Renderer
**Reason Rejected**:
- No path to Rive (requires OpenGL)
- Can't build UI without external library
- Limited to basic 2D graphics

### 2. Use Rive Immediately
**Reason Rejected**:
- $15/mo cost without proving UI needs first
- Don't know UI requirements yet (menus simple? complex?)
- ImGui teaches requirements, then Rive easy to add

### 3. Custom UI Framework
**Reason Rejected**:
- 100-200 hours of work (button system, layout, input)
- Reinventing the wheel
- Won't be as polished as ImGui

### 4. Web-based UI (Electron, CEF)
**Reason Rejected**:
- Massive dependencies (Chromium = 100+ MB)
- Overkill for game UI
- Performance overhead

### 5. Unreal/Unity Built-in UI
**Reason Rejected**:
- Not using those engines (custom C++ engine)
- Want full control over architecture

---

## Documentation & Resources

### Official Docs
- **OpenGL**: https://www.opengl.org/sdk/docs/
- **GLAD**: https://github.com/Dav1dde/glad
- **GLM**: https://github.com/g-truc/glm
- **Dear ImGui**: https://github.com/ocornut/imgui
- **SDL2_image**: https://www.libsdl.org/projects/SDL_image/
- **Rive**: https://rive.app/docs

### Tutorials Referenced
- LearnOpenGL.com: Modern OpenGL (3.3+) tutorial
- Lazy Foo' Productions: SDL2 + OpenGL setup
- ImGui examples: imgui/examples/example_sdl_opengl3

### Design Decisions Influenced By
- **Tiger Style**: Assert early, fail fast (validate OpenGL state)
- **DESIGN.md**: High-code philosophy, event-based architecture
- **Indie constraints**: Free tools prioritized, paid tools when justified

---

## Approval & Sign-off

**Approved by**: User (2024-12-24)
**Implemented by**: Claude (AI pair programmer)

**Key Decisions**:
1. ✅ OpenGL 3.3 Core (modern, Rive-compatible)
2. ✅ Dear ImGui for UI (free, fast iteration)
3. ✅ SDL2_image for textures (familiar, simple)
4. ✅ Phased approach (test incrementally)
5. ✅ Rive upgrade path (optional, later)

**Next Steps**:
1. Implement Phase 1 (OpenGL Foundation)
2. Test: Empty window renders at 60 FPS
3. Document learnings in this file as we go

---

## Change Log

### 2024-12-24
- **Initial version**: Documented UI framework evaluation and OpenGL decision
- **Approved**: OpenGL 3.3 Core + Dear ImGui approach
- **Status**: Ready for implementation (Phase 1)

### Future Updates
- Add post-implementation notes (gotchas, performance findings)
- Update if Rive integration happens
- Document any deviations from plan
