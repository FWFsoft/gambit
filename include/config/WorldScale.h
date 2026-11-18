#pragma once

// Real-world scale of the planet. Everything the world draws is sized from
// this, so scenery reads true against the heroes (a street lamp stands over
// you, a door is your height, a tower dwarfs you).
//
// Anchor: a hero stands about 100 world pixels tall (~78 px on screen at
// WORLD_ZOOM 0.75) and is about 1.8 m tall, so one metre is 55 world pixels.
// Heights and lengths along the tile axes both use it. One tile is about
// 0.65 m along an axis (35.78 world px).
//
// tools/maps/generate_planet.py has the same constant (METRE); keep them
// equal. Reference sizes: docs/design/MAP_PIPELINE.md, "Scale".

namespace Config {
namespace World {

constexpr float METRE = 55.0f;   // world pixels per metre
constexpr float STOREY = 3.5f;   // metres per building storey

}  // namespace World
}  // namespace Config
