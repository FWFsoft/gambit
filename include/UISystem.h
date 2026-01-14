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
class DamageNumberSystem;
class EffectTracker;
struct ItemDefinition;

// UISystem manages ImGui frame lifecycle and rendering
// Subscribes to RenderEvent to draw UI after game world
class UISystem {
 public:
  UISystem(Window* window, ClientPrediction* clientPrediction,
           NetworkClient* client, DamageNumberSystem* damageNumbers,
           EffectTracker* effectTracker);
  ~UISystem();

  // Called each frame to render UI
  void render();

 private:
  void onRender(const RenderEvent& e);

  // Menu rendering
  void renderTitleScreen();
  void renderCharacterSelect();
  void renderMainMenu();
  void renderSettingsPanel();

  // Game UI rendering
  void renderHUD();
  void renderPauseMenu();
  void renderInventory();
  void renderDeathScreen();

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

  // Objective notification system
  struct ObjectiveNotification {
    uint32_t objectiveId;
    std::string name;
    uint8_t state;    // ObjectiveState enum value
    float progress;   // 0.0 - 1.0
    float timestamp;  // Time when notification was created/updated
  };
  void onObjectiveUpdated(const ObjectiveUpdatedEvent& e);
  void renderObjectiveActivity();

  // Effect display
  void renderEffectBars();

  // State
  Window* window;
  ClientPrediction* clientPrediction;
  NetworkClient* client;
  DamageNumberSystem* damageNumberSystem;
  EffectTracker* effectTracker;
  Settings settings;
  Settings tempSettings;  // Temporary settings during editing
  bool showSettings;
  std::deque<Notification> notifications;  // Active pickup notifications
  std::deque<ObjectiveNotification>
      objectiveNotifications;  // Active objective notifications
  float currentTime;           // Accumulated time in seconds

  // Title screen assets
  Texture* titleScreenBackground;
  Mix_Music* titleMusic;

  // Character selection state
  uint32_t hoveredCharacterId;   // Character mouse is over (0 = none)
  uint32_t selectedCharacterId;  // Character selected for preview (0 = none)
  float selectionAnimationTime;  // For pulsing/growing effect
  int keyboardFocusedIndex;      // Character focused by keyboard (-1 = none)

  // Keyboard navigation event handlers
  void onKeyDown(const KeyDownEvent& e);
};
