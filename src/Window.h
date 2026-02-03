#pragma once

#include "Common.h"
#include <SDL2/SDL_video.h>

class Window {
  private:
    SDL_Window* window;
    SDL_GLContext glContext;

    int width;
    int height;
    
    bool open;
    std::string title;

public:
    Window(const std::string& title, int width, int height);
    ~Window();

    void swapBuffers();
    void clear(float r, float g, float b, float a = 1.0f);

    bool isOpen() const;
    void close();

    void handleResize(int newWidth, int newHeight);

    // Getters
    int getWidth() const;
    int getHeight() const;

    SDL_Window* getSDLWindow() const;
};
