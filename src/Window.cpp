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

#include <stdexcept>

#include "EventBus.h"
#include "Logger.h"

Window::Window(const std::string& title, int width, int height) : open(true) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
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
  if (glContext) {
    SDL_GL_DeleteContext(glContext);
  }
  if (sdlWindow) {
    SDL_DestroyWindow(sdlWindow);
  }
  SDL_Quit();
}

SDL_Window* Window::getWindow() const { return sdlWindow; }

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

  Logger::info("ImGui initialized");
}

void Window::shutdownImGui() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
}
