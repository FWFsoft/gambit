#include "Window.h"
#include <SDL2/SDL.h>
#include <stdexcept>

Window::Window(const std::string& title, int width, int height) : open(true) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        throw std::runtime_error("Failed to initialize SDL: " + std::string(SDL_GetError()));
    }

    sdlWindow = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN
    );

    if (!sdlWindow) {
        throw std::runtime_error("Failed to create SDL window: " + std::string(SDL_GetError()));
    }
}

Window::~Window() {
    if (sdlWindow) {
        SDL_DestroyWindow((SDL_Window*)sdlWindow);
    }
    SDL_Quit();
}

void Window::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            open = false;
        }
    }
}

bool Window::isOpen() const {
    return open;
}

