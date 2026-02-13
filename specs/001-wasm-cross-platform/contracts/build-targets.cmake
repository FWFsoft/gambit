# Build Targets Contract
# Feature: 001-wasm-cross-platform
#
# This is a design contract showing the intended CMake structure.
# Not meant to be included directly — serves as a reference for implementation.

# --- Emscripten detection ---
# CMAKE_SYSTEM_NAME is "Emscripten" when using emcmake

# --- Native build (existing, with transport abstraction) ---
# Server target: accepts ENet + WebSocket connections
#   Links: ENet, WebSocket library (e.g., uWebSockets), SDL2, spdlog, tmxlite
#   Sources: server_main.cpp, ENetServerTransport, WebSocketServerTransport
#
# Client target: connects via ENet (or InMemory for embedded mode)
#   Links: ENet, SDL2, OpenGL, GLAD, GLM, ImGui, SDL2_image, SDL2_mixer, spdlog, tmxlite
#   Sources: client_main.cpp, ENetTransport, InMemoryTransport (optional)

# --- WASM build (new) ---
if(EMSCRIPTEN)
    # Emscripten linker flags replace find_package calls
    set(EM_COMMON_FLAGS
        "-sUSE_SDL=2"
        "-sUSE_SDL_IMAGE=2"
        "-sUSE_SDL_MIXER=2"
        "-sSDL2_IMAGE_FORMATS=[\"png\",\"jpg\"]"
        "-sUSE_WEBGL2=1"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sFULL_ES3=1"
        "-sALLOW_MEMORY_GROWTH=1"
    )

    set(EM_CLIENT_FLAGS
        ${EM_COMMON_FLAGS}
        "-lwebsocket.js"
        "--preload-file" "${CMAKE_SOURCE_DIR}/assets@assets"
    )

    # WasmClient target: browser client with embedded server support
    #   No find_package for SDL2, OpenGL, GLAD, SDL2_image, SDL2_mixer
    #   Sources: wasm_client_main.cpp (or client_main.cpp with #ifdef),
    #            WebSocketTransport, InMemoryTransport,
    #            InMemoryServerTransport, ServerGameState (for embedded mode)
    #   Output: .html + .js + .wasm + .data

    # Compile definitions for WASM
    #   SPDLOG_NO_THREAD_ID - disable threading in spdlog
    #   SPDLOG_NO_TLS - disable thread-local storage

    # HTML output
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

# --- Shared source organization ---
# Transport implementations are conditionally compiled:
#   ENetTransport.cpp        - Native only (requires ENet)
#   WebSocketTransport.cpp   - WASM only (requires emscripten/websocket.h)
#   InMemoryTransport.cpp    - Both platforms
#   ENetServerTransport.cpp  - Native only
#   WebSocketServerTransport.cpp - Native only (server is always native)
#   InMemoryServerTransport.cpp  - Both platforms
