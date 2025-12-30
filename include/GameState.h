#pragma once

enum class GameState {
  MainMenu,  // Title screen, settings, quit
  Playing,   // Gameplay, HUD visible
  Paused,    // Pause menu, gameplay frozen
  Inventory  // Inventory screen, gameplay frozen
};

struct GameStateChangedEvent {
  GameState previousState;
  GameState newState;
};
