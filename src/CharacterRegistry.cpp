#include "CharacterRegistry.h"

CharacterRegistry& CharacterRegistry::instance() {
  static CharacterRegistry instance;
  return instance;
}

CharacterRegistry::CharacterRegistry() { initializeCharacters(); }

void CharacterRegistry::initializeCharacters() {
  // 19 Characters with actual portrait images

  characters.push_back(CharacterDefinition(1, "Eliana", 240, 230,
                                           255));  // Light ethereal purple
  characters.push_back(
      CharacterDefinition(2, "Fagan", 100, 150, 50));  // Forest green
  characters.push_back(
      CharacterDefinition(3, "Gravon", 120, 120, 130));  // Stone gray
  characters.push_back(
      CharacterDefinition(4, "Isaac", 50, 150, 200));  // Tech blue
  characters.push_back(
      CharacterDefinition(5, "Jeff", 255, 140, 0));  // Bright orange
  characters.push_back(
      CharacterDefinition(6, "Kade", 180, 30, 30));  // Dark red
  characters.push_back(CharacterDefinition(7, "Lilith", 150, 50,
                                           150));  // Purple mystery
  characters.push_back(
      CharacterDefinition(8, "MILES", 255, 220, 0));  // Bright yellow
  characters.push_back(CharacterDefinition(9, "Mina", 255, 150, 180));  // Pink
  characters.push_back(CharacterDefinition(10, "Mordryn", 60, 60,
                                           80));  // Dark blue-gray
  characters.push_back(
      CharacterDefinition(11, "Namora", 0, 200, 200));  // Cyan water
  characters.push_back(
      CharacterDefinition(12, "Nolan", 140, 90, 50));  // Brown earth
  characters.push_back(
      CharacterDefinition(13, "Nyx", 100, 30, 100));  // Dark purple
  characters.push_back(CharacterDefinition(14, "Presidente",

                                           250, 240, 230));  // Cream/gold
  characters.push_back(
      CharacterDefinition(15, "Stitches", 100, 200, 50));  // Lime green
  characters.push_back(
      CharacterDefinition(16, "Suds", 150, 200, 255));  // Light blue
  characters.push_back(
      CharacterDefinition(17, "Valthor", 190, 200, 210));  // Silver metal
  characters.push_back(CharacterDefinition(18, "Volgore", 255, 50,
                                           0));  // Bright red fire
  characters.push_back(CharacterDefinition(19, "Wade", 30, 50, 120));  // Navy

  // Build ID lookup
  for (size_t i = 0; i < characters.size(); i++) {
    idToIndex[characters[i].id] = i;
  }
}

const CharacterDefinition* CharacterRegistry::getCharacter(uint32_t id) const {
  auto it = idToIndex.find(id);
  if (it != idToIndex.end()) {
    return &characters[it->second];
  }
  return nullptr;
}

const std::vector<CharacterDefinition>& CharacterRegistry::getAllCharacters()
    const {
  return characters;
}
