#include "SpriteAnimator.h"
#include "Animation.h"

SpriteAnimator::SpriteAnimator(
    Sprite* sprite,
    Texture* texture,
    int frameWidth,
    int frameHeight
)
  : sprite(sprite),
    texture(texture),
    frameWidth(frameWidth),
    frameHeight(frameHeight),
    currentAnimation(nullptr),
    currentFrame(0),
    ellapsedTime(0.0f)
{
  this->uvWidth =  static_cast<float>(frameWidth)  / texture->getWidth();
  this->uvHeight = static_cast<float>(frameHeight) / texture->getHeight();

  updateUv();
}

void SpriteAnimator::play(const std::string& name) {
  auto it = animations.find(name);
  if (it == animations.end()) return;

  const Animation* newAnimation = &it->second;

  // If already playing this animation; Do not restart it
  if (currentAnimation == newAnimation) return;

  currentAnimation = newAnimation;
  currentFrame      = 0;
  ellapsedTime      = 0.0f;
  updateUv();
}

void SpriteAnimator::update(float deltaTime) {
  if (!currentAnimation) return;
  if (currentAnimation->frameCount <= 1) return;

  ellapsedTime += deltaTime;

  while (ellapsedTime >= currentAnimation->frameDuration) {
    ellapsedTime -= currentAnimation->frameDuration;
    currentFrame++;

    if (currentFrame >= currentAnimation->frameCount) {
      if (currentAnimation->loop) {
        currentFrame = 0;
      } else {
        currentFrame = currentAnimation->frameCount - 1;
        ellapsedTime = 0.0f;
        break;
      }
    }
  }

  updateUv();
}

void SpriteAnimator::updateUv() {
  if (!currentAnimation || !sprite) return;

  int column = currentFrame;
  int row = currentAnimation->row;

  float uvX = column * uvWidth;
  float uvY = row * uvHeight;

  sprite->setUVRegion(Vector2(uvX, uvY), Vector2(uvWidth, uvHeight));
}

void SpriteAnimator::addAnimation(const Animation& animation) {
  animations[animation.name] = animation;
}
