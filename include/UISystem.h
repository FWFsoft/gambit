#pragma once

#include "EventBus.h"
#include "Settings.h"

class Window;
class ClientPrediction;

// UISystem manages ImGui frame lifecycle and rendering
// Subscribes to RenderEvent to draw UI after game world
class UISystem {
 public:
  UISystem(Window* window, ClientPrediction* clientPrediction);
  ~UISystem() = default;

  // Called each frame to render UI
  void render();

 private:
  void onRender(const RenderEvent& e);

  // Menu rendering
  void renderMainMenu();
  void renderSettingsPanel();

  // Game UI rendering
  void renderHUD();
  void renderPauseMenu();
  void renderInventory();

  // State
  Window* window;
  ClientPrediction* clientPrediction;
  Settings settings;
  Settings tempSettings;  // Temporary settings during editing
  bool showSettings;
};
