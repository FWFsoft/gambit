#include "InputSystem.h"

#include "ClientPrediction.h"
#include "GameStateManager.h"
#include "Logger.h"
#include "NetworkProtocol.h"
#include "Player.h"

InputSystem::InputSystem(ClientPrediction* clientPrediction,
                         CollisionDebugRenderer* collisionDebugRenderer,
                         MusicZoneDebugRenderer* musicZoneDebugRenderer,
                         ObjectiveDebugRenderer* objectiveDebugRenderer)
    : moveLeft(false),
      moveRight(false),
      moveUp(false),
      moveDown(false),
      interactHeld(false),
      inputSequence(0),
      clientPrediction(clientPrediction),
      collisionDebugRenderer(collisionDebugRenderer),
      musicZoneDebugRenderer(musicZoneDebugRenderer),
      objectiveDebugRenderer(objectiveDebugRenderer) {
  // Subscribe to key events
  EventBus::instance().subscribe<KeyDownEvent>(
      [this](const KeyDownEvent& e) { onKeyDown(e); });

  EventBus::instance().subscribe<KeyUpEvent>(
      [this](const KeyUpEvent& e) { onKeyUp(e); });

  EventBus::instance().subscribe<MouseMoveEvent>(
      [this](const MouseMoveEvent& e) { onMouseMove(e); });
  EventBus::instance().subscribe<MouseButtonEvent>(
      [this](const MouseButtonEvent& e) { onMouseButton(e); });

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
      buttons = 0;
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
      buttons = 0;
      if (clientPrediction) {
        Player& localPlayer = clientPrediction->getLocalPlayerMutable();
        localPlayer.getAnimationController()->updateAnimationState(0.0f, 0.0f);
      }
      gsm.transitionTo(GameState::Inventory);
    }
    return;
  }

  // Toggle all debug renderers with F1
  if (e.key == SDLK_F1) {
    if (collisionDebugRenderer) {
      collisionDebugRenderer->toggle();
      Logger::info(
          "Collision debug: " +
          std::string(collisionDebugRenderer->isEnabled() ? "ON" : "OFF"));
    }
    if (musicZoneDebugRenderer) {
      musicZoneDebugRenderer->toggle();
      Logger::info(
          "Music zone debug: " +
          std::string(musicZoneDebugRenderer->isEnabled() ? "ON" : "OFF"));
    }
    if (objectiveDebugRenderer) {
      objectiveDebugRenderer->toggle();
      Logger::info(
          "Objective debug: " +
          std::string(objectiveDebugRenderer->isEnabled() ? "ON" : "OFF"));
    }
  }

  // Toggle mute with M or F2
  if (e.key == SDLK_m || e.key == SDLK_F2) {
    EventBus::instance().publish(ToggleMuteEvent{});
  }

  // Only process game input during Playing state
  if (currentState != GameState::Playing) {
    return;
  }

  // Abilities: LMB / RMB come from the mouse; these are the keyboard ones.
  // Space doubles as the basic attack for trackpads.
  if (e.key == SDLK_q) buttons |= InputButtons::Special;
  if (e.key == SDLK_LSHIFT) buttons |= InputButtons::Mobility;
  if (e.key == SDLK_e) buttons |= InputButtons::Core;
  if (e.key == SDLK_SPACE) buttons |= InputButtons::Primary;

  // Interact input (F key) - for objectives
  if (e.key == SDLK_f) {
    if (!interactHeld) {
      interactHeld = true;
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
  if (e.key == SDLK_f) interactHeld = false;
  if (e.key == SDLK_q) buttons &= ~InputButtons::Special;
  if (e.key == SDLK_LSHIFT) buttons &= ~InputButtons::Mobility;
  if (e.key == SDLK_e) buttons &= ~InputButtons::Core;
  if (e.key == SDLK_SPACE) buttons &= ~InputButtons::Primary;
}

void InputSystem::onMouseMove(const MouseMoveEvent& e) {
  mouseX = e.x;
  mouseY = e.y;
}

void InputSystem::onMouseButton(const MouseButtonEvent& e) {
  mouseX = e.x;
  mouseY = e.y;
  uint8_t bit = 0;
  if (e.button == SDL_BUTTON_LEFT) bit = InputButtons::Primary;
  if (e.button == SDL_BUTTON_RIGHT) bit = InputButtons::Secondary;
  if (bit == 0) return;
  if (!e.down) {
    buttons &= ~bit;
    return;
  }
  // Only start abilities while actually playing (not in menus).
  if (GameStateManager::instance().getCurrentState() == GameState::Playing) {
    buttons |= bit;
  }
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
  input.buttons = buttons;
  input.mouseX = mouseX;
  input.mouseY = mouseY;

  EventBus::instance().publish(input);
}
