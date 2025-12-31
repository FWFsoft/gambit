#pragma once

// Screen and viewport configuration
// All rendering systems should use these constants

namespace Config {
namespace Screen {

// Window dimensions (16:9 aspect ratio for character selection UI)
constexpr int WIDTH = 1280;
constexpr int HEIGHT = 720;

// Viewport bounds for orthographic projection
constexpr float ORTHO_LEFT = 0.0f;
constexpr float ORTHO_RIGHT = static_cast<float>(WIDTH);
constexpr float ORTHO_BOTTOM = static_cast<float>(HEIGHT);
constexpr float ORTHO_TOP = 0.0f;
constexpr float ORTHO_NEAR = -1.0f;
constexpr float ORTHO_FAR = 1.0f;

}  // namespace Screen
}  // namespace Config
