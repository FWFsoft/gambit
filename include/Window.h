#pragma once

#include <string>
#include <SDL2/SDL.h>

class Window {
public:
    Window(const std::string& title, int width, int height);
    ~Window();

    void pollEvents();
    bool isOpen() const;

    // Accessor for the renderer
    SDL_Renderer* getRenderer() const;

private:
    SDL_Window* sdlWindow;
    SDL_Renderer* renderer;
    bool open;
};
