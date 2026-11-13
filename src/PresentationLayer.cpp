#include "PresentationLayer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

#include "Logger.h"
#include "OpenGLUtils.h"
#include "ShaderCompat.h"
#include "config/LookConfig.h"

namespace Look = Config::Look;

namespace {

// One triangle that covers the screen; no vertex buffer needed.
const char* fullscreenVertexShader =
    GAMBIT_GLSL_VERSION
    R"(
out vec2 vUv;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

// Light pools: additive radial falloff, (1 - d)^2 as in the style frame.
const char* poolVertexShader =
    GAMBIT_GLSL_VERSION
    R"(
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUv;
layout (location = 2) in vec3 aColor;
out vec2 vUv;
out vec3 vColor;
void main() {
    vUv = aUv;
    vColor = aColor;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* poolFragmentShader =
    GAMBIT_GLSL_VERSION
    GAMBIT_GLSL_PRECISION_HIGH
    R"(
in vec2 vUv;
in vec3 vColor;
out vec4 FragColor;
void main() {
    float f = clamp(1.0 - length(vUv), 0.0, 1.0);
    FragColor = vec4(vColor * f * f, 1.0);
}
)";

// Bloom source: what was drawn as light (scene alpha is the emissive mask),
// box-filtered down, with a soft threshold.
const char* brightFragmentShader =
    GAMBIT_GLSL_VERSION
    GAMBIT_GLSL_PRECISION_HIGH
    R"(
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D scene;
uniform vec2 texel;
uniform vec2 threshold;  // x = threshold, y = knee
vec3 tap(vec2 offset) {
    vec4 s = texture(scene, vUv + offset * texel);
    // Coverage squared: outlines and cores glow, big faint fills do not.
    return s.rgb * s.a * s.a;
}
void main() {
    vec3 e = (tap(vec2(-1.0, -1.0)) + tap(vec2(1.0, -1.0)) +
              tap(vec2(-1.0, 1.0)) + tap(vec2(1.0, 1.0))) * 0.25;
    float peak = max(e.r, max(e.g, e.b));
    e *= smoothstep(threshold.x, threshold.x + threshold.y, peak);
    FragColor = vec4(e, 1.0);
}
)";

// Separable 9-tap gaussian in 5 bilinear samples.
const char* blurFragmentShader =
    GAMBIT_GLSL_VERSION
    GAMBIT_GLSL_PRECISION_HIGH
    R"(
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 stride;
void main() {
    vec3 c = texture(image, vUv).rgb * 0.2270270;
    c += (texture(image, vUv + stride * 1.3846154).rgb +
          texture(image, vUv - stride * 1.3846154).rgb) * 0.3162162;
    c += (texture(image, vUv + stride * 3.2307692).rgb +
          texture(image, vUv - stride * 3.2307692).rgb) * 0.0702703;
    FragColor = vec4(c, 1.0);
}
)";

// Scene + bloom, vignette, grade (styleframe.py: grade()).
const char* compositeFragmentShader =
    GAMBIT_GLSL_VERSION
    GAMBIT_GLSL_PRECISION_HIGH
    R"(
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D scene;
uniform sampler2D bloomNear;
uniform sampler2D bloomWide;
uniform vec2 bloomGain;  // near, wide
uniform vec4 grade;      // shoulder, gain, gamma, saturation
uniform vec3 vignette;   // amount, start, power
void main() {
    vec3 c = texture(scene, vUv).rgb;
    c += texture(bloomNear, vUv).rgb * bloomGain.x;
    c += texture(bloomWide, vUv).rgb * bloomGain.y;
    float edge = length((vUv - 0.5) * 2.0);
    c *= 1.0 - vignette.x * pow(clamp(edge - vignette.y, 0.0, 1.0), vignette.z);
    c = c / (1.0 + c * grade.x) * grade.y;
    c = pow(clamp(c, 0.0, 1.0), vec3(grade.z));
    float lum = (c.r + c.g + c.b) / 3.0;
    c = vec3(lum) + (c - vec3(lum)) * grade.w;
    FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
)";

constexpr int POOL_VERTEX_FLOATS = 7;  // ndc xy, uv, rgb
constexpr int POOL_VERTICES = 6;
constexpr int BLOB_SIZE = 64;

void setStandardBlend() {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

}  // namespace

PresentationLayer::PresentationLayer() {
  static_assert(Look::LIGHT_MAP_DIVISOR >= 1, "light map divisor");
  static_assert(Look::BLOOM_WIDE_DIVISOR >= Look::BLOOM_NEAR_DIVISOR,
                "the wide bloom is the smaller target");
  static_assert(Look::LIGHT_RANGE > 0.0f, "light range");
  static_assert(Look::KEY_LIGHT_Z > 0.0f, "the key light is above the ground");

  poolProgram =
      OpenGLUtils::createShaderProgram(poolVertexShader, poolFragmentShader);
  brightProgram = OpenGLUtils::createShaderProgram(fullscreenVertexShader,
                                                   brightFragmentShader);
  blurProgram = OpenGLUtils::createShaderProgram(fullscreenVertexShader,
                                                 blurFragmentShader);
  compositeProgram = OpenGLUtils::createShaderProgram(fullscreenVertexShader,
                                                      compositeFragmentShader);

  // The look's strengths are compile-time constants: set them once.
  glUseProgram(brightProgram);
  glUniform1i(glGetUniformLocation(brightProgram, "scene"), 0);
  glUniform2f(glGetUniformLocation(brightProgram, "threshold"),
              Look::BLOOM_THRESHOLD, Look::BLOOM_KNEE);
  brightTexel = glGetUniformLocation(brightProgram, "texel");

  glUseProgram(blurProgram);
  glUniform1i(glGetUniformLocation(blurProgram, "image"), 0);
  blurStep = glGetUniformLocation(blurProgram, "stride");

  glUseProgram(compositeProgram);
  glUniform1i(glGetUniformLocation(compositeProgram, "scene"), 0);
  glUniform1i(glGetUniformLocation(compositeProgram, "bloomNear"), 1);
  glUniform1i(glGetUniformLocation(compositeProgram, "bloomWide"), 2);
  glUniform2f(glGetUniformLocation(compositeProgram, "bloomGain"),
              Look::BLOOM_GAIN_NEAR, Look::BLOOM_GAIN_WIDE);
  glUniform4f(glGetUniformLocation(compositeProgram, "grade"),
              Look::GRADE_SHOULDER, Look::GRADE_GAIN, Look::GRADE_GAMMA,
              Look::GRADE_SATURATION);
  glUniform3f(glGetUniformLocation(compositeProgram, "vignette"),
              Look::SCENE_VIGNETTE, Look::FLOOR_VIGNETTE_START,
              Look::FLOOR_VIGNETTE_POWER);
  glUseProgram(0);

  glGenVertexArrays(1, &fullscreenVAO);

  glGenVertexArrays(1, &poolVAO);
  glGenBuffers(1, &poolVBO);
  glBindVertexArray(poolVAO);
  glBindBuffer(GL_ARRAY_BUFFER, poolVBO);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(float) * POOL_VERTEX_FLOATS * POOL_VERTICES *
                   Look::MAX_LIGHT_POOLS,
               nullptr, GL_DYNAMIC_DRAW);
  const GLsizei stride = POOL_VERTEX_FLOATS * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        (void*)(2 * sizeof(float)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                        (void*)(4 * sizeof(float)));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  pools.reserve(Look::MAX_LIGHT_POOLS);
  poolVertices.reserve(POOL_VERTEX_FLOATS * POOL_VERTICES *
                       Look::MAX_LIGHT_POOLS);

  createBlobTexture();

  // Targets match the framebuffer the window was created with; beginScene
  // rebuilds them if that ever changes.
  GLint viewport[4] = {0, 0, 0, 0};
  glGetIntegerv(GL_VIEWPORT, viewport);
  ensureTargets(viewport[2], viewport[3]);

  OpenGLUtils::checkGLError("PresentationLayer::PresentationLayer");
  Logger::info("PresentationLayer initialized (" + std::to_string(width) +
               "x" + std::to_string(height) + ")");
}

PresentationLayer::~PresentationLayer() {
  destroyTarget(scene);
  destroyTarget(lightMap);
  destroyTarget(bloomNear);
  destroyTarget(bloomNearTmp);
  destroyTarget(bloomWide);
  destroyTarget(bloomWideTmp);
  glDeleteTextures(1, &blobTexture);
  glDeleteVertexArrays(1, &fullscreenVAO);
  glDeleteVertexArrays(1, &poolVAO);
  glDeleteBuffers(1, &poolVBO);
  glDeleteProgram(poolProgram);
  glDeleteProgram(brightProgram);
  glDeleteProgram(blurProgram);
  glDeleteProgram(compositeProgram);
}

void PresentationLayer::createTarget(Target& target, int w, int h) {
  assert(w > 0 && h > 0);
  assert(target.fbo == 0 && target.texture == 0);
  target.w = w;
  target.h = h;
  glGenTextures(1, &target.texture);
  glBindTexture(GL_TEXTURE_2D, target.texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenFramebuffers(1, &target.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         target.texture, 0);
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  assert(status == GL_FRAMEBUFFER_COMPLETE &&
         "presentation target is not renderable");
  (void)status;
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PresentationLayer::destroyTarget(Target& target) {
  if (target.fbo != 0) glDeleteFramebuffers(1, &target.fbo);
  if (target.texture != 0) glDeleteTextures(1, &target.texture);
  target = Target{};
}

void PresentationLayer::ensureTargets(int w, int h) {
  assert(w > 0 && h > 0 && "presentation layer needs a real framebuffer size");
  if (w == width && h == height) return;
  assert(!open && "cannot resize while a scene is open");
  width = w;
  height = h;
  destroyTarget(scene);
  destroyTarget(lightMap);
  destroyTarget(bloomNear);
  destroyTarget(bloomNearTmp);
  destroyTarget(bloomWide);
  destroyTarget(bloomWideTmp);
  auto part = [](int size, int divisor) { return std::max(1, size / divisor); };
  createTarget(scene, w, h);
  createTarget(lightMap, part(w, Look::LIGHT_MAP_DIVISOR),
               part(h, Look::LIGHT_MAP_DIVISOR));
  createTarget(bloomNear, part(w, Look::BLOOM_NEAR_DIVISOR),
               part(h, Look::BLOOM_NEAR_DIVISOR));
  createTarget(bloomNearTmp, bloomNear.w, bloomNear.h);
  createTarget(bloomWide, part(w, Look::BLOOM_WIDE_DIVISOR),
               part(h, Look::BLOOM_WIDE_DIVISOR));
  createTarget(bloomWideTmp, bloomWide.w, bloomWide.h);
}

void PresentationLayer::createBlobTexture() {
  std::vector<uint8_t> pixels(BLOB_SIZE * BLOB_SIZE * 4);
  const float half = BLOB_SIZE * 0.5f;
  for (int y = 0; y < BLOB_SIZE; y++) {
    for (int x = 0; x < BLOB_SIZE; x++) {
      float dx = (x + 0.5f - half) / half;
      float dy = (y + 0.5f - half) / half;
      float f = std::max(0.0f, 1.0f - std::sqrt(dx * dx + dy * dy));
      uint8_t* p = &pixels[(y * BLOB_SIZE + x) * 4];
      p[0] = p[1] = p[2] = 255;
      // Smooth both at the rim and at the core.
      p[3] = static_cast<uint8_t>(255.0f * f * f * (3.0f - 2.0f * f));
    }
  }
  glGenTextures(1, &blobTexture);
  glBindTexture(GL_TEXTURE_2D, blobTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, BLOB_SIZE, BLOB_SIZE, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
}

bool PresentationLayer::beginScene(const Camera& camera) {
  // Every scene must reach the screen before the next one starts.
  assert(!open && "previous scene was never resolved");
  enabled = wantEnabled;
  queued = false;
  if (!enabled) return false;

  GLint viewport[4] = {0, 0, 0, 0};
  glGetIntegerv(GL_VIEWPORT, viewport);
  ensureTargets(viewport[2], viewport[3]);

  pools.clear();
  if (lightProvider) lightProvider(pools);
  assert(pools.size() <= static_cast<size_t>(Look::MAX_LIGHT_POOLS));
  renderLightMap(camera);

  glBindFramebuffer(GL_FRAMEBUFFER, scene.fbo);
  glViewport(0, 0, width, height);
  open = true;
  return true;
}

void PresentationLayer::renderLightMap(const Camera& camera) {
  assert(camera.screenWidth > 0 && camera.screenHeight > 0);
  assert(camera.zoom > 0.0f);
  const float screenW = static_cast<float>(camera.screenWidth);
  const float screenH = static_cast<float>(camera.screenHeight);

  poolVertices.clear();
  for (const LightPool& pool : pools) {
    assert(pool.radius > 0.0f && pool.strength >= 0.0f);
    assert(std::isfinite(pool.x) && std::isfinite(pool.y));
    const float cx = (pool.x - camera.x) * camera.zoom + screenW * 0.5f;
    const float cy = (pool.y - camera.y) * camera.zoom + screenH * 0.5f;
    const float rx = pool.radius * camera.zoom;
    const float ry = rx * Look::LIGHT_POOL_SQUASH;
    if (cx + rx < 0.0f || cx - rx > screenW || cy + ry < 0.0f ||
        cy - ry > screenH) {
      continue;
    }
    // Screen pixels (y down) to clip space (y up).
    const float x0 = (cx - rx) / screenW * 2.0f - 1.0f;
    const float x1 = (cx + rx) / screenW * 2.0f - 1.0f;
    const float y0 = 1.0f - (cy - ry) / screenH * 2.0f;
    const float y1 = 1.0f - (cy + ry) / screenH * 2.0f;
    const float scale = pool.strength / Look::LIGHT_RANGE;
    const float r = pool.r * scale, g = pool.g * scale, b = pool.b * scale;
    const float quad[POOL_VERTICES][4] = {
        {x0, y0, -1.0f, -1.0f}, {x1, y0, 1.0f, -1.0f}, {x1, y1, 1.0f, 1.0f},
        {x0, y0, -1.0f, -1.0f}, {x1, y1, 1.0f, 1.0f},  {x0, y1, -1.0f, 1.0f}};
    for (const auto& v : quad) {
      poolVertices.insert(poolVertices.end(), {v[0], v[1], v[2], v[3], r, g, b});
    }
  }

  glBindFramebuffer(GL_FRAMEBUFFER, lightMap.fbo);
  glViewport(0, 0, lightMap.w, lightMap.h);
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  if (!poolVertices.empty()) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);  // pools add up
    glUseProgram(poolProgram);
    glBindVertexArray(poolVAO);
    glBindBuffer(GL_ARRAY_BUFFER, poolVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, poolVertices.size() * sizeof(float),
                    poolVertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDrawArrays(GL_TRIANGLES, 0,
                 static_cast<GLsizei>(poolVertices.size() / POOL_VERTEX_FLOATS));
    glBindVertexArray(0);
    glUseProgram(0);
  }
  setStandardBlend();
  OpenGLUtils::checkGLError("PresentationLayer::renderLightMap");
}

void PresentationLayer::bindLightMap(int textureUnit) const {
  assert(textureUnit >= 0 && lightMap.texture != 0);
  glActiveTexture(GL_TEXTURE0 + textureUnit);
  glBindTexture(GL_TEXTURE_2D, lightMap.texture);
  glActiveTexture(GL_TEXTURE0);
}

void PresentationLayer::drawFullscreen(const Target& destination) const {
  glBindFramebuffer(GL_FRAMEBUFFER, destination.fbo);
  glViewport(0, 0, destination.w, destination.h);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

void PresentationLayer::resolve() {
  assert(open && "resolve without a scene");
  assert(scene.fbo != 0 && bloomNear.fbo != 0 && bloomWide.fbo != 0);

  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glBindVertexArray(fullscreenVAO);
  glActiveTexture(GL_TEXTURE0);

  // What was drawn as light, at quarter resolution.
  glUseProgram(brightProgram);
  glUniform2f(brightTexel, 1.0f / scene.w, 1.0f / scene.h);
  glBindTexture(GL_TEXTURE_2D, scene.texture);
  drawFullscreen(bloomNear);

  // Tight glow.
  glUseProgram(blurProgram);
  glUniform2f(blurStep, 1.0f / bloomNear.w, 0.0f);
  glBindTexture(GL_TEXTURE_2D, bloomNear.texture);
  drawFullscreen(bloomNearTmp);
  glUniform2f(blurStep, 0.0f, 1.0f / bloomNear.h);
  glBindTexture(GL_TEXTURE_2D, bloomNearTmp.texture);
  drawFullscreen(bloomNear);

  // Wide glow: the tight one blurred again at half its size.
  glUniform2f(blurStep, 1.0f / bloomWide.w, 0.0f);
  glBindTexture(GL_TEXTURE_2D, bloomNear.texture);
  drawFullscreen(bloomWideTmp);
  glUniform2f(blurStep, 0.0f, 1.0f / bloomWide.h);
  glBindTexture(GL_TEXTURE_2D, bloomWideTmp.texture);
  drawFullscreen(bloomWide);

  // Scene + glow, vignette and grade, to the screen.
  glUseProgram(compositeProgram);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, bloomWide.texture);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, bloomNear.texture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, scene.texture);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width, height);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  glBindTexture(GL_TEXTURE_2D, 0);
  glBindVertexArray(0);
  glUseProgram(0);
  setStandardBlend();
  open = false;
  OpenGLUtils::checkGLError("PresentationLayer::resolve");
}

void PresentationLayer::blendGlow() {
  // Normal alpha blend; alpha accumulates coverage (the emissive mask).
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                      GL_ONE_MINUS_SRC_ALPHA);
}

void PresentationLayer::blendMatte() {
  // Normal alpha blend; what it covers stops being light.
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO,
                      GL_ONE_MINUS_SRC_ALPHA);
}
