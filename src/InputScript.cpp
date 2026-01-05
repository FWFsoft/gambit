#include "InputScript.h"

#include <algorithm>

#include "EventBus.h"
#include "Logger.h"

InputScript::InputScript() : nextActionIndex(0) {}

InputScript::~InputScript() {}

void InputScript::addKeyPress(uint64_t frame, int key, uint64_t duration) {
  // Add key down at frame
  actions.push_back({frame, key, true, 0});

  // Add key up after duration
  actions.push_back({frame + duration, key, false, 0});

  // Keep actions sorted by frame for efficient processing
  std::sort(actions.begin(), actions.end(),
            [](const InputAction& a, const InputAction& b) {
              return a.frame < b.frame;
            });
}

void InputScript::addKeyDown(uint64_t frame, int key) {
  actions.push_back({frame, key, true, 0});

  std::sort(actions.begin(), actions.end(),
            [](const InputAction& a, const InputAction& b) {
              return a.frame < b.frame;
            });
}

void InputScript::addKeyUp(uint64_t frame, int key) {
  actions.push_back({frame, key, false, 0});

  std::sort(actions.begin(), actions.end(),
            [](const InputAction& a, const InputAction& b) {
              return a.frame < b.frame;
            });
}

void InputScript::addMove(uint64_t startFrame, uint64_t duration, bool left,
                          bool right, bool up, bool down) {
  // Add key down events for all directions at start frame
  if (left) addKeyDown(startFrame, SDLK_a);
  if (right) addKeyDown(startFrame, SDLK_d);
  if (up) addKeyDown(startFrame, SDLK_w);
  if (down) addKeyDown(startFrame, SDLK_s);

  // Add key up events for all directions at end frame
  uint64_t endFrame = startFrame + duration;
  if (left) addKeyUp(endFrame, SDLK_a);
  if (right) addKeyUp(endFrame, SDLK_d);
  if (up) addKeyUp(endFrame, SDLK_w);
  if (down) addKeyUp(endFrame, SDLK_s);
}

void InputScript::processFrame(uint64_t currentFrame) {
  // Process all actions scheduled for this frame
  while (nextActionIndex < actions.size() &&
         actions[nextActionIndex].frame == currentFrame) {
    const auto& action = actions[nextActionIndex];

    if (action.isKeyDown) {
      EventBus::instance().publish(KeyDownEvent{action.key});
      Logger::debug("InputScript: KeyDown " + std::to_string(action.key) +
                    " at frame " + std::to_string(currentFrame));
    } else {
      EventBus::instance().publish(KeyUpEvent{action.key});
      Logger::debug("InputScript: KeyUp " + std::to_string(action.key) +
                    " at frame " + std::to_string(currentFrame));
    }

    nextActionIndex++;
  }
}

void InputScript::clear() {
  actions.clear();
  nextActionIndex = 0;
}
