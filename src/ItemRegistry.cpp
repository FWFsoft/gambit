#include "ItemRegistry.h"

#include <fstream>
#include <sstream>
#include <vector>

#include "Logger.h"

bool ItemRegistry::loadFromCSV(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    Logger::error("Failed to open item CSV: " + filepath);
    return false;
  }

  items.clear();

  // Read header line
  std::string headerLine;
  std::getline(file, headerLine);

  // Expected format:
  // id,name,type,rarity,damage,armor,health,heal,maxStack,icon,description
  int lineNumber = 1;
  std::string line;
  while (std::getline(file, line)) {
    lineNumber++;

    // Skip empty lines
    if (line.empty() || line[0] == '#') {
      continue;
    }

    std::stringstream ss(line);
    std::string token;
    std::vector<std::string> fields;

    // Parse CSV fields
    while (std::getline(ss, token, ',')) {
      fields.push_back(token);
    }

    if (fields.size() < 11) {
      Logger::error("Skipping malformed line " + std::to_string(lineNumber) +
                    " in " + filepath);
      continue;
    }

    ItemDefinition item;
    try {
      item.id = std::stoul(fields[0]);
      item.name = fields[1];
      item.type = parseItemType(fields[2]);
      item.rarity = parseItemRarity(fields[3]);
      item.damage = std::stoi(fields[4]);
      item.armor = std::stoi(fields[5]);
      item.healthBonus = std::stoi(fields[6]);
      item.healAmount = std::stoi(fields[7]);
      item.maxStackSize = std::stoi(fields[8]);
      item.iconPath = fields[9];
      item.description = fields[10];

      items[item.id] = item;
    } catch (const std::exception& e) {
      Logger::error("Failed to parse line " + std::to_string(lineNumber) +
                    " in " + filepath + ": " + e.what());
      continue;
    }
  }

  Logger::info("Loaded " + std::to_string(items.size()) + " items from " +
               filepath);
  return true;
}

const ItemDefinition* ItemRegistry::getItem(uint32_t id) const {
  auto it = items.find(id);
  if (it != items.end()) {
    return &it->second;
  }
  return nullptr;
}

bool ItemRegistry::hasItem(uint32_t id) const {
  return items.find(id) != items.end();
}

ItemType ItemRegistry::parseItemType(const std::string& str) const {
  int val = std::stoi(str);
  return static_cast<ItemType>(val);
}

ItemRarity ItemRegistry::parseItemRarity(const std::string& str) const {
  int val = std::stoi(str);
  return static_cast<ItemRarity>(val);
}
