#pragma once

// Camera and fog of war tuning. Everything a designer would want to change
// about how much of the world you see is here.
//
// Distances are world pixels (1 world pixel = WORLD_ZOOM screen pixels).

namespace Config {
namespace CameraView {

// --------------------------------------------------------------------------
// Camera modes
//   Locked:   the camera stays centred on your hero.
//   Unlocked: the camera stays where you leave it. Push the cursor against a
//             screen edge to pan; hold Space to snap back to your hero.
// Y switches between the two.
// --------------------------------------------------------------------------
constexpr bool START_LOCKED = true;
// The cursor pans the camera when it is this close to a screen edge.
constexpr float EDGE_PAN_MARGIN = 28.0f;  // screen pixels
// Pan speed with the cursor right on the edge. A full screen width is about
// 1700 world pixels, so this crosses one screen in roughly a second.
constexpr float EDGE_PAN_SPEED = 1600.0f;  // world pixels per second

// --------------------------------------------------------------------------
// Fog of war: the crew shares sight. Ground no one has seen is hidden;
// ground seen earlier stays on the map, dimmed, without its enemies.
// --------------------------------------------------------------------------
constexpr float PLAYER_SIGHT = 640.0f;         // around every living hero
constexpr float DOWNED_PLAYER_SIGHT = 260.0f;  // enough to see the revive
constexpr float SHIP_SIGHT = 520.0f;           // home is always in view
// Sight fades out over this distance at its edge instead of ending on a line.
constexpr float SIGHT_SOFT_EDGE = 150.0f;
// Enemies, pickups and shots are drawn once they are this far into sight
// (0 = the very edge, 1 = fully seen).
constexpr float REVEAL_THRESHOLD = 0.12f;

// One fog texel covers this many world pixels.
constexpr float FOG_CELL = 32.0f;

// How strongly the fog covers the world (0 = clear, 1 = solid).
constexpr float FOG_UNEXPLORED = 0.94f;
constexpr float FOG_EXPLORED = 0.62f;
// Fog colour: a cold haze a little lighter than the night sky, so fog reads
// as fog and not as more dark floor.
constexpr int FOG_R = 13;
constexpr int FOG_G = 17;
constexpr int FOG_B = 30;

// What the crew can see is lifted out of the dark by a wide, faint, cool
// light (a light pool of the look, LookConfig.h).
// The pool is brightest at its centre and fades to nothing at its rim, so
// it is drawn wider than the sight it lights.
constexpr float SIGHT_LIGHT_RADIUS_SCALE = 1.5f;
constexpr float SIGHT_LIGHT_STRENGTH = 1.35f;
constexpr float SIGHT_LIGHT_R = 0.62f;
constexpr float SIGHT_LIGHT_G = 0.74f;
constexpr float SIGHT_LIGHT_B = 1.00f;

}  // namespace CameraView
}  // namespace Config
