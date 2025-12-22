cmake_minimum_required(VERSION 3.16)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(SDL2 REQUIRED)
find_package(spdlog REQUIRED)

# Find ENet library and include directory
find_path(ENET_INCLUDE_DIR enet/enet.h)
find_library(ENET_LIBRARY enet)

if(ENET_INCLUDE_DIR AND ENET_LIBRARY)
    message(STATUS "Found ENet:")
    message(STATUS "  Include directories: ${ENET_INCLUDE_DIR}")
    message(STATUS "  Libraries: ${ENET_LIBRARY}")
else()
    message(FATAL_ERROR "ENet library not found")
endif()

# Server executable
add_executable(Server
    src/server_main.cpp
    src/Logger.cpp
    src/NetworkServer.cpp
    src/FileSystem.cpp
    src/NetworkProtocol.cpp
    src/GameLoop.cpp
    src/ServerGameState.cpp
)
target_include_directories(Server PRIVATE
    include
    ${ENET_INCLUDE_DIR}
)
target_link_libraries(Server PRIVATE
    ${ENET_LIBRARY}
    SDL2::SDL2
    spdlog::spdlog
)

# Client executable
add_executable(Client
    src/client_main.cpp
    src/Logger.cpp
    src/NetworkClient.cpp
    src/FileSystem.cpp
    src/Window.cpp
    src/GameLoop.cpp
    src/InputSystem.cpp
    src/NetworkProtocol.cpp
)
target_include_directories(Client PRIVATE
    include
    ${ENET_INCLUDE_DIR}
)
target_link_libraries(Client PRIVATE
    ${ENET_LIBRARY}
    SDL2::SDL2
    spdlog::spdlog
)

# Test executable
add_executable(Tests
    src/test_main.cpp
    src/Logger.cpp
    src/NetworkProtocol.cpp
)
target_include_directories(Tests PRIVATE
    include
)
target_link_libraries(Tests PRIVATE
    spdlog::spdlog
)

