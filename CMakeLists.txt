cmake_minimum_required(VERSION 3.16)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)

# Code coverage options
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Find packages
find_package(SDL2 REQUIRED)
find_package(spdlog REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

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

# Find tmxlite library
# Try CMake config first (for vcpkg), then pkg-config, then manual search
find_package(tmxlite CONFIG QUIET)

if(NOT tmxlite_FOUND)
    # Try pkg-config (for system installations)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(TMXLITE IMPORTED_TARGET tmxlite)
    endif()

    # If pkg-config also failed, manually search for tmxlite (vcpkg fallback)
    if(NOT TMXLITE_FOUND)
        # Find tmxlite dependencies first (needed for manual linking)
        find_package(ZLIB REQUIRED)
        find_package(zstd CONFIG QUIET)

        # Determine which zstd target is available
        if(TARGET zstd::libzstd_shared)
            set(ZSTD_TARGET zstd::libzstd_shared)
        elseif(TARGET zstd::libzstd_static)
            set(ZSTD_TARGET zstd::libzstd_static)
        elseif(TARGET zstd::libzstd)
            set(ZSTD_TARGET zstd::libzstd)
        else()
            message(FATAL_ERROR "zstd library not found")
        endif()

        find_path(TMXLITE_INCLUDE_DIR tmxlite/Map.hpp)
        find_library(TMXLITE_LIBRARY NAMES tmxlite libtmxlite)

        if(TMXLITE_INCLUDE_DIR AND TMXLITE_LIBRARY)
            message(STATUS "Found tmxlite (manual):")
            message(STATUS "  Include: ${TMXLITE_INCLUDE_DIR}")
            message(STATUS "  Library: ${TMXLITE_LIBRARY}")

            # Create imported target
            add_library(tmxlite::tmxlite UNKNOWN IMPORTED)
            set_target_properties(tmxlite::tmxlite PROPERTIES
                IMPORTED_LOCATION "${TMXLITE_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${TMXLITE_INCLUDE_DIR}"
            )

            # Link tmxlite dependencies
            target_link_libraries(tmxlite::tmxlite INTERFACE ZLIB::ZLIB ${ZSTD_TARGET})

            set(tmxlite_FOUND TRUE)
        else()
            message(FATAL_ERROR "tmxlite library not found")
        endif()
    endif()
endif()

# tmxlite dependencies (only needed when using pkg-config)
if(TARGET PkgConfig::TMXLITE)
    find_package(ZLIB REQUIRED)
    find_package(zstd CONFIG QUIET)
    if(NOT TARGET zstd::libzstd_shared AND NOT TARGET zstd::libzstd_static AND NOT TARGET zstd::libzstd)
        message(FATAL_ERROR "zstd library not found for pkg-config tmxlite")
    endif()
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
    src/TiledMap.cpp
    src/CollisionSystem.cpp
    src/AnimationController.cpp
    src/AnimationSystem.cpp
    src/AnimationAssetLoader.cpp
    src/EnemySystem.cpp
    src/ItemRegistry.cpp
)
target_include_directories(Server SYSTEM PRIVATE ${ENET_INCLUDE_DIR})
target_include_directories(Server PRIVATE include)
target_link_libraries(Server PRIVATE
    ${ENET_LIBRARY}
    SDL2::SDL2
    spdlog::spdlog
)
# Link tmxlite (CMake config, manual, or pkg-config)
if(tmxlite_FOUND)
    target_link_libraries(Server PRIVATE tmxlite::tmxlite)
elseif(TARGET PkgConfig::TMXLITE)
    target_link_libraries(Server PRIVATE
        PkgConfig::TMXLITE
        ZLIB::ZLIB
        $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
    )
endif()

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
    src/CollisionSystem.cpp
    src/CollisionDebugRenderer.cpp
    src/OpenGLUtils.cpp
    src/Texture.cpp
    src/TextureManager.cpp
    src/SpriteRenderer.cpp
    src/AnimationController.cpp
    src/AnimationSystem.cpp
    src/AnimationAssetLoader.cpp
    src/MusicSystem.cpp
    src/MusicZoneDebugRenderer.cpp
    src/EnemyInterpolation.cpp
    src/CombatSystem.cpp
    src/UISystem.cpp
    src/GameStateManager.cpp
    src/Settings.cpp
    src/ItemRegistry.cpp
    src/imgui_backends/imgui_impl_sdl2.cpp
    src/imgui_backends/imgui_impl_opengl3.cpp
)
target_include_directories(Client SYSTEM PRIVATE ${ENET_INCLUDE_DIR})
target_include_directories(Client PRIVATE include)
target_include_directories(Client SYSTEM PRIVATE src/imgui_backends)
target_link_libraries(Client PRIVATE
    ${ENET_LIBRARY}
    SDL2::SDL2
    spdlog::spdlog
    OpenGL::GL
    glad::glad
    glm::glm
    imgui::imgui
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,$<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image-static>,SDL2_image::SDL2_image-static,SDL2_image::SDL2_image>>
    $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,$<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer-static>,SDL2_mixer::SDL2_mixer-static,SDL2_mixer::SDL2_mixer>>
)
# Link tmxlite (CMake config, manual, or pkg-config)
if(tmxlite_FOUND)
    target_link_libraries(Client PRIVATE tmxlite::tmxlite)
elseif(TARGET PkgConfig::TMXLITE)
    target_link_libraries(Client PRIVATE
        PkgConfig::TMXLITE
        ZLIB::ZLIB
        $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
    )
endif()

# Enable testing
enable_testing()

# Test executables
add_executable(test_player
    tests/test_player.cpp
    src/Logger.cpp
    src/NetworkProtocol.cpp
    src/CollisionSystem.cpp
    src/AnimationController.cpp
    src/AnimationSystem.cpp
    src/AnimationAssetLoader.cpp
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
    src/GameStateManager.cpp
    src/AnimationController.cpp
    src/AnimationSystem.cpp
    src/AnimationAssetLoader.cpp
)
target_include_directories(test_inputsystem SYSTEM PRIVATE ${ENET_INCLUDE_DIR})
target_include_directories(test_inputsystem PRIVATE include tests)
target_link_libraries(test_inputsystem PRIVATE spdlog::spdlog SDL2::SDL2)

add_executable(test_remote_player_interpolation
    tests/test_remote_player_interpolation.cpp
    src/Logger.cpp
    src/NetworkProtocol.cpp
    src/RemotePlayerInterpolation.cpp
    src/AnimationController.cpp
    src/AnimationSystem.cpp
    src/AnimationAssetLoader.cpp
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
    src/CollisionSystem.cpp
    src/AnimationController.cpp
    src/AnimationSystem.cpp
    src/AnimationAssetLoader.cpp
)
target_include_directories(test_client_prediction SYSTEM PRIVATE ${ENET_INCLUDE_DIR})
target_include_directories(test_client_prediction PRIVATE include tests)
target_link_libraries(test_client_prediction PRIVATE
    ${ENET_LIBRARY}
    spdlog::spdlog
    SDL2::SDL2
)

add_executable(test_collision
    tests/test_collision.cpp
    src/Logger.cpp
    src/CollisionSystem.cpp
    src/NetworkProtocol.cpp
)
target_include_directories(test_collision SYSTEM PRIVATE ${ENET_INCLUDE_DIR})
target_include_directories(test_collision PRIVATE include tests)
target_link_libraries(test_collision PRIVATE spdlog::spdlog SDL2::SDL2)

add_executable(test_music_zone
    tests/test_music_zone.cpp
    src/Logger.cpp
)
target_include_directories(test_music_zone PRIVATE include tests)
target_link_libraries(test_music_zone PRIVATE spdlog::spdlog SDL2::SDL2)

add_executable(test_music_system
    tests/test_music_system.cpp
    src/Logger.cpp
)
target_include_directories(test_music_system PRIVATE include tests)
target_link_libraries(test_music_system PRIVATE spdlog::spdlog SDL2::SDL2 $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,$<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer-static>,SDL2_mixer::SDL2_mixer-static,SDL2_mixer::SDL2_mixer>>)

# Register tests with CTest
add_test(NAME PlayerMovement COMMAND test_player)
add_test(NAME NetworkProtocol COMMAND test_network_protocol)
add_test(NAME EventBus COMMAND test_eventbus)
add_test(NAME InputSystem COMMAND test_inputsystem)
add_test(NAME RemotePlayerInterpolation COMMAND test_remote_player_interpolation)
add_test(NAME ClientPrediction COMMAND test_client_prediction)
add_test(NAME CollisionSystem COMMAND test_collision)
add_test(NAME MusicZone COMMAND test_music_zone)
add_test(NAME MusicSystem COMMAND test_music_system)

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
    target_compile_options(test_collision PRIVATE --coverage)
    target_link_options(test_collision PRIVATE --coverage)
    target_compile_options(test_music_zone PRIVATE --coverage)
    target_link_options(test_music_zone PRIVATE --coverage)
    target_compile_options(test_music_system PRIVATE --coverage)
    target_link_options(test_music_system PRIVATE --coverage)
endif()

