#pragma once

enum class GameState {
  TitleScreen,      // Landing screen with background and music
  CharacterSelect,  // Character selection screen
  MainMenu,         // Title screen, settings, quit
  Playing,          // Gameplay, HUD visible
  Paused,           // Pause menu, gameplay frozen
  Inventory         // Inventory screen, gameplay frozen
};

struct GameStateChangedEvent {
  GameState previousState;
  GameState newState;
};
