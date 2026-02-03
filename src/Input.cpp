#include "Input.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_scancode.h>

Input::Input() {
  keyboardState = SDL_GetKeyboardState(nullptr);
  quitRequested = false;
}

void Input::update() {
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      quitRequested = true;
    }
  
    if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        quitRequested = true;
      }
    }
  }

  // Update keyboard state
  keyboardState = SDL_GetKeyboardState(nullptr);
}

bool Input::isKeyDown(SDL_Scancode key) const {
  return keyboardState[key] != 0;
}

bool Input::isQuitRequested() const {
  return quitRequested;
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
