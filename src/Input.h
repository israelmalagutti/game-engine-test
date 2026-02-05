#pragma once

#include "Common.h"
#include "Vector2.h"
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_scancode.h>
#include <unordered_set>

class Input {
  private:
    const Uint8* keyboardState;
    bool quitRequested;
    bool windowResized;
    int newWindowWidth;
    int newWindowHeight;
    std::unordered_set<SDL_Keycode> keysJustPressed;

  public:
    Input();

    void update();

    bool isKeyDown(SDL_Scancode key) const;
    bool wasKeyPressed(SDL_Keycode key) const;
    bool isQuitRequested() const;
    bool wasWindowResized() const;
    int getNewWindowWidth() const;
    int getNewWindowHeight() const;

    Vector2 getMovementInput() const;
};
