#pragma once

#include <cstdint>
#include <string>

// Item types
enum class ItemType : uint8_t {
  Consumable = 0,  // Potions, food, etc.
  Weapon = 1,      // Swords, bows, etc.
  Armor = 2        // Chest plates, helmets, etc.
};

// Item rarity (affects color in UI)
enum class ItemRarity : uint8_t {
  Common = 0,    // White/gray
  Uncommon = 1,  // Green
  Rare = 2,      // Blue
  Epic = 3,      // Purple
  Legendary = 4  // Orange/gold
};

// Item definition (immutable, loaded from CSV)
struct ItemDefinition {
  uint32_t id;
  std::string name;
  std::string description;
  ItemType type;
  ItemRarity rarity;

  // Stats
  int damage;       // Weapon damage bonus
  int armor;        // Armor defense bonus
  int healthBonus;  // Max health increase (armor)
  int healAmount;   // Healing amount (consumables)

  // Stacking
  int maxStackSize;  // 1 = non-stackable, 99 = stackable

  // Asset path
  std::string iconPath;

  ItemDefinition()
      : id(0),
        type(ItemType::Consumable),
        rarity(ItemRarity::Common),
        damage(0),
        armor(0),
        healthBonus(0),
        healAmount(0),
        maxStackSize(1) {}
};

// Item stack (per-player instance)
struct ItemStack {
  uint32_t itemId;  // References ItemDefinition
  int quantity;

  ItemStack() : itemId(0), quantity(0) {}
  ItemStack(uint32_t id, int qty) : itemId(id), quantity(qty) {}

  bool isEmpty() const { return itemId == 0 || quantity <= 0; }
};
