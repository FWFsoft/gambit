#pragma once

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif

#include <functional>
#include <vector>

#include "Camera.h"

// The presentation layer of the art direction ("inked heroes on a dark
// world", docs/design/ART_DIRECTION.md): the world is drawn into an
// offscreen scene target, the floor is lit from a low-resolution light map,
// and the scene is resolved to the screen with bloom and a mild grade. The
// HUD is drawn after the resolve and is never touched.
//
// One frame:
//   beginScene()   RenderSystem, before the tiles: draws the light map,
//                  binds and clears the scene target.
//   ... tiles (TileRenderer reads the light map), then the world layer of
//       ArenaView, which marks what is light with blendGlow()/blendMatte().
//   resolve()      once, after the world and before the HUD.
//
// Only what was drawn as light blooms: the scene's alpha channel is an
// emissive mask (blendGlow writes coverage into it, blendMatte erases it),
// so sprites stay crisp and effects glow.
//
// Every strength is a constant in include/config/LookConfig.h.
class PresentationLayer {
 public:
  // A pool of light on the ground, in world coordinates. `strength` is how
  // many times brighter the floor gets at the centre.
  struct LightPool {
    float x, y, radius;
    float r, g, b;
    float strength;
  };
  using LightProvider = std::function<void(std::vector<LightPool>&)>;

  PresentationLayer();
  ~PresentationLayer();
  PresentationLayer(const PresentationLayer&) = delete;
  PresentationLayer& operator=(const PresentationLayer&) = delete;

  // Off draws the game as it was before the look (for comparisons).
  // Takes effect at the next beginScene.
  void setEnabled(bool on) { wantEnabled = on; }
  void toggle() { wantEnabled = !wantEnabled; }
  bool isEnabled() const { return enabled; }

  // Asked once per frame for the pools to draw.
  void setLightProvider(LightProvider provider) {
    lightProvider = std::move(provider);
  }

  // Returns true when the scene target is bound (the look is on). The
  // caller clears; alpha must be cleared to 0.
  bool beginScene(const Camera& camera);
  bool sceneOpen() const { return open; }
  // The resolve is queued into an ImGui draw list by whoever draws the
  // world; this lets UISystem queue it when nobody did.
  void markResolveQueued() { queued = true; }
  bool resolveQueued() const { return queued; }
  // Bloom, vignette and grade to the default framebuffer. Leaves the
  // default framebuffer bound.
  void resolve();

  // For the floor shader.
  void bindLightMap(int textureUnit) const;
  float targetWidth() const { return static_cast<float>(width); }
  float targetHeight() const { return static_cast<float>(height); }

  // White with a soft radial alpha falloff: contact shadows.
  GLuint softBlobTexture() const { return blobTexture; }

  // Blend states for the scene target. Glow is the normal alpha blend and
  // also writes coverage to the emissive mask; matte erases the mask.
  static void blendGlow();
  static void blendMatte();

 private:
  struct Target {
    GLuint fbo = 0, texture = 0;
    int w = 0, h = 0;
  };

  bool enabled = true;
  bool wantEnabled = true;
  bool open = false;
  bool queued = false;
  int width = 0, height = 0;
  LightProvider lightProvider;
  std::vector<LightPool> pools;
  std::vector<float> poolVertices;

  Target scene, lightMap, bloomNear, bloomNearTmp, bloomWide, bloomWideTmp;
  GLuint blobTexture = 0;
  GLuint fullscreenVAO = 0;
  GLuint poolVAO = 0, poolVBO = 0;
  GLuint poolProgram = 0, brightProgram = 0, blurProgram = 0,
         compositeProgram = 0;
  GLint brightTexel = -1, blurStep = -1;

  void ensureTargets(int w, int h);
  static void createTarget(Target& target, int w, int h);
  static void destroyTarget(Target& target);
  void createBlobTexture();
  void renderLightMap(const Camera& camera);
  void drawFullscreen(const Target& destination) const;
};
