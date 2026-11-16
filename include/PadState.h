#pragma once

// Shared controller state between the window, input and UI layers.
//
// The client is bound to at most one game controller. While a menu is up
// the controller drives the menu (ImGui gamepad navigation) and must not
// also drive the hero, so the UI says here when it has a panel open over
// the game.
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

}  // namespace PadState
