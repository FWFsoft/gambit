#include "UISystem.h"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <cmath>

#include "CharacterRegistry.h"
#include "CharacterSelectionState.h"
#include "ClientPrediction.h"
#include "DamageNumberSystem.h"
#include "Effect.h"
#include "EffectTracker.h"
#include "GameStateManager.h"
#include "ItemRegistry.h"
#include "Logger.h"
#include "MapSelectionState.h"
#include "NetworkClient.h"
#include "Player.h"
#include "Texture.h"
#include "TextureManager.h"
#include "Window.h"
#include "config/GameplayConfig.h"
#include "config/PlayerConfig.h"

UISystem::UISystem(Window* window, ClientPrediction* clientPrediction,
                   NetworkClient* client, DamageNumberSystem* damageNumbers,
                   EffectTracker* effectTracker)
    : window(window),
      clientPrediction(clientPrediction),
      client(client),
      damageNumberSystem(damageNumbers),
      effectTracker(effectTracker),
      showSettings(false),
      currentTime(0.0f),
      titleScreenBackground(nullptr),
      titleMusic(nullptr),
      hoveredCharacterId(0),
      selectedCharacterId(0),
      selectionAnimationTime(0.0f),
      keyboardFocusedIndex(-1),
      showCoordinates(false) {
  // Load settings
  settings.load(Settings::DEFAULT_FILENAME);

  // Subscribe to keyboard events for navigation
  EventBus::instance().subscribe<KeyDownEvent>(
      [this](const KeyDownEvent& e) { onKeyDown(e); });

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

  EventBus::instance().subscribe<ObjectiveUpdatedEvent>(
      [this](const ObjectiveUpdatedEvent& e) { onObjectiveUpdated(e); });

  EventBus::instance().subscribe<UpdateEvent>([this](const UpdateEvent& e) {
    currentTime += e.deltaTime / 1000.0f;  // Convert ms to seconds

    // Remove old notifications (after 3 seconds)
    while (!notifications.empty() &&
           currentTime - notifications.front().timestamp > 3.0f) {
      notifications.pop_front();
    }

    // Remove old objective notifications (after 5 seconds)
    while (!objectiveNotifications.empty() &&
           currentTime - objectiveNotifications.front().timestamp > 5.0f) {
      objectiveNotifications.pop_front();
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
    case GameState::CharacterSelect:
      renderCharacterSelect();
      break;
    case GameState::MainMenu:
      renderMainMenu();
      if (showSettings) {
        renderSettingsPanel();
      }
      break;
    case GameState::Playing:
      renderHUD();
      // Show death screen overlay if player is dead
      if (clientPrediction && clientPrediction->getLocalPlayer().isDead()) {
        renderDeathScreen();
      }
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
    renderObjectiveActivity();
  }

  // Render effect bars (only during gameplay)
  if (currentState == GameState::Playing) {
    renderEffectBars();
  }

  // Render damage numbers (only during gameplay)
  if (currentState == GameState::Playing && damageNumberSystem) {
    damageNumberSystem->render();
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

  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin("##TitlePrompt", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

  // Pulsing text effect
  float alpha = 0.6f + 0.4f * sinf(currentTime * 3.0f);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, alpha));
  ImGui::Text("Press any key to continue");
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();

  ImGui::End();

  // Settings and Quit buttons in bottom-right corner
  ImGui::SetNextWindowPos(ImVec2(windowSize.x - 20, windowSize.y - 20),
                          ImGuiCond_Always, ImVec2(1.0f, 1.0f));
  ImGui::SetNextWindowBgAlpha(0.5f);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin("##TitleButtons", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

  if (ImGui::Button("Settings", ImVec2(100, 30))) {
    showSettings = true;
  }

  ImGui::SameLine();

  if (ImGui::Button("Quit", ImVec2(100, 30))) {
    window->close();
  }

  ImGui::End();
  ImGui::PopStyleVar();

  // Render settings panel if open
  if (showSettings) {
    renderSettingsPanel();
  }
}

void UISystem::renderCharacterSelect() {
  // Update animation time
  selectionAnimationTime = currentTime;

  ImVec2 windowSize = ImGui::GetIO().DisplaySize;
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();

  // Title at top
  ImGui::SetNextWindowPos(ImVec2(center.x, 30), ImGuiCond_Always,
                          ImVec2(0.5f, 0.0f));
  ImGui::SetNextWindowBgAlpha(0.0f);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::Begin("##CharacterSelectTitle", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);

  ImGui::SetWindowFontScale(2.5f);
  ImGui::Text("Select Your Character");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::End();
  ImGui::PopStyleVar();

  // TOP-LEFT: Character grid
  const CharacterRegistry& registry = CharacterRegistry::instance();
  const auto& characters = registry.getAllCharacters();

  // Grid layout: 5 columns x 4 rows for 19 characters
  const int columns = 5;
  const int rows = 4;
  const float portraitSize = 120.0f;
  const float padding = 20.0f;
  const float gridWidth = columns * (portraitSize + padding);
  const float gridHeight = rows * (portraitSize + padding);

  ImGui::SetNextWindowPos(ImVec2(50, 100), ImGuiCond_Always,
                          ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(gridWidth + 40, gridHeight + 40),
                           ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin("Character Grid", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);

  hoveredCharacterId = 0;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < columns; col++) {
      int index = row * columns + col;
      if (index >= static_cast<int>(characters.size())) break;

      const CharacterDefinition& character = characters[index];

      if (col > 0) {
        ImGui::SameLine();
      }

      ImGui::PushID(character.id);

      bool isSelected = (selectedCharacterId == character.id);
      bool isFocused = (keyboardFocusedIndex == index);

      // Calculate display size with selection animation (doesn't affect layout)
      float displaySize = portraitSize;
      if (isSelected) {
        // Grow by 10% with subtle pulse
        float pulse = 1.0f + 0.05f * sinf(selectionAnimationTime * 4.0f);
        displaySize = portraitSize * 1.1f * pulse;
      }

      // Try to load portrait texture
      Texture* portrait =
          TextureManager::instance().get(character.getCharacterPortraitPath());

      // If portrait exists, use it as image button
      if (portrait && portrait->isLoaded()) {
        // Create invisible button for click detection (fixed size for layout)
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton(
            "##portrait", ImVec2(portraitSize, portraitSize));

        // Calculate centered position for scaled portrait
        float offset = (displaySize - portraitSize) / 2.0f;
        ImVec2 topLeft = ImVec2(cursorPos.x - offset, cursorPos.y - offset);
        ImVec2 bottomRight =
            ImVec2(topLeft.x + displaySize, topLeft.y + displaySize);

        // Draw texture centered and scaled (doesn't affect layout)
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddImage((ImTextureID)(intptr_t)portrait->getID(), topLeft,
                           bottomRight);

        // Draw keyboard focus border (yellow)
        if (isFocused) {
          drawList->AddRect(topLeft, bottomRight, IM_COL32(255, 255, 0, 255),
                            0.0f, 0, 3.0f);
        }

        if (clicked) {
          selectedCharacterId = character.id;
          keyboardFocusedIndex = index;  // Sync keyboard focus with click
          Logger::info("Selected character: " + character.name);
        }

        if (ImGui::IsItemHovered()) {
          hoveredCharacterId = character.id;
        }

      } else {
        // Fallback: colored button with character name
        ImGui::PushStyleColor(ImGuiCol_Button,
                              ImVec4(character.r / 255.0f, character.g / 255.0f,
                                     character.b / 255.0f, 1.0f));

        bool clicked = ImGui::Button(character.name.c_str(),
                                     ImVec2(portraitSize, portraitSize));

        ImGui::PopStyleColor();

        if (clicked) {
          selectedCharacterId = character.id;
          Logger::info("Selected character: " + character.name);
        }

        if (ImGui::IsItemHovered()) {
          hoveredCharacterId = character.id;
        }
      }

      // Show character name tooltip on hover
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", character.name.c_str());
        // Future: Add stats preview here
        ImGui::EndTooltip();
      }

      ImGui::PopID();
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();

  // CENTER-RIGHT: Map selection (positioned to the right of character grid)
  const float mapSelectorX = 50 + gridWidth + 80;  // Right of grid with gap
  const float mapSelectorY = 300;                  // Middle of screen
  ImGui::SetNextWindowPos(ImVec2(mapSelectorX, mapSelectorY), ImGuiCond_Always,
                          ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(280, 150), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin("Map Selection", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);

  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("Select Map");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::Separator();
  ImGui::Spacing();

  // List of available maps
  struct MapInfo {
    std::string name;
    std::string path;
  };
  static const MapInfo maps[] = {
      {"Test Map 1", "assets/maps/test_map.tmx"},
      {"Test Map 2", "assets/maps/test_map2.tmx"},
  };

  std::string currentMapPath =
      MapSelectionState::instance().getSelectedMapPath();

  for (size_t i = 0; i < sizeof(maps) / sizeof(maps[0]); i++) {
    bool isSelected = (currentMapPath == maps[i].path);

    if (isSelected) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    }

    if (ImGui::Button(maps[i].name.c_str(), ImVec2(250, 40))) {
      MapSelectionState::instance().setSelectedMap(maps[i].path);
      Logger::info("Selected map: " + maps[i].name);
    }

    if (isSelected) {
      ImGui::PopStyleColor();
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();

  // BOTTOM-RIGHT: Character preview
  ImGui::SetNextWindowPos(ImVec2(windowSize.x - 50, windowSize.y - 130),
                          ImGuiCond_Always, ImVec2(1.0f, 1.0f));
  ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin("Character Preview", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoCollapse);

  if (selectedCharacterId != 0) {
    const CharacterDefinition* character =
        registry.getCharacter(selectedCharacterId);

    if (character) {
      // Display character name
      ImGui::SetWindowFontScale(1.5f);
      ImGui::Text("%s", character->name.c_str());
      ImGui::SetWindowFontScale(1.0f);

      ImGui::Separator();
      ImGui::Spacing();

      // Display character using relevant character image asset
      Texture* playerTexture =
          TextureManager::instance().get(character->getCharacterPreviewPath());
      if (playerTexture && playerTexture->isLoaded()) {
        // Calculate preview size (3x game size = 96x96)
        float previewSize = 250.0f;
        float previewSizeY = previewSize * 1.44;

        // Center the preview in the window
        ImVec2 cursorPos = ImGui::GetCursorPos();
        float centerX = (350 - previewSize) / 2.0f;
        ImGui::SetCursorPos(ImVec2(centerX, cursorPos.y));

        ImGui::Image((ImTextureID)(intptr_t)playerTexture->getID(),
                     ImVec2(previewSize, previewSizeY));
      }

      // Future: Show character stats here
    }
  } else {
    // No character selected
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextWrapped("Select a character from the grid");
    ImGui::SetWindowFontScale(1.0f);
  }

  ImGui::End();
  ImGui::PopStyleVar();

  // BOTTOM CENTER: Confirm button
  ImGui::SetNextWindowPos(ImVec2(windowSize.x / 2, windowSize.y - 50),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowBgAlpha(0.8f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin("##ConfirmSelection", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

  if (selectedCharacterId != 0) {
    if (ImGui::Button("Confirm Selection", ImVec2(300, 50))) {
      // Save selection to state
      CharacterSelectionState::instance().setSelectedCharacter(
          selectedCharacterId);

      // Transition to Playing state (skip MainMenu as per user request)
      GameStateManager::instance().transitionTo(GameState::Playing);
    }
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::Button("Select a Character", ImVec2(300, 50));
    ImGui::PopStyleColor(2);
  }

  ImGui::End();
  ImGui::PopStyleVar();
}

void UISystem::renderMainMenu() {
  // Center the menu window
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_Always);

  ImGui::Begin("Blue Zone", nullptr,
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
    ImGui::Text("F1 - Toggle Coordinates & Debug Visualizations");
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

  // HUD with portrait in top-left corner
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(280, 80), ImGuiCond_Always);

  ImGui::Begin("HUD", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

  // Get selected character portrait
  if (CharacterSelectionState::instance().hasSelection()) {
    uint32_t selectedId =
        CharacterSelectionState::instance().getSelectedCharacterId();
    const CharacterDefinition* character =
        CharacterRegistry::instance().getCharacter(selectedId);

    if (character) {
      Texture* portrait =
          TextureManager::instance().get(character->getCharacterPortraitPath());

      if (portrait && portrait->isLoaded()) {
        // Draw portrait on the left (60x60)
        ImGui::Image((ImTextureID)(intptr_t)portrait->getID(), ImVec2(60, 60));
        ImGui::SameLine();
      }
    }
  }

  // Health bar on the right
  ImGui::BeginGroup();
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
  ImGui::ProgressBar(healthPercent, ImVec2(200, 0));
  ImGui::PopStyleColor();

  ImGui::EndGroup();

  ImGui::End();

  // Debug coordinate display (toggled with F1)
  if (showCoordinates) {
    ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.8f);

    ImGui::Begin("Coordinates", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Position:");
    ImGui::Text("X: %.1f", localPlayer.x);
    ImGui::Text("Y: %.1f", localPlayer.y);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::End();
  }
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

  // Main Menu button (returns to character selection)
  if (ImGui::Button("Main Menu", ImVec2(-1, 40))) {
    // Reset character selection state
    CharacterSelectionState::instance().reset();
    clientPrediction->resetCharacterSelection();

    // Return to character selection screen
    GameStateManager::instance().transitionTo(GameState::CharacterSelect);
  }

  ImGui::Spacing();

  // Quit button
  if (ImGui::Button("Quit", ImVec2(-1, 40))) {
    window->close();
  }

  ImGui::End();
}

void UISystem::renderDeathScreen() {
  // Dark red overlay (darker and more dramatic than pause)
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.2f, 0.0f, 0.0f, 0.7f));

  ImGui::Begin("DeathOverlay", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
  ImGui::End();
  ImGui::PopStyleColor();

  // Centered death message
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowBgAlpha(0.0f);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

  ImGui::Begin("DeathMessage", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

  // Calculate respawn time remaining
  const Player& localPlayer = clientPrediction->getLocalPlayer();
  float deathDuration = currentTime - localPlayer.deathTime / 1000.0f;
  float respawnDelay = Config::Gameplay::PLAYER_RESPAWN_DELAY / 1000.0f;
  float timeRemaining = std::max(0.0f, respawnDelay - deathDuration);

  // Pulsing "YOU DIED" text in red
  float pulseAlpha = 0.7f + 0.3f * sinf(currentTime * 4.0f);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, pulseAlpha));

  // Large font for dramatic effect
  ImGui::SetWindowFontScale(3.0f);
  ImGui::Text("YOU DIED");
  ImGui::SetWindowFontScale(1.0f);

  ImGui::PopStyleColor();
  ImGui::PopStyleVar();

  ImGui::Spacing();
  ImGui::Spacing();

  // Respawn countdown in white
  if (timeRemaining > 0.0f) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
    ImGui::Text("Respawning in %.1f...", timeRemaining);
    ImGui::PopStyleColor();
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 0.9f));
    ImGui::Text("Respawning...");
    ImGui::PopStyleColor();
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

void UISystem::renderEffectBars() {
  if (!effectTracker) {
    return;
  }

  // Get nearby enemies from enemy interpolation
  // For now, just show effects in top-right corner for all enemies with effects
  ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 210, 10),
                          ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(200, 0), ImGuiCond_Always);

  ImGui::Begin("Active Effects", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::Text("Enemy Effects:");
  ImGui::Separator();

  // For now, we'll iterate over all possible enemy IDs (1-100)
  // In a real implementation, you'd get the list from EnemyInterpolation
  bool foundAny = false;
  for (uint32_t enemyId = 1; enemyId < 100; ++enemyId) {
    const auto& effects = effectTracker->getEffects(enemyId, true);
    if (effects.empty()) {
      continue;
    }

    foundAny = true;
    ImGui::Text("Enemy %u:", enemyId);
    ImGui::Indent();

    for (const auto& effect : effects) {
      const EffectDefinition& def = EffectRegistry::get(effect.type);

      // Color based on effect category
      ImVec4 color;
      if (def.category == EffectCategory::Buff) {
        color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);  // Green for buffs
      } else if (def.category == EffectCategory::Debuff) {
        color = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);  // Red for debuffs
      } else {
        color = ImVec4(0.7f, 0.7f, 0.2f, 1.0f);  // Yellow for neutral
      }

      // Create colored box with effect name and stacks
      ImGui::PushStyleColor(ImGuiCol_Button, color);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);

      char label[64];
      if (effect.stacks > 1) {
        snprintf(label, sizeof(label), "%s x%d", def.name, effect.stacks);
      } else {
        snprintf(label, sizeof(label), "%s", def.name);
      }

      ImGui::Button(label, ImVec2(180, 22));
      ImGui::PopStyleColor(3);

      // Show duration on same line
      ImGui::SameLine();
      float durationSec = effect.remainingDuration / 1000.0f;
      ImGui::Text("%.1fs", durationSec);
    }

    ImGui::Unindent();
    ImGui::Spacing();
  }

  if (!foundAny) {
    ImGui::TextDisabled("No active effects");
  }

  ImGui::End();
}

void UISystem::onObjectiveUpdated(const ObjectiveUpdatedEvent& e) {
  // Check if we already have a notification for this objective
  for (auto& notification : objectiveNotifications) {
    if (notification.objectiveId == e.objectiveId) {
      // Update existing notification
      notification.state = e.state;
      notification.progress = e.progress;
      notification.name = e.name;
      notification.timestamp = currentTime;
      return;
    }
  }

  // Create new notification
  ObjectiveNotification notification;
  notification.objectiveId = e.objectiveId;
  notification.name = e.name;
  notification.state = e.state;
  notification.progress = e.progress;
  notification.timestamp = currentTime;
  objectiveNotifications.push_back(notification);

  // Keep only last 5 objective notifications
  while (objectiveNotifications.size() > 5) {
    objectiveNotifications.pop_front();
  }

  Logger::info("Objective updated: " + e.name +
               " - State: " + std::to_string(e.state) +
               " Progress: " + std::to_string(e.progress));
}

void UISystem::renderObjectiveActivity() {
  if (objectiveNotifications.empty()) {
    return;
  }

  // Render objective notifications in top-left corner below HUD
  ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.8f);

  ImGui::Begin("Objectives", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_AlwaysAutoResize);

  for (const auto& notification : objectiveNotifications) {
    float age = currentTime - notification.timestamp;
    float alpha = 1.0f;

    // Fade out in last 1 second
    if (age > 4.0f) {
      alpha = 1.0f - ((age - 4.0f) / 1.0f);
    }

    // Color based on state: Yellow=Inactive, Blue=InProgress, Green=Completed
    ImVec4 color;
    const char* stateText;
    switch (notification.state) {
      case 0:                                     // Inactive
        color = ImVec4(1.0f, 1.0f, 0.2f, alpha);  // Yellow
        stateText = "Available";
        break;
      case 1:                                     // InProgress
        color = ImVec4(0.2f, 0.6f, 1.0f, alpha);  // Blue
        stateText = "In Progress";
        break;
      case 2:                                     // Completed
        color = ImVec4(0.2f, 1.0f, 0.2f, alpha);  // Green
        stateText = "Completed!";
        break;
      default:
        color = ImVec4(0.8f, 0.8f, 0.8f, alpha);  // Gray
        stateText = "Unknown";
        break;
    }

    ImGui::TextColored(color, "%s", notification.name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, alpha), "- %s", stateText);

    // Show progress bar for in-progress objectives
    if (notification.state == 1) {  // InProgress
      ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                            ImVec4(0.2f, 0.6f, 1.0f, alpha));
      ImGui::ProgressBar(notification.progress, ImVec2(200, 0));
      ImGui::PopStyleColor();
    }
  }

  ImGui::End();
}

void UISystem::onKeyDown(const KeyDownEvent& e) {
  GameState currentState = GameStateManager::instance().getCurrentState();

  // F1: Toggle coordinate display (during gameplay)
  if (e.key == SDLK_F1 && (currentState == GameState::Playing ||
                           currentState == GameState::Paused)) {
    showCoordinates = !showCoordinates;
    Logger::info("Coordinate display: " +
                 std::string(showCoordinates ? "ON" : "OFF"));
    return;
  }

  // Only handle keyboard navigation in CharacterSelect state
  if (currentState != GameState::CharacterSelect) {
    return;
  }

  const CharacterRegistry& registry = CharacterRegistry::instance();
  const auto& characters = registry.getAllCharacters();
  int totalCharacters = static_cast<int>(characters.size());

  if (totalCharacters == 0) {
    return;
  }

  const int columns = 5;

  // Initialize focus to first character if not set
  if (keyboardFocusedIndex == -1) {
    keyboardFocusedIndex = 0;
  }

  // Handle arrow key navigation
  switch (e.key) {
    case SDLK_LEFT:
      if (keyboardFocusedIndex > 0) {
        keyboardFocusedIndex--;
      }
      break;

    case SDLK_RIGHT:
      if (keyboardFocusedIndex < totalCharacters - 1) {
        keyboardFocusedIndex++;
      }
      break;

    case SDLK_UP:
      if (keyboardFocusedIndex >= columns) {
        keyboardFocusedIndex -= columns;
      }
      break;

    case SDLK_DOWN:
      if (keyboardFocusedIndex + columns < totalCharacters) {
        keyboardFocusedIndex += columns;
      }
      break;

    case SDLK_SPACE:
      // Space: Select the focused character
      if (keyboardFocusedIndex >= 0 && keyboardFocusedIndex < totalCharacters) {
        const CharacterDefinition& character = characters[keyboardFocusedIndex];
        selectedCharacterId = character.id;
        Logger::info("Selected character via keyboard: " + character.name);
      }
      break;

    case SDLK_RETURN:
      // Enter: Confirm selection (or select and confirm if nothing selected)
      if (keyboardFocusedIndex >= 0 && keyboardFocusedIndex < totalCharacters) {
        // If nothing selected yet, select the focused character first
        if (selectedCharacterId == 0) {
          const CharacterDefinition& character =
              characters[keyboardFocusedIndex];
          selectedCharacterId = character.id;
          Logger::info("Selected character via keyboard: " + character.name);
        }

        // Now confirm the selection and enter game
        CharacterSelectionState::instance().setSelectedCharacter(
            selectedCharacterId);
        GameStateManager::instance().transitionTo(GameState::Playing);
        Logger::info("Confirmed character selection, entering game");
      }
      break;

    default:
      break;
  }

  // Ensure focused index is valid
  if (keyboardFocusedIndex < 0) {
    keyboardFocusedIndex = 0;
  }
  if (keyboardFocusedIndex >= totalCharacters) {
    keyboardFocusedIndex = totalCharacters - 1;
  }
}
