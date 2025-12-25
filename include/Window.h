#pragma once

#include <SDL2/SDL.h>

#include <string>

class Window {
 public:
  Window(const std::string& title, int width, int height);
  ~Window();

  void pollEvents();
  bool isOpen() const;

  // Accessor for the SDL window (needed for OpenGL swap)
  SDL_Window* getWindow() const;

 private:
  SDL_Window* sdlWindow;
  SDL_GLContext glContext;
  bool open;
};
