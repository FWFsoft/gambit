#include "InputSystem.h"

#include "Logger.h"

InputSystem::InputSystem(CollisionDebugRenderer* collisionDebugRenderer,
                         MusicZoneDebugRenderer* musicZoneDebugRenderer)
    : moveLeft(false),
      moveRight(false),
      moveUp(false),
      moveDown(false),
      inputSequence(0),
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

  // Attack input (Spacebar)
  if (e.key == SDLK_SPACE) {
    Logger::info("InputSystem: SPACEBAR PRESSED - publishing AttackInputEvent");
    EventBus::instance().publish(AttackInputEvent{});
  }

  if (e.key == SDLK_a || e.key == SDLK_LEFT) moveLeft = true;
  if (e.key == SDLK_d || e.key == SDLK_RIGHT) moveRight = true;
  if (e.key == SDLK_w || e.key == SDLK_UP) moveUp = true;
  if (e.key == SDLK_s || e.key == SDLK_DOWN) moveDown = true;
}

void InputSystem::onKeyUp(const KeyUpEvent& e) {
  if (e.key == SDLK_a || e.key == SDLK_LEFT) moveLeft = false;
  if (e.key == SDLK_d || e.key == SDLK_RIGHT) moveRight = false;
  if (e.key == SDLK_w || e.key == SDLK_UP) moveUp = false;
  if (e.key == SDLK_s || e.key == SDLK_DOWN) moveDown = false;
}

void InputSystem::onUpdate(const UpdateEvent& e) {
  // Publish input state every frame
  LocalInputEvent input;
  input.moveLeft = moveLeft;
  input.moveRight = moveRight;
  input.moveUp = moveUp;
  input.moveDown = moveDown;
  input.inputSequence = inputSequence++;

  EventBus::instance().publish(input);
}
