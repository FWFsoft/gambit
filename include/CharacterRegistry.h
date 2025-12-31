#pragma once

#include <unordered_map>
#include <vector>

#include "CharacterDefinition.h"

// Singleton registry for all available characters
class CharacterRegistry {
 public:
  static CharacterRegistry& instance();

  const CharacterDefinition* getCharacter(uint32_t id) const;
  const std::vector<CharacterDefinition>& getAllCharacters() const;

 private:
  CharacterRegistry();
  void initializeCharacters();

  std::vector<CharacterDefinition> characters;
  std::unordered_map<uint32_t, size_t> idToIndex;
};
