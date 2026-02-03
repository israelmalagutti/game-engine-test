#pragma once

#include "Common.h"
#include "Vector2.h"
#include <SDL2/SDL_scancode.h>

class Input {
  private:
    const Uint8* keyboardState;
    bool quitRequested;

  public:
    Input();
    
    void update();

    bool isKeyDown(SDL_Scancode key) const;
    bool isQuitRequested() const;

    Vector2 getMovementInput() const;
};
