#pragma once

#include <string>
#include <unordered_map>

#include "Animation.h"
#include "Common.h"
#include "Sprite.h"
#include "Texture.h"

class SpriteAnimator{
  private:
    Sprite* sprite;
    Texture* texture;

    int frameWidth, frameHeight;
    float uvWidth, uvHeight;
    
    const Animation* currentAnimation;
    int currentFrame;

    float ellapsedTime;

    std::unordered_map<std::string, Animation> animations;

    void updateUv();

  public:
    SpriteAnimator(Sprite* sprite, Texture* texture, int frameWidth, int frameHeight);

    // Lifecycle
    void play(const std::string& name);
    void update(float deltaTime);

    void addAnimation(const Animation& anim);
};
