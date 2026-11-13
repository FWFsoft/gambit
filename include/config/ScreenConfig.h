#pragma once

// Screen and viewport configuration
// All rendering systems should use these constants

namespace Config {
namespace Screen {

// Window dimensions (16:9 aspect ratio for character selection UI)
constexpr int WIDTH = 1280;
constexpr int HEIGHT = 720;

// How much the world is magnified on screen. Sprites are authored at 1x.
// 0.75 puts roughly 1700x960 world pixels in view and a ~78px hero at about
// 8% of the screen height: a pulled-back ARPG view where you read the room,
// not just your hero. Raise it to move the camera in.
constexpr float WORLD_ZOOM = 0.75f;

// Viewport bounds for orthographic projection
constexpr float ORTHO_LEFT = 0.0f;
constexpr float ORTHO_RIGHT = static_cast<float>(WIDTH);
constexpr float ORTHO_BOTTOM = static_cast<float>(HEIGHT);
constexpr float ORTHO_TOP = 0.0f;
constexpr float ORTHO_NEAR = -1.0f;
constexpr float ORTHO_FAR = 1.0f;

}  // namespace Screen
}  // namespace Config
