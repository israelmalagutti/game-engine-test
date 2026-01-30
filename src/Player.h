#pragma once

#include "Entity.h"

class Player : public Entity {
private:
  int health;
  int maxHealth;
  float speed;

public:
  Player(float x, float y);

  // Override parent methods
  void update() override;
  void render() override;

  // Player-specific methods
  void move(const Vector2& direction);
  void takeDamage(int damage);
  void heal(int amount);

  // Getters
  int getHealth() const;
  float getSpeed() const;
};
