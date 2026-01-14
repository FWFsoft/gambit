#include "TestInputReader.h"

#include <sstream>

#include "Logger.h"

// Key name to SDL scancode mapping
const std::unordered_map<std::string, SDL_Scancode> TestInputReader::keyMap = {
    {"W", SDL_SCANCODE_W},           {"A", SDL_SCANCODE_A},
    {"S", SDL_SCANCODE_S},           {"D", SDL_SCANCODE_D},
    {"Up", SDL_SCANCODE_UP},         {"Down", SDL_SCANCODE_DOWN},
    {"Left", SDL_SCANCODE_LEFT},     {"Right", SDL_SCANCODE_RIGHT},
    {"Space", SDL_SCANCODE_SPACE},   {"Enter", SDL_SCANCODE_RETURN},
    {"Escape", SDL_SCANCODE_ESCAPE}, {"E", SDL_SCANCODE_E},
    {"I", SDL_SCANCODE_I},           {"Tab", SDL_SCANCODE_TAB},
    {"F1", SDL_SCANCODE_F1},
};

TestInputReader::TestInputReader(const std::string& commandFilePath)
    : filePath(commandFilePath) {
  Logger::info("TestInputReader initialized with file: " + filePath);
}

void TestInputReader::reopenFile() {
  if (file.is_open()) {
    file.close();
  }
  file.open(filePath);
  if (!file.is_open()) {
    Logger::error("Could not open test input file: " + filePath);
  }
  // Don't reset lineNumber - we need to continue from where we left off
}

std::optional<TestInputReader::Command> TestInputReader::readNextCommand() {
  // If we're waiting, don't read new commands
  if (waitFramesRemaining > 0) {
    return std::nullopt;
  }

  // Reopen file to get latest commands (allows live editing)
  reopenFile();

  // Skip to current line
  std::string line;
  int currentLine = 0;
  while (currentLine < lineNumber && std::getline(file, line)) {
    currentLine++;
  }

  // Read next line
  while (std::getline(file, line)) {
    lineNumber++;

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
      continue;
    }

    auto command = parseLine(line);
    if (command) {
      // If it's a WAIT command, set wait counter
      if (command->type == CommandType::WAIT) {
        waitFramesRemaining = command->intArg;
        Logger::info("TestInput: Waiting " + std::to_string(command->intArg) +
                     " frames");
      }
      return command;
    }
  }

  return std::nullopt;
}

void TestInputReader::tick() {
  if (waitFramesRemaining > 0) {
    waitFramesRemaining--;
  }
}

std::optional<TestInputReader::Command> TestInputReader::parseLine(
    const std::string& line) {
  std::istringstream iss(line);
  std::string commandStr;
  iss >> commandStr;

  Command cmd;

  if (commandStr == "WAIT") {
    cmd.type = CommandType::WAIT;
    iss >> cmd.intArg;
    if (cmd.intArg <= 0) {
      Logger::error("TestInput: Invalid WAIT value on line " +
                    std::to_string(lineNumber));
      return std::nullopt;
    }
    return cmd;
  } else if (commandStr == "KEY_DOWN") {
    cmd.type = CommandType::KEY_DOWN;
    iss >> cmd.stringArg;
    if (cmd.stringArg.empty()) {
      Logger::error("TestInput: Missing key name for KEY_DOWN on line " +
                    std::to_string(lineNumber));
      return std::nullopt;
    }
    Logger::info("TestInput: KEY_DOWN " + cmd.stringArg);
    return cmd;
  } else if (commandStr == "KEY_UP") {
    cmd.type = CommandType::KEY_UP;
    iss >> cmd.stringArg;
    if (cmd.stringArg.empty()) {
      Logger::error("TestInput: Missing key name for KEY_UP on line " +
                    std::to_string(lineNumber));
      return std::nullopt;
    }
    Logger::info("TestInput: KEY_UP " + cmd.stringArg);
    return cmd;
  } else if (commandStr == "SCREENSHOT") {
    cmd.type = CommandType::SCREENSHOT;
    iss >> cmd.stringArg;
    if (cmd.stringArg.empty()) {
      cmd.stringArg = "screenshot";
    }
    Logger::info("TestInput: SCREENSHOT " + cmd.stringArg);
    return cmd;
  } else if (commandStr == "MOUSE_MOVE") {
    cmd.type = CommandType::MOUSE_MOVE;
    iss >> cmd.intArg >> cmd.intArg2;
    if (iss.fail()) {
      Logger::error("TestInput: Invalid MOUSE_MOVE coordinates on line " +
                    std::to_string(lineNumber));
      return std::nullopt;
    }
    Logger::info("TestInput: MOUSE_MOVE " + std::to_string(cmd.intArg) + " " +
                 std::to_string(cmd.intArg2));
    return cmd;
  } else if (commandStr == "MOUSE_CLICK") {
    cmd.type = CommandType::MOUSE_CLICK;
    iss >> cmd.stringArg;
    if (cmd.stringArg.empty()) {
      cmd.stringArg = "left";  // Default to left click
    }
    Logger::info("TestInput: MOUSE_CLICK " + cmd.stringArg);
    return cmd;
  }

  Logger::error("TestInput: Unknown command '" + commandStr + "' on line " +
                std::to_string(lineNumber));
  return std::nullopt;
}

SDL_Scancode TestInputReader::stringToScancode(const std::string& keyName) {
  auto it = keyMap.find(keyName);
  if (it != keyMap.end()) {
    return it->second;
  }
  Logger::error("TestInput: Unknown key name '" + keyName + "'");
  return SDL_SCANCODE_UNKNOWN;
}
