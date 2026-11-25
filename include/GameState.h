#pragma once

enum class GameState {
  TitleScreen,      // Landing screen with background and music
  CharacterSelect,  // Character selection screen
  MainMenu,         // Title screen, settings, quit
  Playing,          // Gameplay, HUD visible
  Paused,           // Pause menu, gameplay frozen
  Inventory         // Inventory screen, gameplay frozen
};

// Whether the match world is on screen (and audible): playing, or a pause
// or inventory overlay over it. Menus never show or play the match behind
// them, though the match itself (shared server) keeps running.
constexpr bool showsMatchWorld(GameState state) {
  return state == GameState::Playing || state == GameState::Paused ||
         state == GameState::Inventory;
}

struct GameStateChangedEvent {
  GameState previousState;
  GameState newState;
};
