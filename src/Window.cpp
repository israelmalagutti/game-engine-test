#include "Window.h"
#include "Common.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_video.h>
#include <iostream>
#include <ostream>
#include <type_traits>

Window::Window(const std::string& title, int width, int height) {
  this->title = title;
  this->width = width;
  this->height = height;
  this->open = true;
  this->window = nullptr;
  this->glContext = nullptr;

  // Initialize SDL
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "Failed to Initialize SDL: " << SDL_GetError() << std::endl;
    open = false;
    return;
  }

  // Set OpenGL version
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  // Enable double buffering
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  // Create window with OpenGL support
  window = SDL_CreateWindow(
      title.c_str(),
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      width,
      height,
      SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
  );

  if (!window) {
    std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
    open = false;
    return;
  }

  // Create OpenGL context
  glContext = SDL_GL_CreateContext(window);
  if (!glContext) {
    std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
    open = false;
    return;
  }

  // Initialize GLEW
  glewExperimental = GL_TRUE;
  GLenum glewError = glewInit();
  if (glewError != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewError) << std::endl;
    open = false;
    return;
  }

  // Enable VSync
  SDL_GL_SetSwapInterval(1);

  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Set viewport
  glViewport(0, 0, width, height);

  std::cout << "Window created successfully!" << std::endl;
  std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
}

Window::~Window() {
  if (glContext) {
    SDL_GL_DeleteContext(glContext);
  }

  if (window) {
    SDL_DestroyWindow(window);
  }

  SDL_Quit();
  std::cout << "Window destroyed." << std::endl;
}

void Window::swapBuffers() {
  SDL_GL_SwapWindow(window);
}

void Window::clear(float r, float g, float b, float a) {
  glClearColor(r, g, b, a);
  glClear(GL_COLOR_BUFFER_BIT);
}

bool Window::isOpen() const {
  return open;
}

void Window::close() {
  open = false;
}

int Window::getWidth() const {
  return width;
}

int Window::getHeight() const {
  return height;
}

SDL_Window* Window::getSDLWindow() const {
  return window;
}
