#include <enet/enet.h>

#include <iostream>

#include "FileSystem.h"
#include "Logger.h"
#include "Window.h"

int main() {
  Logger::init();
  Logger::info("Game engine started");
  Logger::error("Test error message");

  if (enet_initialize() != 0) {
    std::cerr << "An error occurred while initializing ENet." << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << "ENet initialized successfully." << std::endl;

  enet_deinitialize();
  std::cout << "ENet deinitialized." << std::endl;

  if (FileSystem::exists("test.txt")) {
    std::cout << "test.txt exists" << std::endl;
  } else {
    std::cout << "test.txt does not exist" << std::endl;
  }

  for (const auto& file : FileSystem::listFiles(".")) {
    std::cout << "File: " << file << std::endl;
  }

  try {
    Window window("Game Engine", 800, 600);
    while (window.isOpen()) {
      window.pollEvents();
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
  return 0;
}
