#pragma once

#include "GameState.h"

class GameStateManager {
 public:
  static GameStateManager& instance();

  GameState getCurrentState() const { return currentState; }
  void transitionTo(GameState newState);

  // Prevent copying
  GameStateManager(const GameStateManager&) = delete;
  GameStateManager& operator=(const GameStateManager&) = delete;

 private:
  GameStateManager() : currentState(GameState::MainMenu) {}
  GameState currentState;
};
