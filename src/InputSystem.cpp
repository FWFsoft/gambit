#include "InputSystem.h"

InputSystem::InputSystem()
    : moveLeft(false), moveRight(false), moveUp(false), moveDown(false), inputSequence(0) {

    // Subscribe to key events
    EventBus::instance().subscribe<KeyDownEvent>([this](const KeyDownEvent& e) {
        onKeyDown(e);
    });

    EventBus::instance().subscribe<KeyUpEvent>([this](const KeyUpEvent& e) {
        onKeyUp(e);
    });

    // Subscribe to update events to publish input state
    EventBus::instance().subscribe<UpdateEvent>([this](const UpdateEvent& e) {
        onUpdate(e);
    });
}

void InputSystem::onKeyDown(const KeyDownEvent& e) {
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
