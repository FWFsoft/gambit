#include "InputSystem.h"

#include "ClientPrediction.h"
#include "GameStateManager.h"
#include "Logger.h"
#include "Player.h"

InputSystem::InputSystem(ClientPrediction* clientPrediction,
                         CollisionDebugRenderer* collisionDebugRenderer,
                         MusicZoneDebugRenderer* musicZoneDebugRenderer)
    : moveLeft(false),
      moveRight(false),
      moveUp(false),
      moveDown(false),
      interactHeld(false),
      inputSequence(0),
      clientPrediction(clientPrediction),
      collisionDebugRenderer(collisionDebugRenderer),
      musicZoneDebugRenderer(musicZoneDebugRenderer) {
  // Subscribe to key events
  EventBus::instance().subscribe<KeyDownEvent>(
      [this](const KeyDownEvent& e) { onKeyDown(e); });

  EventBus::instance().subscribe<KeyUpEvent>(
      [this](const KeyUpEvent& e) { onKeyUp(e); });

  // Subscribe to update events to publish input state
  EventBus::instance().subscribe<UpdateEvent>(
      [this](const UpdateEvent& e) { onUpdate(e); });
}

void InputSystem::onKeyDown(const KeyDownEvent& e) {
  GameStateManager& gsm = GameStateManager::instance();
  GameState currentState = gsm.getCurrentState();

  // TitleScreen: any key advances to character select
  if (currentState == GameState::TitleScreen) {
    gsm.transitionTo(GameState::CharacterSelect);
    return;
  }

  // CharacterSelect: ESC returns to title screen
  if (currentState == GameState::CharacterSelect) {
    if (e.key == SDLK_ESCAPE) {
      gsm.transitionTo(GameState::TitleScreen);
    }
    return;  // All other input handled by ImGui clicks
  }

  // ESC: Toggle pause (Playing <-> Paused)
  if (e.key == SDLK_ESCAPE) {
    if (currentState == GameState::Playing) {
      // Clear movement state and reset animation to idle when pausing
      moveLeft = moveRight = moveUp = moveDown = false;
      if (clientPrediction) {
        Player& localPlayer = clientPrediction->getLocalPlayerMutable();
        localPlayer.getAnimationController()->updateAnimationState(0.0f, 0.0f);
      }
      gsm.transitionTo(GameState::Paused);
    } else if (currentState == GameState::Paused) {
      gsm.transitionTo(GameState::Playing);
    }
    return;
  }

  // I: Toggle inventory (Playing/Paused <-> Inventory)
  if (e.key == SDLK_i) {
    if (currentState == GameState::Inventory) {
      gsm.transitionTo(GameState::Playing);
    } else if (currentState == GameState::Playing ||
               currentState == GameState::Paused) {
      // Clear movement state and reset animation to idle when opening inventory
      moveLeft = moveRight = moveUp = moveDown = false;
      if (clientPrediction) {
        Player& localPlayer = clientPrediction->getLocalPlayerMutable();
        localPlayer.getAnimationController()->updateAnimationState(0.0f, 0.0f);
      }
      gsm.transitionTo(GameState::Inventory);
    }
    return;
  }

  // Toggle collision and music zone debug with F1
  if (e.key == SDLK_F1 && collisionDebugRenderer) {
    collisionDebugRenderer->toggle();
    Logger::info(
        "Collision debug: " +
        std::string(collisionDebugRenderer->isEnabled() ? "ON" : "OFF"));
    musicZoneDebugRenderer->toggle();
    Logger::info(
        "Music zone debug: " +
        std::string(musicZoneDebugRenderer->isEnabled() ? "ON" : "OFF"));
  }

  // Toggle mute with M or F2
  if (e.key == SDLK_m || e.key == SDLK_F2) {
    EventBus::instance().publish(ToggleMuteEvent{});
  }

  // Only process game input during Playing state
  if (currentState != GameState::Playing) {
    return;
  }

  // Attack input (Spacebar)
  if (e.key == SDLK_SPACE) {
    Logger::info("InputSystem: SPACEBAR PRESSED - publishing AttackInputEvent");
    EventBus::instance().publish(AttackInputEvent{});
  }

  // Interact input (E key) - for objectives
  if (e.key == SDLK_e) {
    if (!interactHeld) {
      interactHeld = true;
      Logger::info("InputSystem: E PRESSED - publishing InteractInputEvent");
      EventBus::instance().publish(InteractInputEvent{});
    }
  }

  if (e.key == SDLK_a || e.key == SDLK_LEFT) moveLeft = true;
  if (e.key == SDLK_d || e.key == SDLK_RIGHT) moveRight = true;
  if (e.key == SDLK_w || e.key == SDLK_UP) moveUp = true;
  if (e.key == SDLK_s || e.key == SDLK_DOWN) moveDown = true;
}

void InputSystem::onKeyUp(const KeyUpEvent& e) {
  // Only process game input during Playing state
  if (GameStateManager::instance().getCurrentState() != GameState::Playing) {
    return;
  }

  if (e.key == SDLK_a || e.key == SDLK_LEFT) moveLeft = false;
  if (e.key == SDLK_d || e.key == SDLK_RIGHT) moveRight = false;
  if (e.key == SDLK_w || e.key == SDLK_UP) moveUp = false;
  if (e.key == SDLK_s || e.key == SDLK_DOWN) moveDown = false;
  if (e.key == SDLK_e) interactHeld = false;
}

void InputSystem::onUpdate(const UpdateEvent& e) {
  // Only publish input during Playing state
  if (GameStateManager::instance().getCurrentState() != GameState::Playing) {
    return;
  }

  // Publish input state every frame
  LocalInputEvent input;
  input.moveLeft = moveLeft;
  input.moveRight = moveRight;
  input.moveUp = moveUp;
  input.moveDown = moveDown;
  input.inputSequence = inputSequence++;

  EventBus::instance().publish(input);
}
