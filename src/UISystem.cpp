#include "UISystem.h"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <cmath>

#include "ClientPrediction.h"
#include "GameStateManager.h"
#include "ItemRegistry.h"
#include "Logger.h"
#include "NetworkClient.h"
#include "Player.h"
#include "Texture.h"
#include "TextureManager.h"
#include "Window.h"
#include "config/PlayerConfig.h"

UISystem::UISystem(Window* window, ClientPrediction* clientPrediction,
                   NetworkClient* client)
    : window(window),
      clientPrediction(clientPrediction),
      client(client),
      showSettings(false),
      currentTime(0.0f),
      titleScreenBackground(nullptr),
      titleMusic(nullptr) {
  // Load settings
  settings.load(Settings::DEFAULT_FILENAME);

  // Load title screen background
  titleScreenBackground =
      TextureManager::instance().get("assets/uis/blue_zone.png");
  if (!titleScreenBackground) {
    Logger::error("Failed to load title screen background");
  }

  // Load title screen music (SDL_mixer already initialized by MusicSystem)
  titleMusic = Mix_LoadMUS("assets/music/landing_screen.wav");
  if (!titleMusic) {
    Logger::error("Failed to load title screen music: " +
                  std::string(Mix_GetError()));
  }

  // Subscribe to state changes to control title music
  EventBus::instance().subscribe<GameStateChangedEvent>(
      [this](const GameStateChangedEvent& e) {
        // Start title music when entering TitleScreen
        if (e.newState == GameState::TitleScreen && titleMusic) {
          Mix_VolumeMusic(64);                   // 50% volume
          Mix_FadeInMusic(titleMusic, -1, 500);  // Loop forever, 500ms fade
          Logger::info("Started title screen music");
        }
        // Fade out title music when leaving TitleScreen
        else if (e.previousState == GameState::TitleScreen && titleMusic) {
          Mix_FadeOutMusic(500);  // 500ms fade out
          Logger::info("Faded out title screen music");
        }
      });

  // If we're already in TitleScreen state (transition happened before UISystem
  // was created), start the music now
  if (GameStateManager::instance().getCurrentState() ==
          GameState::TitleScreen &&
      titleMusic) {
    Mix_VolumeMusic(64);
    Mix_FadeInMusic(titleMusic, -1, 500);
    Logger::info("Started title screen music (initial state)");
  }

  EventBus::instance().subscribe<RenderEvent>(
      [this](const RenderEvent& e) { onRender(e); });

  EventBus::instance().subscribe<ItemPickedUpEvent>(
      [this](const ItemPickedUpEvent& e) { onItemPickedUp(e); });

  EventBus::instance().subscribe<UpdateEvent>([this](const UpdateEvent& e) {
    currentTime += e.deltaTime / 1000.0f;  // Convert ms to seconds

    // Remove old notifications (after 3 seconds)
    while (!notifications.empty() &&
           currentTime - notifications.front().timestamp > 3.0f) {
      notifications.pop_front();
    }
  });

  Logger::info("UISystem initialized");
}

UISystem::~UISystem() {
  if (titleMusic) {
    Mix_FreeMusic(titleMusic);
  }
}

void UISystem::onRender(const RenderEvent& e) { render(); }

void UISystem::render() {
  // Start ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  // Render UI based on game state
  GameState currentState = GameStateManager::instance().getCurrentState();

  switch (currentState) {
    case GameState::TitleScreen:
      renderTitleScreen();
      break;
    case GameState::MainMenu:
      renderMainMenu();
      if (showSettings) {
        renderSettingsPanel();
      }
      break;
    case GameState::Playing:
      renderHUD();
      break;
    case GameState::Paused:
      renderHUD();
      renderPauseMenu();
      if (showSettings) {
        renderSettingsPanel();
      }
      break;
    case GameState::Inventory:
      renderInventory();
      break;
  }

  // Render pickup notifications (always on top, except in main menu)
  if (currentState != GameState::MainMenu) {
    renderNotifications();
  }

  // Render ImGui
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UISystem::renderTitleScreen() {
  ImVec2 windowSize = ImGui::GetIO().DisplaySize;

  // Fullscreen background image
  if (titleScreenBackground) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(windowSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##TitleBackground", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);

    ImGui::Image((ImTextureID)(intptr_t)titleScreenBackground->getID(),
                 windowSize);

    ImGui::End();
    ImGui::PopStyleVar();
  }

  // "Press any key to continue" prompt at bottom
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  float yPos = windowSize.y * 0.92f;
  ImGui::SetNextWindowPos(ImVec2(center.x, yPos), ImGuiCond_Always,
                          ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowBgAlpha(0.0f);

  ImGui::Begin("##TitlePrompt", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

  // Pulsing text effect
  float alpha = 0.6f + 0.4f * sinf(currentTime * 3.0f);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, alpha));
  ImGui::Text("Press any key to continue");
  ImGui::PopStyleColor();

  ImGui::End();
}

void UISystem::renderMainMenu() {
  // Center the menu window
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_Always);

  ImGui::Begin("Gambit", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse);

  ImGui::Text("Main Menu");
  ImGui::Separator();
  ImGui::Spacing();

  // Start Game button
  if (ImGui::Button("Start Game", ImVec2(-1, 40))) {
    GameStateManager::instance().transitionTo(GameState::Playing);
  }

  ImGui::Spacing();

  // Settings button
  if (ImGui::Button("Settings", ImVec2(-1, 40))) {
    tempSettings = settings;
    showSettings = true;
  }

  ImGui::Spacing();

  // Quit button
  if (ImGui::Button("Quit", ImVec2(-1, 40))) {
    window->close();
  }

  ImGui::End();
}

void UISystem::renderSettingsPanel() {
  // Center the settings window
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_Always);

  ImGui::Begin("Settings", &showSettings,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse);

  // Audio Settings
  if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::SliderFloat("Master Volume", &tempSettings.masterVolume, 0.0f, 1.0f);
    ImGui::SliderFloat("Music Volume", &tempSettings.musicVolume, 0.0f, 1.0f);
    ImGui::SliderFloat("SFX Volume", &tempSettings.sfxVolume, 0.0f, 1.0f);
    ImGui::Checkbox("Muted", &tempSettings.muted);
  }

  // Graphics Settings
  if (ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::InputInt("Window Width", &tempSettings.windowWidth);
    ImGui::InputInt("Window Height", &tempSettings.windowHeight);
    ImGui::Checkbox("Fullscreen", &tempSettings.fullscreen);
    ImGui::Checkbox("VSync", &tempSettings.vsync);
  }

  // Controls Settings
  if (ImGui::CollapsingHeader("Controls")) {
    ImGui::Text("WASD / Arrow Keys - Movement");
    ImGui::Text("Space - Attack");
    ImGui::Text("ESC - Pause");
    ImGui::Text("I - Inventory");
    ImGui::Text("M / F2 - Toggle Mute");
    ImGui::Text("F1 - Toggle Debug Visualizations");
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Save/Cancel buttons
  if (ImGui::Button("Save", ImVec2(240, 40))) {
    settings = tempSettings;
    settings.save(Settings::DEFAULT_FILENAME);
    showSettings = false;
  }

  ImGui::SameLine();

  if (ImGui::Button("Cancel", ImVec2(240, 40))) {
    showSettings = false;
  }

  ImGui::End();
}

void UISystem::renderHUD() {
  const Player& localPlayer = clientPrediction->getLocalPlayer();

  // Health bar in top-left corner
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(200, 60), ImGuiCond_Always);

  ImGui::Begin("HUD", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

  ImGui::Text("Health");

  // Calculate health percentage
  float healthPercent = localPlayer.health / 100.0f;

  // Color based on health
  ImVec4 barColor;
  if (healthPercent > 0.6f) {
    barColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green
  } else if (healthPercent > 0.3f) {
    barColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow
  } else {
    barColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red
  }

  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
  ImGui::ProgressBar(healthPercent, ImVec2(-1, 0));
  ImGui::PopStyleColor();

  ImGui::End();
}

void UISystem::renderPauseMenu() {
  // Semi-transparent black overlay
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
  ImGui::Begin("Overlay", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
  ImGui::End();
  ImGui::PopStyleColor();

  // Centered pause menu
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_Always);

  ImGui::Begin("Paused", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse);

  ImGui::Text("Game Paused");
  ImGui::Separator();
  ImGui::Spacing();

  // Resume button (ESC)
  if (ImGui::Button("Resume (ESC)", ImVec2(-1, 40))) {
    GameStateManager::instance().transitionTo(GameState::Playing);
  }

  ImGui::Spacing();

  // Settings button
  if (ImGui::Button("Settings", ImVec2(-1, 40))) {
    tempSettings = settings;
    showSettings = true;
  }

  ImGui::Spacing();

  // Main Menu button
  if (ImGui::Button("Main Menu", ImVec2(-1, 40))) {
    GameStateManager::instance().transitionTo(GameState::MainMenu);
  }

  ImGui::Spacing();

  // Quit button
  if (ImGui::Button("Quit", ImVec2(-1, 40))) {
    window->close();
  }

  ImGui::End();
}

void UISystem::renderInventory() {
  const Player& localPlayer = clientPrediction->getLocalPlayer();
  const ItemRegistry& registry = ItemRegistry::instance();

  // Fullscreen inventory window
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_Always);

  ImGui::Begin("Inventory (Press I to close)", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse);

  // Layout: Left side = inventory grid, Right side = equipment + stats
  ImGui::Columns(2, "InventoryColumns", true);
  ImGui::SetColumnWidth(0, 550);

  // === LEFT COLUMN: Inventory Grid ===
  ImGui::Text("Inventory (20 slots)");
  ImGui::Separator();
  ImGui::Spacing();

  const float slotSize = 100.0f;
  const int columns = 5;
  const int rows = 4;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < columns; col++) {
      int slotIndex = row * columns + col;
      const ItemStack& stack = localPlayer.inventory[slotIndex];

      if (col > 0) {
        ImGui::SameLine();
      }

      ImGui::PushID(slotIndex);

      // Draw slot button
      if (stack.isEmpty()) {
        ImGui::Button("Empty", ImVec2(slotSize, slotSize));
      } else {
        const ItemDefinition* item = registry.getItem(stack.itemId);
        if (item) {
          std::string label =
              item->name + "\nx" + std::to_string(stack.quantity);
          bool clicked =
              ImGui::Button(label.c_str(), ImVec2(slotSize, slotSize));

          // Handle click-to-equip
          if (clicked) {
            handleInventorySlotClick(slotIndex, *item);
          }

          // Handle double-click-to-use for consumables
          if (item->type == ItemType::Consumable && ImGui::IsItemHovered() &&
              ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            handleConsumableUse(slotIndex, *item);
          }

          // Tooltip on hover
          if (ImGui::IsItemHovered()) {
            ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
            ImGui::BeginTooltip();
            ImGui::Text("%s", item->name.c_str());

            // Rarity color
            const char* rarityName = "Common";
            ImVec4 rarityColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
            switch (item->rarity) {
              case ItemRarity::Uncommon:
                rarityName = "Uncommon";
                rarityColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
                break;
              case ItemRarity::Rare:
                rarityName = "Rare";
                rarityColor = ImVec4(0.0f, 0.5f, 1.0f, 1.0f);
                break;
              case ItemRarity::Epic:
                rarityName = "Epic";
                rarityColor = ImVec4(0.6f, 0.0f, 1.0f, 1.0f);
                break;
              case ItemRarity::Legendary:
                rarityName = "Legendary";
                rarityColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
                break;
              default:
                break;
            }
            ImGui::TextColored(rarityColor, "%s", rarityName);

            ImGui::Separator();
            ImGui::TextWrapped("%s", item->description.c_str());

            // Stats
            if (item->damage > 0) {
              ImGui::Text("Damage: +%d", item->damage);
            }
            if (item->armor > 0) {
              ImGui::Text("Armor: +%d", item->armor);
            }
            if (item->healthBonus > 0) {
              ImGui::Text("Health: +%d", item->healthBonus);
            }
            if (item->healAmount > 0) {
              ImGui::Text("Heals: %d HP", item->healAmount);
            }

            ImGui::EndTooltip();
          }
        } else {
          ImGui::Button("Unknown", ImVec2(slotSize, slotSize));
        }
      }

      ImGui::PopID();
    }
  }

  // === RIGHT COLUMN: Equipment + Stats ===
  ImGui::NextColumn();

  ImGui::Text("Equipment");
  ImGui::Separator();
  ImGui::Spacing();

  // Weapon slot
  ImGui::Text("Weapon:");
  const ItemStack& weaponSlot = localPlayer.equipment[EQUIPMENT_WEAPON_SLOT];
  if (weaponSlot.isEmpty()) {
    ImGui::Button("No Weapon", ImVec2(200, 80));
  } else {
    const ItemDefinition* weapon = registry.getItem(weaponSlot.itemId);
    if (weapon) {
      bool clicked = ImGui::Button(weapon->name.c_str(), ImVec2(200, 80));
      if (clicked) {
        handleEquipmentSlotClick(EQUIPMENT_WEAPON_SLOT);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_Always);
        ImGui::BeginTooltip();
        ImGui::Text("%s", weapon->name.c_str());
        ImGui::Text("Damage: +%d", weapon->damage);
        ImGui::EndTooltip();
      }
    }
  }

  ImGui::Spacing();

  // Armor slot
  ImGui::Text("Armor:");
  const ItemStack& armorSlot = localPlayer.equipment[EQUIPMENT_ARMOR_SLOT];
  if (armorSlot.isEmpty()) {
    ImGui::Button("No Armor", ImVec2(200, 80));
  } else {
    const ItemDefinition* armor = registry.getItem(armorSlot.itemId);
    if (armor) {
      bool clicked = ImGui::Button(armor->name.c_str(), ImVec2(200, 80));
      if (clicked) {
        handleEquipmentSlotClick(EQUIPMENT_ARMOR_SLOT);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_Always);
        ImGui::BeginTooltip();
        ImGui::Text("%s", armor->name.c_str());
        ImGui::Text("Armor: +%d", armor->armor);
        ImGui::Text("Health: +%d", armor->healthBonus);
        ImGui::EndTooltip();
      }
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Stats display
  ImGui::Text("Stats");
  ImGui::Separator();

  // Calculate total stats with equipment bonuses
  int totalDamage = 0;
  int totalArmor = 0;
  int totalHealthBonus = 0;

  if (!weaponSlot.isEmpty()) {
    const ItemDefinition* weapon = registry.getItem(weaponSlot.itemId);
    if (weapon) {
      totalDamage += weapon->damage;
    }
  }

  if (!armorSlot.isEmpty()) {
    const ItemDefinition* armor = registry.getItem(armorSlot.itemId);
    if (armor) {
      totalArmor += armor->armor;
      totalHealthBonus += armor->healthBonus;
    }
  }

  ImGui::Text("Damage: %d", totalDamage);
  ImGui::Text("Armor: %d", totalArmor);
  ImGui::Text("Max Health: %.0f (+%d)", localPlayer.health, totalHealthBonus);

  ImGui::Columns(1);

  ImGui::End();
}

// UI interaction handlers

void UISystem::handleInventorySlotClick(int slotIndex,
                                        const ItemDefinition& item) {
  uint8_t equipmentSlot = 255;

  if (item.type == ItemType::Weapon) {
    equipmentSlot = EQUIPMENT_WEAPON_SLOT;
  } else if (item.type == ItemType::Armor) {
    equipmentSlot = EQUIPMENT_ARMOR_SLOT;
  } else if (item.type == ItemType::Consumable) {
    Logger::info("Double-click consumable items to use them");
    return;
  }

  EquipItemPacket packet;
  packet.inventorySlot = static_cast<uint8_t>(slotIndex);
  packet.equipmentSlot = equipmentSlot;

  client->send(serialize(packet));

  Logger::info("Equipping " + item.name);
}

void UISystem::handleEquipmentSlotClick(int equipmentSlotIndex) {
  const Player& localPlayer = clientPrediction->getLocalPlayer();
  const ItemStack& equipSlot = localPlayer.equipment[equipmentSlotIndex];

  if (equipSlot.isEmpty()) {
    return;
  }

  int emptySlot = localPlayer.findEmptySlot();
  if (emptySlot == -1) {
    Logger::info("Inventory full, cannot unequip item");
    return;
  }

  EquipItemPacket packet;
  packet.inventorySlot = static_cast<uint8_t>(equipmentSlotIndex);
  packet.equipmentSlot = 255;  // Unequip mode

  client->send(serialize(packet));

  const ItemRegistry& registry = ItemRegistry::instance();
  const ItemDefinition* item = registry.getItem(equipSlot.itemId);
  if (item) {
    Logger::info("Unequipping " + item->name);
  }
}

void UISystem::handleConsumableUse(int slotIndex, const ItemDefinition& item) {
  // Check if healing item and already at max health
  const Player& localPlayer = clientPrediction->getLocalPlayer();
  if (item.healAmount > 0 && localPlayer.health >= Config::Player::MAX_HEALTH) {
    // Show notification that you're already at full health
    Notification notification;
    notification.message = "Already at full health!";
    notification.timestamp = currentTime;
    notifications.push_back(notification);

    Logger::info("Cannot use " + item.name + " - already at full health");
    return;
  }

  UseItemPacket packet;
  packet.slotIndex = static_cast<uint8_t>(slotIndex);

  client->send(serialize(packet));

  Logger::info("Using " + item.name);
}

void UISystem::onItemPickedUp(const ItemPickedUpEvent& e) {
  const ItemRegistry& registry = ItemRegistry::instance();
  const ItemDefinition* item = registry.getItem(e.itemId);

  if (item) {
    std::string message = "+ " + item->name;
    if (e.quantity > 1) {
      message += " x" + std::to_string(e.quantity);
    }

    Notification notification;
    notification.message = message;
    notification.timestamp = currentTime;
    notifications.push_back(notification);

    // Keep only last 5 notifications
    while (notifications.size() > 5) {
      notifications.pop_front();
    }
  }
}

void UISystem::renderNotifications() {
  if (notifications.empty()) {
    return;
  }

  // Render notifications in bottom-right corner
  ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 10,
                                 ImGui::GetIO().DisplaySize.y - 10),
                          ImGuiCond_Always, ImVec2(1.0f, 1.0f));
  ImGui::SetNextWindowBgAlpha(0.7f);

  ImGui::Begin("Notifications", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoInputs |
                   ImGuiWindowFlags_AlwaysAutoResize);

  // Render notifications from oldest to newest
  for (const auto& notification : notifications) {
    float age = currentTime - notification.timestamp;
    float alpha = 1.0f;

    // Fade out in last 0.5 seconds
    if (age > 2.5f) {
      alpha = 1.0f - ((age - 2.5f) / 0.5f);
    }

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.6f, alpha), "%s",
                       notification.message.c_str());
  }

  ImGui::End();
}
