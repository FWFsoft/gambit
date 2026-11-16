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

  // Which game controller drives this client: the Nth connected controller
  // (0 = first), or -1 for none (the default). Two clients on one machine
  // bind 0 and 1. Call before initImGui.
  //
  // If the controller is not connected yet it is picked up when it appears.
  // If it is lost later, the client takes the next controller that is
  // plugged in, never one that is already connected: that one may be
  // another client's.
  void bindController(int index);

  // Move the window (screen coordinates), for side-by-side clients.
  void setPosition(int x, int y);

 private:
  SDL_Window* sdlWindow;
  SDL_GLContext glContext;
  bool open;
  bool imguiReady = false;
  int controllerIndex = -1;
  bool controllerEverBound = false;
  SDL_GameController* controller = nullptr;
  SDL_JoystickID controllerInstance = -1;

  // Drop the controller if it was unplugged; open one if we have none.
  // addedDevice is the SDL device index of a controller that was just
  // plugged in, or -1.
  void refreshController(int addedDevice = -1);
  void openController(int device);
  void dropController();
};
