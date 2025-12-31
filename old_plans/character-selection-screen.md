# Character Selection Screen Implementation Plan

## Overview
Implement a character selection screen that appears after the title screen, allowing players to choose from 20 characters displayed in a grid layout. Characters are differentiated by color tints initially (cosmetic only), with architecture stubbed for future stats/abilities.

## User Requirements
- **Flow**: TitleScreen → CharacterSelect → Playing (MainMenu skipped)
- **Visual**: Parallelogram-shaped character portraits (user will provide pre-skewed images)
- **Selection Indicator**: Selected character's portrait grows ~10% with subtle pulse animation
- **Character Count**: 20 characters in 5x4 grid, all visible on one screen
- **Implementation**: Color tints of existing sprite as proof of concept
- **Selection**: Final once confirmed (no going back without restart)
- **Settings Access**: Add Settings/Quit buttons to TitleScreen

## Architecture Changes

### 1. Game State Flow Modification

**Current Flow:**
```
TitleScreen → MainMenu → Playing
```

**New Flow:**
```
TitleScreen → CharacterSelect → Playing
          ↓
       Settings/Quit (buttons added to TitleScreen)
```

### 2. Character Definition System

Create extensible character definition structure for 20 characters with color variants.

**New Files:**
- `include/CharacterDefinition.h` - Character data structure with stubbed stats/classes
- `include/CharacterRegistry.h` - Singleton registry for all characters
- `src/CharacterRegistry.cpp` - 20 character definitions with color tints

**CharacterDefinition Structure:**
```cpp
struct CharacterDefinition {
  uint32_t id;
  std::string name;
  std::string portraitPath;      // Pre-skewed parallelogram image
  std::string spriteSheetPath;   // "assets/player_animated.png" for now
  uint8_t r, g, b;               // Color tint (proof of concept)
  // CharacterStats baseStats;   // Stubbed for future
  // CharacterClass characterClass; // Stubbed for future
};
```

**20 Character Roster (Color Variants):**
1. Red Knight (255, 0, 0)
2. Green Mage (0, 255, 0)
3. Blue Rogue (0, 0, 255)
4. Yellow Cleric (255, 255, 0)
5. Cyan Ranger (0, 255, 255)
6. Magenta Warlock (255, 0, 255)
7. White Paladin (255, 255, 255)
8. Gray Assassin (128, 128, 128)
9. Orange Berserker (255, 165, 0)
10. Pink Bard (255, 192, 203)
11. Purple Necromancer (128, 0, 128)
12. Brown Monk (139, 69, 19)
13. Teal Druid (0, 128, 128)
14. Lime Shaman (0, 255, 128)
15. Navy Warrior (0, 0, 128)
16. Maroon Hunter (128, 0, 0)
17. Olive Scout (128, 128, 0)
18. Silver Sorcerer (192, 192, 192)
19. Gold Champion (255, 215, 0)
20. Black Reaper (64, 64, 64)

### 3. Character Selection State Management

**New Files:**
- `include/CharacterSelectionState.h` - Client-side selection state singleton
- `src/CharacterSelectionState.cpp` - Stores selected character ID

This keeps character selection data separate from Player struct, avoiding network protocol changes.

### 4. UI System Changes

**Modified Files:**
- `include/UISystem.h` - Add `renderCharacterSelect()` method and state fields
- `src/UISystem.cpp` - Implement character select screen with 5x4 grid

**UI Layout:**
- **Title**: "Select Your Character" at top (centered, large font)
- **Character Grid**: 5 columns × 4 rows, left side of screen
  - Each portrait: 120×120px base size (grows to 132px when selected)
  - 20px padding between portraits
  - Pre-skewed parallelogram images from user
  - Fallback: Colored buttons with character names if images missing
- **Character Preview**: Right side of screen
  - Shows selected character's sprite using idle animation frame
  - Uses actual sprite sheet with character's color tint applied
  - Character name displayed below sprite
  - Large preview (3-4x game size for visibility)
  - Placeholder text "Select a character" if none selected
- **Selection Effect**: 10% size increase + subtle pulse (sin wave @ 4Hz)
- **Hover Tooltip**: Character name on hover (future: stats preview)
- **Confirm Button**: Bottom center, 300×50px
  - Disabled until character selected
  - On click: Save selection → Transition to Playing

**Modified Files:**
- `src/UISystem.cpp` - Update `renderTitleScreen()` to add Settings/Quit buttons

### 5. Game State Enum Update

**Modified Files:**
- `include/GameState.h` - Add CharacterSelect between TitleScreen and MainMenu

```cpp
enum class GameState {
  TitleScreen,
  CharacterSelect,  // NEW
  MainMenu,         // Still exists but not in main flow
  Playing,
  Paused,
  Inventory
};
```

- `src/GameStateManager.cpp` - Update state names array

### 6. Input Handling

**Modified Files:**
- `src/InputSystem.cpp` - Update `onKeyDown()`:
  - TitleScreen: Any key → CharacterSelect (not MainMenu)
  - CharacterSelect: ESC → TitleScreen (restart flow)
  - Character selection clicks handled by ImGui

### 7. Applying Character Selection

**Modified Files:**
- `src/client_main.cpp` - After creating local player, apply character selection:

```cpp
// Apply character selection (color tint for now)
if (CharacterSelectionState::instance().hasSelection()) {
  uint32_t selectedId = CharacterSelectionState::instance().getSelectedCharacterId();
  const CharacterDefinition* character = CharacterRegistry::instance().getCharacter(selectedId);
  if (character) {
    localPlayer.r = character->r;
    localPlayer.g = character->g;
    localPlayer.b = character->b;
  }
}
```

**CRITICAL**: Preserve character selection colors when server sends PlayerJoinedPacket:

- `src/ClientPrediction.cpp` - Modify PlayerJoined handling to NOT override RGB if character selected
- `src/RemotePlayerInterpolation.cpp` - Same modification for local player color preservation

### 8. Asset Directory Structure

**New Directory:**
```
assets/portraits/
  red_knight.png       (Pre-skewed parallelogram, ~120×120px)
  green_mage.png
  blue_rogue.png
  ... (20 total)
```

User will provide these images. Images should be pre-rendered with parallelogram skew effect (ImGui doesn't support arbitrary transforms).

**Fallback**: If images don't exist, UI renders colored buttons with character names.

## Implementation Details

### Character Grid Rendering Logic

```cpp
void UISystem::renderCharacterSelect() {
  const int columns = 5;
  const int rows = 4;
  const float portraitSize = 120.0f;
  const float padding = 20.0f;

  ImVec2 windowSize = ImGui::GetIO().DisplaySize;
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  float gridWidth = columns * (portraitSize + padding);
  float gridHeight = rows * (portraitSize + padding);

  // Render title at top
  // ...

  // LEFT SIDE: Character grid
  // Position grid on left side of screen
  ImGui::SetNextWindowPos(ImVec2(50, center.y), ImGuiCond_Always, ImVec2(0.0f, 0.5f));
  ImGui::Begin("Character Grid", ...);

  // Render 5×4 grid of portraits
  // For each character:
  //   - Load portrait texture (or fallback to colored button)
  //   - Calculate size (grow if selected: size * 1.1 * pulse)
  //   - Draw portrait as ImGui::Image()
  //   - Add yellow border if selected
  //   - Handle click to select
  //   - Show name tooltip on hover

  ImGui::End();

  // RIGHT SIDE: Character preview
  ImGui::SetNextWindowPos(ImVec2(windowSize.x - 50, center.y),
                          ImGuiCond_Always, ImVec2(1.0f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_Always);

  ImGui::Begin("Character Preview", ...);

  if (selectedCharacterId != 0) {
    const CharacterDefinition* character =
      CharacterRegistry::instance().getCharacter(selectedCharacterId);

    if (character) {
      // Display character name
      ImGui::SetWindowFontScale(1.5f);
      ImGui::Text("%s", character->name.c_str());
      ImGui::SetWindowFontScale(1.0f);

      ImGui::Separator();
      ImGui::Spacing();

      // Render sprite preview using idle animation frame
      // Use SpriteRenderer to draw a region of the player sprite sheet
      // Get idle frame coordinates from AnimationController
      // Idle animation: srcX=0, srcY=0, srcW=32, srcH=32 (from AnimationConfig.h)

      Texture* playerTexture = TextureManager::instance().get("assets/player_animated.png");
      if (playerTexture && playerTexture->isLoaded()) {
        // Calculate preview size (3x game size = 96x96)
        float previewSize = 96.0f;

        // Center the preview in the window
        ImVec2 cursorPos = ImGui::GetCursorPos();
        float centerX = (300 - previewSize) / 2.0f;
        ImGui::SetCursorPos(ImVec2(centerX, cursorPos.y));

        // Draw sprite with color tint applied
        // Note: ImGui::Image doesn't support color tinting directly
        // Need to use SpriteRenderer or create colored quad behind sprite

        ImGui::Image((ImTextureID)(intptr_t)playerTexture->getID(),
                    ImVec2(previewSize, previewSize),
                    ImVec2(0, 0),           // UV top-left (idle frame)
                    ImVec2(32.0f/256.0f, 32.0f/256.0f),  // UV bottom-right
                    ImVec4(character->r / 255.0f,
                           character->g / 255.0f,
                           character->b / 255.0f,
                           1.0f));  // Color tint
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

  // BOTTOM CENTER: Confirm button
  if (selectedCharacterId != 0) {
    if (ImGui::Button("Confirm Selection")) {
      CharacterSelectionState::instance().setSelectedCharacter(selectedCharacterId);
      GameStateManager::instance().transitionTo(GameState::Playing);
    }
  }
}
```

### Color Preservation Strategy

Server currently auto-assigns colors to players. To preserve client's character selection:

1. Client selects character → RGB stored in CharacterSelectionState
2. Client joins server → Server sends PlayerJoinedPacket with auto-assigned color
3. **Client IGNORES server color** if CharacterSelectionState has a selection
4. Client keeps character selection colors for rendering

This avoids network protocol changes while allowing cosmetic character selection.

## File Summary

### New Files (6)
1. `include/CharacterDefinition.h` - Character data structures
2. `include/CharacterRegistry.h` - Character registry interface
3. `src/CharacterRegistry.cpp` - 20 character definitions
4. `include/CharacterSelectionState.h` - Selection state interface
5. `src/CharacterSelectionState.cpp` - Selection state implementation
6. `assets/portraits/` - Directory with 20 character portrait images (user-provided)

### Modified Files (9)
1. `include/GameState.h` - Add CharacterSelect enum value
2. `src/GameStateManager.cpp` - Update state names array
3. `include/UISystem.h` - Add renderCharacterSelect() + state fields
4. `src/UISystem.cpp` - Implement renderCharacterSelect() + update renderTitleScreen()
5. `src/InputSystem.cpp` - Route TitleScreen → CharacterSelect
6. `src/client_main.cpp` - Apply character selection to local player
7. `src/ClientPrediction.cpp` - Preserve character colors on PlayerJoinedPacket
8. `src/RemotePlayerInterpolation.cpp` - Preserve character colors on PlayerJoinedPacket
9. `CMakeLists.txt` - Add CharacterRegistry.cpp and CharacterSelectionState.cpp to build

## Critical Integration Points

### State Flow
```
TitleScreen (any key)
  ↓
CharacterSelect (click character + confirm)
  ↓
Playing (character color applied to local player)
```

### Character Data Flow
```
1. User clicks character in grid
2. selectedCharacterId stored in UISystem state
3. User clicks "Confirm Selection"
4. CharacterSelectionState::setSelectedCharacter(id)
5. Transition to Playing state
6. client_main.cpp reads CharacterSelectionState
7. Apply character RGB to localPlayer
8. Server sends PlayerJoinedPacket (ignored for local player RGB)
9. Player rendered with character selection colors
```

## Future Extensibility

### Phase 2: Stats & Classes (Stubbed for Now)
- Uncomment `CharacterStats` and `CharacterClass` fields in CharacterDefinition
- Add stat modifiers to Player struct
- Apply bonuses in combat/movement systems
- Display stats in character select tooltips

### Phase 3: Network Synchronization
- Add `CharacterSelectionPacket` to NetworkProtocol.h
- Client sends selection to server
- Server validates and stores in Player struct
- Server broadcasts character selections to all clients
- Enforce unique character limits (optional)

### Phase 4: Unique Sprites Per Character
- User provides different sprite sheets per character
- AnimationAssetLoader uses `character.spriteSheetPath` instead of hardcoded path
- Support character-specific animation layouts

## Testing Checklist
- [ ] TitleScreen shows Settings/Quit buttons
- [ ] Any key on TitleScreen → CharacterSelect
- [ ] Character grid displays 5×4 layout on left side
- [ ] Character preview panel on right side
- [ ] Preview shows "Select a character" when none selected
- [ ] Hover over character shows name tooltip
- [ ] Click character → selection border appears
- [ ] Selected character portrait grows/pulses
- [ ] Character preview updates to show selected character sprite (idle frame)
- [ ] Character preview shows character name
- [ ] Character preview sprite has correct color tint applied
- [ ] Confirm button disabled until selection made
- [ ] Confirm button → Transition to Playing
- [ ] Local player renders with selected character color in-game
- [ ] ESC in CharacterSelect → Back to TitleScreen
- [ ] Server color assignment doesn't override character selection
- [ ] Multiple clients can select different characters

## Known Limitations
- Character selection is client-side only (no server validation)
- Server still auto-assigns colors (overridden by client)
- All characters use same sprite sheet with color tints
- No stats/abilities differences between characters
- Character selection is final (can't change without restart)

These limitations are intentional for Phase 1 proof of concept and will be addressed in future phases.
