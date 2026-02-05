#include "Input.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_scancode.h>

Input::Input() {
  keyboardState = SDL_GetKeyboardState(nullptr);
  quitRequested = false;
  windowResized = false;
  newWindowWidth = 0;
  newWindowHeight = 0;
}

void Input::update() {
  SDL_Event event;
  windowResized = false;
  keysJustPressed.clear();

  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      quitRequested = true;
    }

    if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        quitRequested = true;
      }

      if (!event.key.repeat) {
        keysJustPressed.insert(event.key.keysym.sym);
      }
    }

    if (event.type == SDL_WINDOWEVENT) {
      if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
        windowResized = true;
        newWindowWidth = event.window.data1;
        newWindowHeight = event.window.data2;
      }
    }
  }

  // Update keyboard state
  keyboardState = SDL_GetKeyboardState(nullptr);
}

bool Input::isKeyDown(SDL_Scancode key) const {
  return keyboardState[key] != 0;
}

bool Input::wasKeyPressed(SDL_Keycode key) const {
  return keysJustPressed.count(key) > 0;
}

bool Input::isQuitRequested() const {
  return quitRequested;
}

bool Input::wasWindowResized() const {
  return windowResized;
}

int Input::getNewWindowWidth() const {
  return newWindowWidth;
}

int Input::getNewWindowHeight() const {
  return newWindowHeight;
}

Vector2 Input::getMovementInput() const {
  Vector2 movement(0, 0);

  if (keyboardState[SDL_SCANCODE_W] || keyboardState[SDL_SCANCODE_UP]) {
    movement.y -= 1.0f;
  }

  if (keyboardState[SDL_SCANCODE_S] || keyboardState[SDL_SCANCODE_DOWN]) {
    movement.y += 1.0f;
  }

  if (keyboardState[SDL_SCANCODE_A] || keyboardState[SDL_SCANCODE_LEFT]) {
    movement.x -= 1.0f;
  }

  if (keyboardState[SDL_SCANCODE_D] || keyboardState[SDL_SCANCODE_RIGHT]) {
    movement.x += 1.0f;
  }

  // Normalize sp diagonal movement isn't faster
  if (movement.length() > 0) {
    movement = movement.normalized();
  }

  return movement;
}
