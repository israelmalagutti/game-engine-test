#pragma once

#include "Entity.h"
#include "Sprite.h"
#include "Shader.h"

class Enemy : public Entity {
private:
  int damage;
  float speed;
  Vector2 targetPosition;
  std::unique_ptr<Sprite> sprite;

public:
  Enemy(const std::string& name, float x, float y, int damage, float speed, Texture* texture);

  void update(float deltaTime) override;
  void render(Shader& shader, int screenWidth, int screenHeight);

  void setTarget(const Vector2& targetPos);
  
  // Getters
  int getDamage() const;
  float getSpeed() const;
};
