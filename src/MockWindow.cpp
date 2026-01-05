#include "MockWindow.h"

#include "InputScript.h"
#include "Logger.h"

MockWindow::MockWindow(const std::string& title, int width, int height)
    : title(title),
      width(width),
      height(height),
      openFlag(true),
      inputScript(nullptr),
      currentFrame(0) {
  Logger::info("MockWindow created: " + title + " (" + std::to_string(width) +
               "x" + std::to_string(height) + ")");
}

MockWindow::~MockWindow() { Logger::info("MockWindow destroyed"); }

void MockWindow::pollEvents() {
  // If we have an input script, process scripted inputs for this frame
  if (inputScript) {
    inputScript->processFrame(currentFrame);
  }

  // No actual SDL event polling in headless mode
}

bool MockWindow::isOpen() const { return openFlag; }

void MockWindow::close() {
  openFlag = false;
  Logger::info("MockWindow closed");
}

void* MockWindow::getWindow() const {
  // Return nullptr since there's no real SDL window
  return nullptr;
}

void MockWindow::initImGui() {
  // No-op: ImGui not needed in headless mode
  Logger::debug("MockWindow: ImGui initialization skipped (headless mode)");
}

void MockWindow::shutdownImGui() {
  // No-op: ImGui not used in headless mode
  Logger::debug("MockWindow: ImGui shutdown skipped (headless mode)");
}

void MockWindow::setInputScript(InputScript* script) {
  inputScript = script;
  Logger::info("MockWindow: Input script set");
}

void MockWindow::setFrameNumber(uint64_t frame) { currentFrame = frame; }
