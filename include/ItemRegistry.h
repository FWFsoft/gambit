#pragma once

#include <map>
#include <string>

#include "Item.h"

// Singleton registry for item definitions
class ItemRegistry {
 public:
  static ItemRegistry& instance() {
    static ItemRegistry registry;
    return registry;
  }

  // Load items from CSV file
  bool loadFromCSV(const std::string& filepath);

  // Get item definition by ID
  const ItemDefinition* getItem(uint32_t id) const;

  // Check if item exists
  bool hasItem(uint32_t id) const;

  // Get all items (for debugging/testing)
  const std::map<uint32_t, ItemDefinition>& getAllItems() const {
    return items;
  }

 private:
  ItemRegistry() = default;
  std::map<uint32_t, ItemDefinition> items;

  // Helper to parse ItemType from string
  ItemType parseItemType(const std::string& str) const;

  // Helper to parse ItemRarity from string
  ItemRarity parseItemRarity(const std::string& str) const;
};
