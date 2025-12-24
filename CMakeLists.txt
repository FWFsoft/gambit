cmake_minimum_required(VERSION 3.16)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)

# Code coverage options
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Find packages
find_package(SDL2 REQUIRED)
find_package(spdlog REQUIRED)

# Find ENet library and include directory
if(APPLE)
    include_directories(SYSTEM /opt/homebrew/include)
endif()
find_path(ENET_INCLUDE_DIR enet/enet.h)
find_library(ENET_LIBRARY enet)

if(ENET_INCLUDE_DIR AND ENET_LIBRARY)
    message(STATUS "Found ENet:")
    message(STATUS "  Include directories: ${ENET_INCLUDE_DIR}")
    message(STATUS "  Libraries: ${ENET_LIBRARY}")
else()
    message(FATAL_ERROR "ENet library not found")
endif()

# Find tmxlite library (uses pkg-config)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TMXLITE REQUIRED IMPORTED_TARGET tmxlite)

# tmxlite dependencies
find_package(ZLIB REQUIRED)
find_package(zstd CONFIG REQUIRED)

# Server executable
add_executable(Server
    src/server_main.cpp
    src/Logger.cpp
    src/NetworkServer.cpp
    src/FileSystem.cpp
    src/NetworkProtocol.cpp
    src/GameLoop.cpp
    src/ServerGameState.cpp
    src/TiledMap.cpp
)
target_include_directories(Server SYSTEM PRIVATE ${ENET_INCLUDE_DIR})
target_include_directories(Server PRIVATE include)
target_link_libraries(Server PRIVATE
    ${ENET_LIBRARY}
    SDL2::SDL2
    spdlog::spdlog
    PkgConfig::TMXLITE
    ZLIB::ZLIB
    $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
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
    src/ClientPrediction.cpp
    src/RemotePlayerInterpolation.cpp
    src/RenderSystem.cpp
    src/TiledMap.cpp
    src/TileRenderer.cpp
)
target_include_directories(Client SYSTEM PRIVATE ${ENET_INCLUDE_DIR})
target_include_directories(Client PRIVATE include)
target_link_libraries(Client PRIVATE
    ${ENET_LIBRARY}
    SDL2::SDL2
    spdlog::spdlog
    PkgConfig::TMXLITE
    ZLIB::ZLIB
    $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
)

# Enable testing
enable_testing()

# Test executables
add_executable(test_player
    tests/test_player.cpp
    src/Logger.cpp
    src/NetworkProtocol.cpp
)
target_include_directories(test_player SYSTEM PRIVATE ${ENET_INCLUDE_DIR})
target_include_directories(test_player PRIVATE include tests)
target_link_libraries(test_player PRIVATE spdlog::spdlog SDL2::SDL2)

add_executable(test_network_protocol
    tests/test_network_protocol.cpp
    src/Logger.cpp
    src/NetworkProtocol.cpp
)
target_include_directories(test_network_protocol SYSTEM PRIVATE ${ENET_INCLUDE_DIR})
target_include_directories(test_network_protocol PRIVATE include tests)
target_link_libraries(test_network_protocol PRIVATE spdlog::spdlog SDL2::SDL2)

add_executable(test_eventbus
    tests/test_eventbus.cpp
    src/Logger.cpp
    src/GameLoop.cpp
    src/NetworkProtocol.cpp
)
target_include_directories(test_eventbus SYSTEM PRIVATE ${ENET_INCLUDE_DIR})
target_include_directories(test_eventbus PRIVATE include tests)
target_link_libraries(test_eventbus PRIVATE spdlog::spdlog SDL2::SDL2)

add_executable(test_inputsystem
    tests/test_inputsystem.cpp
    src/Logger.cpp
    src/InputSystem.cpp
    src/GameLoop.cpp
    src/NetworkProtocol.cpp
)
target_include_directories(test_inputsystem SYSTEM PRIVATE ${ENET_INCLUDE_DIR})
target_include_directories(test_inputsystem PRIVATE include tests)
target_link_libraries(test_inputsystem PRIVATE spdlog::spdlog SDL2::SDL2)

add_executable(test_remote_player_interpolation
    tests/test_remote_player_interpolation.cpp
    src/Logger.cpp
    src/NetworkProtocol.cpp
    src/RemotePlayerInterpolation.cpp
)
target_include_directories(test_remote_player_interpolation SYSTEM PRIVATE ${ENET_INCLUDE_DIR})
target_include_directories(test_remote_player_interpolation PRIVATE include tests)
target_link_libraries(test_remote_player_interpolation PRIVATE spdlog::spdlog SDL2::SDL2)

add_executable(test_client_prediction
    tests/test_client_prediction.cpp
    src/Logger.cpp
    src/NetworkProtocol.cpp
    src/ClientPrediction.cpp
    src/NetworkClient.cpp
)
target_include_directories(test_client_prediction SYSTEM PRIVATE ${ENET_INCLUDE_DIR})
target_include_directories(test_client_prediction PRIVATE include tests)
target_link_libraries(test_client_prediction PRIVATE
    ${ENET_LIBRARY}
    spdlog::spdlog
    SDL2::SDL2
)

# Register tests with CTest
add_test(NAME PlayerMovement COMMAND test_player)
add_test(NAME NetworkProtocol COMMAND test_network_protocol)
add_test(NAME EventBus COMMAND test_eventbus)
add_test(NAME InputSystem COMMAND test_inputsystem)
add_test(NAME RemotePlayerInterpolation COMMAND test_remote_player_interpolation)
add_test(NAME ClientPrediction COMMAND test_client_prediction)

# Enable code coverage for test targets
if(ENABLE_COVERAGE)
    target_compile_options(test_player PRIVATE --coverage)
    target_link_options(test_player PRIVATE --coverage)
    target_compile_options(test_network_protocol PRIVATE --coverage)
    target_link_options(test_network_protocol PRIVATE --coverage)
    target_compile_options(test_eventbus PRIVATE --coverage)
    target_link_options(test_eventbus PRIVATE --coverage)
    target_compile_options(test_inputsystem PRIVATE --coverage)
    target_link_options(test_inputsystem PRIVATE --coverage)
    target_compile_options(test_remote_player_interpolation PRIVATE --coverage)
    target_link_options(test_remote_player_interpolation PRIVATE --coverage)
    target_compile_options(test_client_prediction PRIVATE --coverage)
    target_link_options(test_client_prediction PRIVATE --coverage)
endif()

