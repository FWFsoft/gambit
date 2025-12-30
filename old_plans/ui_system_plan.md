# ImGui-Based UI System Implementation Plan

## Goal

Implement a complete UI system for Gambit using ImGui, including:
- **ImGui Core Integration**: Framework setup with SDL2/OpenGL backend
- **Game State Management**: State machine for menu/playing/inventory transitions
- **Main Menu**: Start game, quit, settings with persistence
- **In-Game HUD**: Health display and pause menu
- **Inventory System**: Full item management with network synchronization

This provides a functional UI foundation with clean hooks for future Rive migration.

---

## Phase 1: ImGui Integration

### Goal
Set up ImGui framework with SDL2/OpenGL backend, integrate into Window and RenderSystem.

### New Files
- `include/UISystem.h` - UI system class declaration
- `src/UISystem.cpp` - UI rendering implementation

### Modified Files
- `vcpkg.json` - Add ImGui dependency
- `CMakeLists.txt` - Link ImGui, add UISystem sources
- `include/Window.h` - Add initImGui/shutdownImGui methods
- `src/Window.cpp` - Implement ImGui initialization and SDL event routing
- `src/client_main.cpp` - Initialize UISystem

### Key Changes

**vcpkg.json**: Add `"imgui[sdl2-binding,opengl3-binding]"` to dependencies

**Window.cpp**:
- Initialize ImGui after GLAD (OpenGL 3.3 context)
- Route SDL events to ImGui first with `WantCaptureKeyboard/Mouse` checks
- Prevents game input when UI has focus

**UISystem.cpp**:
- Subscribes to RenderEvent
- Manages ImGui frame lifecycle (NewFrame → UI rendering → Render)
- Initially shows demo window for verification

---

## Phase 2: Game State Machine

### Goal
Create state management system to control game flow (MainMenu → Playing → Inventory/Paused).

### New Files
- `include/GameState.h` - GameState enum and GameStateChangedEvent
- `include/GameStateManager.h` - State manager singleton
- `src/GameStateManager.cpp` - State transition implementation

### Modified Files
- `include/EventBus.h` - Include GameState.h for events
- `include/InputSystem.h` - Add GameStateManager include
- `src/InputSystem.cpp` - State-aware input handling (ESC/I keys)
- `src/client_main.cpp` - Conditional network processing

### States
```cpp
enum class GameState {
  MainMenu,    // Title screen, settings, quit
  Playing,     // Gameplay, HUD visible
  Paused,      // Pause menu, gameplay frozen
  Inventory    // Inventory screen, gameplay frozen
};
```

### Input Mapping
- **ESC**: Toggle pause (Playing ↔ Paused)
- **I**: Toggle inventory (Playing/Paused ↔ Inventory)
- Game input only processes during Playing state

---

## Phase 3: Main Menu

### Goal
Implement main menu UI with Start Game, Quit, and Settings panel.

### New Files
- `include/Settings.h` - Settings data structure
- `src/Settings.cpp` - Settings save/load (INI format)

### Modified Files
- `include/UISystem.h` - Add settings member, renderMainMenu/renderSettingsPanel methods
- `src/UISystem.cpp` - Implement menu UI
- `include/Window.h` - Add close() method
- `src/client_main.cpp` - Pass window pointer to UISystem

### Settings Structure
```cpp
struct Settings {
  float masterVolume, musicVolume, sfxVolume;
  bool muted;
  int windowWidth, windowHeight;
  bool fullscreen, vsync;
};
```

**Persistence**: Simple INI format (`settings.ini` in working directory)

**UI Features**:
- Centered main menu window (300x200px)
- Settings panel overlay with collapsing headers (Audio, Graphics, Controls)
- Save/Cancel buttons for settings
- Quit button triggers window close

---

## Phase 4: In-Game HUD and Pause Menu

### Goal
Add health display HUD and pause menu overlay.

### Modified Files
- `include/UISystem.h` - Add clientPrediction member
- `src/UISystem.cpp` - Implement renderHUD and renderPauseMenu
- `src/client_main.cpp` - Pass ClientPrediction to UISystem

### HUD Design
- **Location**: Top-left corner (10, 10)
- **Display**: Health text + progress bar
- **Color**: Green → Yellow → Red based on health percentage
- No title bar, background (clean overlay)

### Pause Menu
- Semi-transparent black background overlay
- Centered menu (300x250px)
- Buttons: Resume (ESC), Settings, Main Menu, Quit
- Settings panel accessible from pause

---

## Phase 5: Inventory System

### Goal
Implement full inventory system with item definitions, UI, equipping, and network sync.

### New Files
- `include/Item.h` - Item data structures (ItemDefinition, ItemStack, ItemType, ItemRarity)
- `include/ItemRegistry.h` - Item definition registry singleton
- `src/ItemRegistry.cpp` - CSV item loading
- `src/PlayerInventory.cpp` - Inventory operations (add/remove/find)
- `assets/items.csv` - Item definitions database

### Modified Files
- `include/Player.h` - Add inventory array, equipment slots, helper methods
- `include/UISystem.h` - Add inventory rendering methods
- `src/UISystem.cpp` - Implement renderInventory
- `include/NetworkProtocol.h` - Add inventory packet types
- `src/NetworkProtocol.cpp` - Implement inventory serialization
- `include/ServerGameState.h` - Add item packet handler
- `src/ServerGameState.cpp` - Server-side inventory validation

### Item System Design

**ItemDefinition** (immutable, loaded from CSV):
- id, name, description, type, rarity, iconPath
- Stats: damage, armor, healthBonus, healAmount
- maxStackSize (1 = non-stackable, 99 = stackable)

**ItemStack** (per-player instance):
- itemId (references ItemDefinition)
- quantity

**Player Inventory**:
- Fixed 20-slot array
- 2 equipment slots (weapon, armor)
- Auto-stacking for consumables

### Inventory UI

**Layout**: Fullscreen (2-column)
- Left: 5x4 grid of inventory slots (100x100px each)
- Right: Equipment panel + stats display

**Features**:
- Tooltips on hover (name, rarity color, description, stats)
- Stack quantity display (e.g., "x10" for potions)
- Equipment slots separate from inventory
- Stat calculation with equipment bonuses

### Network Protocol

**Packet Types**:
- `InventoryUpdate` - Full inventory sync from server
- `ItemPickup` - Client requests item pickup
- `ItemDrop` - Client requests item drop
- `ItemEquip` - Client requests equip/unequip

**Architecture**: Server-authoritative
- Server validates all inventory operations
- Server broadcasts updates to all clients
- Prevents client-side cheating

### CSV Format
```csv
id,name,type,rarity,damage,armor,health,heal,maxStack,icon,description
1,Health Potion,0,0,0,0,0,50,99,assets/icons/potion.png,Restores 50 health
2,Iron Sword,1,0,25,0,0,0,1,assets/icons/sword.png,A basic iron sword
```

---

## Critical Files

| File | Purpose |
|------|---------|
| `src/UISystem.cpp` | Core UI rendering logic for all screens |
| `src/Window.cpp` | ImGui initialization and SDL event routing |
| `src/GameStateManager.cpp` | State transition coordination |
| `include/NetworkProtocol.h` | Inventory packet definitions |
| `src/ItemRegistry.cpp` | Item loading and management |
| `src/PlayerInventory.cpp` | Inventory operations |
| `src/client_main.cpp` | System initialization hookup |

---

## File Manifest

### New Files (11)
**Headers:**
- `include/GameState.h`
- `include/GameStateManager.h`
- `include/Settings.h`
- `include/UISystem.h`
- `include/Item.h`
- `include/ItemRegistry.h`

**Implementation:**
- `src/GameStateManager.cpp`
- `src/Settings.cpp`
- `src/UISystem.cpp`
- `src/ItemRegistry.cpp`
- `src/PlayerInventory.cpp`

### Modified Files (11)
- `vcpkg.json`
- `CMakeLists.txt`
- `include/Window.h`
- `src/Window.cpp`
- `include/EventBus.h`
- `src/InputSystem.cpp`
- `src/client_main.cpp`
- `include/Player.h`
- `include/NetworkProtocol.h`
- `src/NetworkProtocol.cpp`
- `include/ServerGameState.h`
- `src/ServerGameState.cpp`

### Assets
- `assets/items.csv` (create manually with test items)
- `settings.ini` (generated at runtime)

---

## Testing Strategy

### Phase 1: ImGui Integration
- Build succeeds with ImGui dependency
- Client runs and shows demo window
- Keyboard input blocked when typing in demo window

### Phase 2: Game State
- Client starts in MainMenu state
- ESC toggles pause during gameplay
- I opens inventory
- Movement blocked when not Playing

### Phase 3: Main Menu
- Main menu appears centered on startup
- Settings panel opens/closes
- Settings persist to `settings.ini`
- Start Game transitions to Playing state
- Quit closes window

### Phase 4: HUD
- Health bar appears top-left
- Health bar color changes with damage
- Pause menu appears on ESC with dimmed background
- Resume returns to gameplay

### Phase 5: Inventory
- Items load from CSV
- Inventory UI opens with I key
- Tooltips show item details
- Equipment slots display equipped items
- Stats calculate with equipment bonuses
- Inventory syncs across multiplayer clients

---

## Architecture Benefits

**Maintains Gambit Principles:**
- Event-based: UISystem subscribes to RenderEvent
- Tiger Style: Simple implementations, no over-engineering
- Fast build/boot: ImGui compiles quickly
- Server-authoritative: Inventory validated server-side

**Clean Migration to Rive:**
- UISystem encapsulates all UI logic
- Swap ImGui → Rive by changing render() implementations
- Settings/GameState/Items remain unchanged
- Network protocol independent of UI framework

**Extensibility:**
- Item system supports rarity, stats, consumables
- GameState easily extended (Crafting, Trading states)
- Settings structure easily extended
- Inventory ready for drag-drop, right-click menus

---

## Implementation Notes

**ImGui Event Handling:**
- `WantCaptureKeyboard/Mouse` prevents game input when UI focused
- Critical for inventory typing, menu navigation

**Network Bandwidth:**
- Full inventory sync: 164 bytes (20 slots × 8 bytes + equipment)
- Only sent on change, not every frame
- Acceptable for 4-player co-op

**Performance:**
- ImGui is immediate-mode (rebuilds each frame)
- Lightweight for our use case (< 100 widgets)
- Renders after game world, before buffer swap
- No impact on 60 FPS update loop
