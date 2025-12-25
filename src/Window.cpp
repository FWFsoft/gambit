#include "Window.h"

#include <SDL2/SDL.h>
#include <glad/glad.h>

#include <stdexcept>

#include "EventBus.h"

Window::Window(const std::string& title, int width, int height) : open(true) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    throw std::runtime_error("Failed to initialize SDL: " +
                             std::string(SDL_GetError()));
  }

  // Set OpenGL attributes before creating the window
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
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

  // Initialize GLAD
  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    throw std::runtime_error("Failed to initialize GLAD");
  }
}

Window::~Window() {
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
    if (event.type == SDL_QUIT) {
      open = false;
    } else if (event.type == SDL_KEYDOWN) {
      EventBus::instance().publish(KeyDownEvent{event.key.keysym.sym});
    } else if (event.type == SDL_KEYUP) {
      EventBus::instance().publish(KeyUpEvent{event.key.keysym.sym});
    }
  }
}

bool Window::isOpen() const { return open; }
