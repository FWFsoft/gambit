#pragma once

// The look: inked heroes on a dark world (docs/design/ART_DIRECTION.md).
//
// Every strength of the presentation layer lives here so the look can be
// tuned in one place. The starting numbers come from the style frame
// (tools/sprite_forge/styleframe.py); where one was changed after looking
// at real frames, the comment says so.
//
// Value is what makes the game read (Clarity): dark world, mid-to-bright
// characters, effects brighter than both.

namespace Config {
namespace Look {

// --------------------------------------------------------------------------
// 1. World values: the floor is dark, cool and low in contrast.
//    floor = (FLOOR_VALUE_MIN + desaturated tile * FLOOR_VALUE_RANGE) * tint
//    Applied in the tile shader, so every tileset is covered.
// --------------------------------------------------------------------------
constexpr float FLOOR_VALUE_MIN = 0.045f;    // black tile texels land here
constexpr float FLOOR_VALUE_RANGE = 0.200f;  // white texels: MIN + RANGE
constexpr float FLOOR_SATURATION = 0.80f;    // how much tile hue survives
constexpr float FLOOR_TINT_R = 0.82f;        // cool slate (style frame)
constexpr float FLOOR_TINT_G = 0.92f;
constexpr float FLOOR_TINT_B = 1.25f;
// Darkens the floor (only the floor) toward the screen edges.
constexpr float FLOOR_VIGNETTE = 0.40f;
constexpr float FLOOR_VIGNETTE_START = 0.35f;
constexpr float FLOOR_VIGNETTE_POWER = 1.5f;

// --------------------------------------------------------------------------
// 2. Grounding: one fixed key light for every shadow in the game.
//    Must match KEY_LIGHT and CAMERA_ELEVATION_DEG in
//    tools/sprite_forge/forge.py, which lights the baked sprites
//    (x right, y away from the camera, z up).
// --------------------------------------------------------------------------
constexpr float KEY_LIGHT_X = -0.55f;
constexpr float KEY_LIGHT_Y = -0.50f;
constexpr float KEY_LIGHT_Z = 0.68f;
constexpr float CAMERA_COS = 0.8660254f;  // cos(30 degrees)
constexpr float CAMERA_TAN = 0.5773503f;  // tan(30 degrees)
// Where the shadow of a point one sprite pixel above the ground lands,
// in screen pixels: to the right, and up the screen (behind the actor).
constexpr float SHADOW_SHEAR_X = -KEY_LIGHT_X / KEY_LIGHT_Z / CAMERA_COS;
constexpr float SHADOW_SHEAR_Y = KEY_LIGHT_Y / KEY_LIGHT_Z * CAMERA_TAN;
constexpr float CAST_SHADOW_ALPHA = 0.45f;
constexpr float CONTACT_SHADOW_ALPHA = 0.55f;
constexpr float CONTACT_SHADOW_WIDTH = 1.35f;   // times the body radius
constexpr float CONTACT_SHADOW_SQUASH = 0.45f;  // height / width

// --------------------------------------------------------------------------
// 3. Light pools: radial, squashed 2:1 for the isometric floor, summed
//    into a low-resolution light map that the floor shader reads.
//    floor *= 1 + light;  light = sum / (1 + sum * LIGHT_KNEE)
// --------------------------------------------------------------------------
constexpr int LIGHT_MAP_DIVISOR = 4;       // light map is 1/4 resolution
constexpr float LIGHT_RANGE = 8.0f;        // brightest sum the map can hold
constexpr float LIGHT_KNEE = 0.10f;        // crowds of pools roll off
constexpr float LIGHT_ADD = 0.03f;         // a hint of light on black ground
constexpr float LIGHT_POOL_SQUASH = 0.5f;  // 2:1
constexpr int MAX_LIGHT_POOLS = 256;

// A warm pool around every player.
constexpr float PLAYER_POOL_RADIUS = 300.0f;  // world pixels
constexpr float PLAYER_POOL_STRENGTH = 3.2f;
constexpr float PLAYER_POOL_R = 1.00f;
constexpr float PLAYER_POOL_G = 0.62f;
constexpr float PLAYER_POOL_B = 0.32f;
constexpr float HIDDEN_POOL_SCALE = 0.4f;  // stealthed players glow less

// Enemies stand in a faint pool of their genus colour: genus colour arrives
// as light, and a dark body never sits on a dark floor unlit.
constexpr float ENEMY_POOL_RADIUS_SCALE = 3.4f;  // times the body radius
constexpr float ENEMY_POOL_RADIUS_MIN = 60.0f;
constexpr float ENEMY_POOL_STRENGTH = 1.8f;
constexpr float ELITE_POOL_STRENGTH = 2.4f;
// The boss body is dark: it gets the strongest pool so it never sits on an
// unlit floor (Clarity).
constexpr float BOSS_POOL_STRENGTH = 4.5f;
constexpr float BOSS_POOL_RADIUS_SCALE = 1.6f;  // on top of the enemy scale
// While an enemy winds up a hit its pool turns danger red (0 -> this).
constexpr float ENEMY_WINDUP_POOL_STRENGTH = 2.5f;

// Effects light the ground around them; strengths fade with the effect.
constexpr float FX_HIT_STRENGTH = 3.5f;
constexpr float FX_HIT_RADIUS_MIN = 70.0f;
constexpr float FX_BURST_STRENGTH = 3.0f;
constexpr float FX_BURST_RADIUS_SCALE = 1.9f;
constexpr float FX_SWING_STRENGTH = 2.2f;  // slashes and cones
constexpr float FX_RING_STRENGTH = 1.8f;
constexpr float FX_BEAM_STRENGTH = 2.5f;
constexpr float FX_TRAIL_STRENGTH = 1.6f;  // dashes, tethers, lobs, puffs
constexpr float FX_TRAIL_RADIUS = 60.0f;
constexpr float PROJECTILE_POOL_STRENGTH = 2.0f;
constexpr float PROJECTILE_POOL_RADIUS_SCALE = 5.0f;
constexpr float ZONE_POOL_STRENGTH = 1.6f;
constexpr float ZONE_POOL_RADIUS_SCALE = 1.3f;
constexpr float PICKUP_POOL_STRENGTH = 1.2f;
constexpr float PICKUP_POOL_RADIUS = 48.0f;
// Telegraphs brighten as the attack is about to land.
constexpr float TELEGRAPH_POOL_STRENGTH_START = 0.6f;
constexpr float TELEGRAPH_POOL_STRENGTH_END = 1.6f;
constexpr float TELEGRAPH_POOL_RADIUS_SCALE = 1.25f;

// --------------------------------------------------------------------------
// 4. Bloom and grade, applied to the world only (never the HUD).
//    Only pixels drawn as light (effects, telegraphs, auras) bloom;
//    sprites and the floor do not.
// --------------------------------------------------------------------------
constexpr int BLOOM_NEAR_DIVISOR = 4;  // tight glow, 1/4 resolution
constexpr int BLOOM_WIDE_DIVISOR = 8;  // wide glow, 1/8 resolution
constexpr float BLOOM_THRESHOLD = 0.25f;
constexpr float BLOOM_KNEE = 0.25f;
constexpr float BLOOM_GAIN_NEAR = 0.80f;
constexpr float BLOOM_GAIN_WIDE = 0.60f;
// Grade: soft shoulder, slight gamma lift, a touch more saturation.
constexpr float GRADE_SHOULDER = 0.18f;
constexpr float GRADE_GAIN = 1.12f;
constexpr float GRADE_GAMMA = 0.94f;
constexpr float GRADE_SATURATION = 1.12f;
// A mild vignette over the whole world. Kept weak on purpose: enemies
// coming in from the screen edge must stay readable (Clarity).
constexpr float SCENE_VIGNETTE = 0.12f;

// Colour beyond the map edge (the night sky).
constexpr float VOID_R = 0.03f;
constexpr float VOID_G = 0.03f;
constexpr float VOID_B = 0.06f;

}  // namespace Look
}  // namespace Config
