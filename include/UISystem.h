#pragma once

#include <SDL_mixer.h>

#include <deque>
#include <string>

#include "EventBus.h"
#include "Settings.h"

class Window;
class ClientPrediction;
class NetworkClient;
class Texture;
struct ItemDefinition;

// UISystem manages ImGui frame lifecycle and rendering
// Subscribes to RenderEvent to draw UI after game world
class UISystem {
 public:
  UISystem(Window* window, ClientPrediction* clientPrediction,
           NetworkClient* client);
  ~UISystem();

  // Called each frame to render UI
  void render();

 private:
  void onRender(const RenderEvent& e);

  // Menu rendering
  void renderTitleScreen();
  void renderMainMenu();
  void renderSettingsPanel();

  // Game UI rendering
  void renderHUD();
  void renderPauseMenu();
  void renderInventory();

  // UI interaction handlers
  void handleInventorySlotClick(int slotIndex, const ItemDefinition& item);
  void handleEquipmentSlotClick(int equipmentSlotIndex);
  void handleConsumableUse(int slotIndex, const ItemDefinition& item);

  // Notification system
  struct Notification {
    std::string message;
    float timestamp;  // Time when notification was created
  };
  void onItemPickedUp(const ItemPickedUpEvent& e);
  void renderNotifications();

  // State
  Window* window;
  ClientPrediction* clientPrediction;
  NetworkClient* client;
  Settings settings;
  Settings tempSettings;  // Temporary settings during editing
  bool showSettings;
  std::deque<Notification> notifications;  // Active pickup notifications
  float currentTime;                       // Accumulated time in seconds

  // Title screen assets
  Texture* titleScreenBackground;
  Mix_Music* titleMusic;
};
