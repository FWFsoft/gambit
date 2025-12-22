#pragma once

#include "EventBus.h"

class InputSystem {
public:
    InputSystem();

private:
    bool moveLeft;
    bool moveRight;
    bool moveUp;
    bool moveDown;
    uint32_t inputSequence;

    void onKeyDown(const KeyDownEvent& e);
    void onKeyUp(const KeyUpEvent& e);
    void onUpdate(const UpdateEvent& e);
};
