#include "GameStateManager.h"

#include "EventBus.h"
#include "Logger.h"

GameStateManager& GameStateManager::instance() {
  static GameStateManager instance;
  return instance;
}

void GameStateManager::transitionTo(GameState newState) {
  if (newState == currentState) {
    return;
  }

  GameState previousState = currentState;
  currentState = newState;

  // Log state transition
  const char* stateNames[] = {"TitleScreen", "CharacterSelect", "MainMenu",
                              "Playing",     "Paused",          "Inventory"};
  Logger::info("Game state transition: " +
               std::string(stateNames[static_cast<int>(previousState)]) +
               " -> " + std::string(stateNames[static_cast<int>(newState)]));

  // Publish event
  EventBus::instance().publish(GameStateChangedEvent{previousState, newState});
}
