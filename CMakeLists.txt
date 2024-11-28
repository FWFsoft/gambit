cmake_minimum_required(VERSION 3.16)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)

# Add source files
add_executable(GameEngine main.cpp src/Logger.cpp src/Window.cpp src/FileSystem.cpp)

# Include directories
target_include_directories(GameEngine PRIVATE include)

# Link libraries
find_package(SDL2 REQUIRED)
find_package(spdlog REQUIRED)

target_link_libraries(GameEngine PRIVATE SDL2::SDL2 spdlog::spdlog)
