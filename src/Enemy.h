#pragma once

#include "Camera.h"
#include "Entity.h"
#include "Sprite.h"
#include "Shader.h"

struct EnemySpawn {
  std::string name;
  float x, y;
  int damage;
  float speed;
};

class Enemy : public Entity {
private:
  int damage;
  float speed;
  Vector2 targetPosition;
  std::unique_ptr<Sprite> sprite;

public:
  Enemy(const std::string& name, float x, float y, int damage, float speed, Texture* texture);

  void update(float deltaTime) override;
  void render(Shader& shader, const Camera& camera) override;

  void setTarget(const Vector2& targetPos);
  
  // Getters
  int getDamage() const;
  float getSpeed() const;
};
