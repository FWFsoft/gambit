# Shader Compatibility Contract

**Feature**: 001-wasm-cross-platform

## Cross-Platform Shader Strategy

All shader source strings must use a platform-dependent version prefix. Game shaders use the same GLSL body — only the version directive and precision qualifiers change.

### Version Prefix

```cpp
#ifdef __EMSCRIPTEN__
static const char* SHADER_VERSION = "#version 300 es\nprecision mediump float;\n";
#else
static const char* SHADER_VERSION = "#version 330 core\n";
#endif
```

### Files Requiring Update

| File | Shader Type | Lines |
|------|-------------|-------|
| src/SpriteRenderer.cpp | Vertex + Fragment | 9, 26 |
| src/TileRenderer.cpp | Vertex + Fragment | 13, 36 |
| src/CollisionDebugRenderer.cpp | Vertex + Fragment | 14, 26 |
| src/MusicZoneDebugRenderer.cpp | Vertex + Fragment | 14, 25 |
| src/ObjectiveDebugRenderer.cpp | Vertex + Fragment | 16, 28 |
| src/Window.cpp | ImGui init string | 109 |

### GL Header Include Guard

```cpp
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
```

### Compatibility Rules

1. All shaders MUST use `in`/`out` syntax (already the case with `#version 330 core`)
2. Fragment shaders on WASM MUST include `precision mediump float;`
3. `texture()` function is compatible across both versions
4. No `gl_FragColor` usage (already compliant with 330 core)
5. GLAD loader call (`gladLoadGLLoader`) MUST be `#ifdef`'d out on Emscripten
