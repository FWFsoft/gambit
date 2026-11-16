#include "Window.h"

#include <SDL2/SDL.h>
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include "ShaderCompat.h"

#include <cassert>
#include <stdexcept>

#include "EventBus.h"
#include "Logger.h"
#include "PadState.h"

namespace {
// Stick / trigger travel as -1..1 (triggers 0..1).
float normalizeAxis(Sint16 raw) {
  return raw < 0 ? raw / 32768.0f : raw / 32767.0f;
}
}  // namespace

Window::Window(const std::string& title, int width, int height) : open(true) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) !=
      0) {
    throw std::runtime_error("Failed to initialize SDL: " +
                             std::string(SDL_GetError()));
  }

  // Set OpenGL attributes before creating the window
#ifdef __EMSCRIPTEN__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  sdlWindow = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, width, height,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);

  if (!sdlWindow) {
    throw std::runtime_error("Failed to create SDL window: " +
                             std::string(SDL_GetError()));
  }

  // Create OpenGL context
  glContext = SDL_GL_CreateContext(sdlWindow);
  if (!glContext) {
    throw std::runtime_error("Failed to create OpenGL context: " +
                             std::string(SDL_GetError()));
  }

  // Enable VSync
  SDL_GL_SetSwapInterval(1);

#ifndef __EMSCRIPTEN__
  // Initialize GLAD (not needed on Emscripten — WebGL provides GL directly)
  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    throw std::runtime_error("Failed to initialize GLAD");
  }
#endif

  // Subscribe to swap buffers event
  EventBus::instance().subscribe<SwapBuffersEvent>(
      [this](const SwapBuffersEvent& e) { SDL_GL_SwapWindow(sdlWindow); });
}

Window::~Window() {
  shutdownImGui();
  if (controller) {
    SDL_GameControllerClose(controller);
    controller = nullptr;
  }
  if (glContext) {
    SDL_GL_DeleteContext(glContext);
  }
  if (sdlWindow) {
    SDL_DestroyWindow(sdlWindow);
  }
  SDL_Quit();
}

SDL_Window* Window::getWindow() const { return sdlWindow; }

void Window::setPosition(int x, int y) {
  SDL_SetWindowPosition(sdlWindow, x, y);
}

void Window::bindController(int index) {
  assert(index >= -1);
  dropController();
  controllerIndex = index;
  controllerEverBound = false;
  refreshController();
}

void Window::dropController() {
  if (!controller) return;
  SDL_GameControllerClose(controller);
  controller = nullptr;
  controllerInstance = -1;
  // Whatever was held on it is released.
  EventBus::instance().publish(ControllerDisconnectedEvent{});
}

void Window::openController(int device) {
  assert(!controller);
  controller = SDL_GameControllerOpen(device);
  if (!controller) {
    Logger::error("Could not open controller: " + std::string(SDL_GetError()));
    return;
  }
  controllerEverBound = true;
  controllerInstance =
      SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
  // Player light on pads that have one: 1 for the first client, 2 for the
  // second.
  SDL_GameControllerSetPlayerIndex(controller, controllerIndex);
  const char* name = SDL_GameControllerName(controller);
  Logger::info("Controller " + std::to_string(controllerIndex) +
               " bound: " + std::string(name ? name : "unknown"));
}

void Window::refreshController(int addedDevice) {
  if (controller && !SDL_GameControllerGetAttached(controller)) {
    Logger::info("Controller disconnected");
    dropController();
  }

  if (!controller && controllerIndex >= 0) {
    if (!controllerEverBound) {
      // First bind: the Nth device that SDL recognises as a game controller.
      int seen = 0;
      for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (!SDL_IsGameController(i)) continue;
        if (seen++ != controllerIndex) continue;
        openController(i);
        break;
      }
    } else if (addedDevice >= 0 && SDL_IsGameController(addedDevice)) {
      // We lost ours: take the one that was just plugged in. Counting to
      // the Nth again could pick up the other player's controller.
      openController(addedDevice);
    }
  }

  PadState::bound = controller != nullptr;
  if (!controller) PadState::active = false;

  // Menus are navigated by this client's own controller only, never by
  // whichever one ImGui would find first.
  if (imguiReady) {
    ImGui_ImplSDL2_SetGamepadMode(ImGui_ImplSDL2_GamepadMode_Manual,
                                  controller ? &controller : nullptr,
                                  controller ? 1 : 0);
  }
}

void Window::pollEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // Let ImGui handle events first
    ImGui_ImplSDL2_ProcessEvent(&event);

    // Check if ImGui wants to capture this event
    ImGuiIO& io = ImGui::GetIO();
    bool imguiWantsKeyboard = io.WantCaptureKeyboard;

    if (event.type == SDL_QUIT) {
      open = false;
    } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
      // Ignore key repeat events (held keys) — InputSystem tracks key state
      // Always allow ESC and I for menu navigation
      bool isMenuKey = (event.key.keysym.sym == SDLK_ESCAPE ||
                        event.key.keysym.sym == SDLK_i);

      if (isMenuKey || !imguiWantsKeyboard) {
        EventBus::instance().publish(KeyDownEvent{event.key.keysym.sym});
      }
    } else if (event.type == SDL_KEYUP && !imguiWantsKeyboard) {
      EventBus::instance().publish(KeyUpEvent{event.key.keysym.sym});
    } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
      refreshController(event.cdevice.which);  // device index
    } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
      refreshController();
    } else if (event.type == SDL_CONTROLLERBUTTONDOWN ||
               event.type == SDL_CONTROLLERBUTTONUP) {
      if (controller && event.cbutton.which == controllerInstance) {
        EventBus::instance().publish(ControllerButtonEvent{
            event.cbutton.button, event.type == SDL_CONTROLLERBUTTONDOWN});
      }
    } else if (event.type == SDL_CONTROLLERAXISMOTION) {
      if (controller && event.caxis.which == controllerInstance) {
        EventBus::instance().publish(ControllerAxisEvent{
            event.caxis.axis, normalizeAxis(event.caxis.value)});
      }
    } else if (event.type == SDL_MOUSEMOTION) {
      EventBus::instance().publish(
          MouseMoveEvent{event.motion.x, event.motion.y});
    } else if (event.type == SDL_MOUSEBUTTONDOWN) {
      // Clicks on UI (infusion panel, menus) must not fire abilities.
      if (!io.WantCaptureMouse) {
        EventBus::instance().publish(MouseButtonEvent{
            event.button.button, true, event.button.x, event.button.y});
      }
    } else if (event.type == SDL_MOUSEBUTTONUP) {
      // Always deliver releases so a held button can never get stuck.
      EventBus::instance().publish(MouseButtonEvent{
          event.button.button, false, event.button.x, event.button.y});
    }
  }
}

bool Window::isOpen() const { return open; }

void Window::close() { open = false; }

void Window::initImGui() {
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();

  // Disable imgui.ini file (we'll handle settings ourselves)
  io.IniFilename = nullptr;

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForOpenGL(sdlWindow, glContext);
  ImGui_ImplOpenGL3_Init(GAMBIT_GLSL_VERSION_IMGUI);  // Cross-platform GL version

  // Setup Dear ImGui style (dark theme)
  ImGui::StyleColorsDark();

  imguiReady = true;
  refreshController();

  Logger::info("ImGui initialized");
}

void Window::shutdownImGui() {
  imguiReady = false;
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
}
