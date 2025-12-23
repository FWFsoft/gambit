#include "Window.h"

#include <SDL2/SDL.h>

#include <stdexcept>

#include "EventBus.h"

Window::Window(const std::string& title, int width, int height) : open(true) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    throw std::runtime_error("Failed to initialize SDL: " +
                             std::string(SDL_GetError()));
  }

  sdlWindow =
      SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);

  if (!sdlWindow) {
    throw std::runtime_error("Failed to create SDL window: " +
                             std::string(SDL_GetError()));
  }

  renderer = SDL_CreateRenderer(
      sdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    throw std::runtime_error("Failed to create SDL renderer: " +
                             std::string(SDL_GetError()));
  }
}

Window::~Window() {
  if (renderer) {
    SDL_DestroyRenderer(renderer);
  }
  if (sdlWindow) {
    SDL_DestroyWindow((SDL_Window*)sdlWindow);
  }
  SDL_Quit();
}

SDL_Renderer* Window::getRenderer() const { return renderer; }

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
