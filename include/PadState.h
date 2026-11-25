#pragma once

// Shared controller state between the window, input and UI layers.
//
// The client is bound to at most one game controller. While a menu is up
// the controller drives the menu (ImGui gamepad navigation) and must not
// also drive the hero, so the UI says here when it has a panel open over
// the game.
#include <cstdint>

namespace PadState {

// A controller is connected and bound to this client.
inline bool bound = false;

// The controller was the last device used to play (aim comes from the
// stick, the reticle is drawn at the aim point instead of the cursor).
inline bool active = false;

// A panel that needs menu input is open over gameplay (suit infusions,
// end screen). Set by the HUD every frame.
inline bool overlayOpen = false;

// The controller asked to open / close the suit screen (Back button).
// Consumed by the HUD.
inline bool toggleSuitRequested = false;

// A button, trigger or stick is being held right now. A menu waits for
// this to clear before it takes controller input, so the trigger you were
// firing when the round ended does not press a menu button.
inline bool holding = false;

// Y is held: show the scoreboard (Tab on the keyboard).
inline bool scoreboardHeld = false;

// How far the reticle reaches at full tilt, world px. Set from the local
// hero's kit (PadAim::reach), so the reticle can go as far as their
// longest placed ability and no farther.
inline float aimReach = 320.0f;

// The ally the stick is pointing at, for a hero with an ability that picks
// an ally (PadAim::allyTargeting). Set by the HUD every frame, which also
// marks them. While one of allyButtons is held, the aim is that ally.
inline bool allyAimed = false;
inline float allyX = 0.0f, allyY = 0.0f;
inline uint8_t allyButtons = 0;

// The way the stick is aiming, before any snap onto an ally (offset from
// the hero, world px). The ally is chosen from this, so holding the button
// does not lock the choice onto whoever was picked first.
inline float aimDirX = 0.0f, aimDirY = 0.0f;

}  // namespace PadState
