#pragma once

#include <cstdint>
#include <string>

// Forward declaration for future extensibility
// TODO: Implement stats system in Phase 2
struct CharacterStats {
  // Stubbed for future implementation
  // Will eventually hold damage, health, speed modifiers, etc.
  // For now, cosmetic only
};

// TODO: Implement class system in Phase 2
struct CharacterClass {
  // Stubbed for future implementation
  // Will eventually hold class-specific abilities
  // For now, cosmetic only
};

// Character definition - cosmetic for now, extensible for stats/classes later
struct CharacterDefinition {
  uint32_t id;       // Unique character ID
  std::string name;  // Display name
  std::string
      spriteSheetPath;  // Path to character sprite sheet (unused for now)

  // Color tint for proof-of-concept (reuses existing player sprite)
  uint8_t r, g, b;

  // Stubbed fields for future implementation
  // CharacterStats baseStats;
  // CharacterClass characterClass;

  CharacterDefinition(uint32_t id, const std::string& name, uint8_t r,
                      uint8_t g, uint8_t b)
      : id(id),
        name(name),
        spriteSheetPath("assets/player_animated.png"),  // Use default for now
        r(r),
        g(g),
        b(b) {}

  // TODO: Path joining?
  std::string const getCharacterPortraitPath() const {
    return "assets/portraits/" + this->name + ".png";
  };

  std::string getCharacterPreviewPath() const {
    return "assets/previews/" + this->name + ".png";
  };
};
