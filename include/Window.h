#pragma once

#include <SDL2/SDL.h>

#include <string>

class Window {
 public:
  Window(const std::string& title, int width, int height);
  ~Window();

  void pollEvents();
  bool isOpen() const;
  void close();

  // Accessor for the SDL window (needed for OpenGL swap)
  SDL_Window* getWindow() const;

  // Initialize/shutdown ImGui (must be called after OpenGL context creation)
  void initImGui();
  void shutdownImGui();

 private:
  SDL_Window* sdlWindow;
  SDL_GLContext glContext;
  bool open;
};
