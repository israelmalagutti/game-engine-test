#pragma once

#include "Camera.h"
#include "Entity.h"
#include "Sprite.h"
#include "Shader.h"
#include "SpriteAnimator.h"
#include <memory>

class Player : public Entity {
private:
  int health;
  int maxHealth;

  float speed;


  Vector2 velocity;
  std::unique_ptr<Sprite> sprite;
  std::unique_ptr<SpriteAnimator> animator;

public:
  Player(float x, float y, Texture* texture);

  // Override parent methods
  void update(float deltaTime) override;
  void render(Shader& shader, const Camera& camera) override;

  void setPosition(const Vector2& pos);
  void move(const Vector2& direction);
  void takeDamage(int damage);
  void heal(int amount);

  // Getters
  int getHealth() const;
  int getMaxHealth() const;
  float getSpeed() const;
  bool isDead() const;
};
