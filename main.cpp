#include "Window.h"
#include "Logger.h"
#include "FileSystem.h"
#include <iostream>

int main() {
    Logger::init();
    Logger::info("Game engine started");
    Logger::error("Test error message");
    
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
