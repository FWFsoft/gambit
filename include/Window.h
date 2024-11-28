#pragma once

#include <string>

class Window {
public:
    Window(const std::string& title, int width, int height);
    ~Window();

    void pollEvents();
    bool isOpen() const;

private:
    void* sdlWindow;
    bool open;
};
