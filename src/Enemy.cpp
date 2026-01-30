#include "Enemy.h"

Enemy::Enemy(const std::string& name, float x, float y, int damage)
  : Entity(name, x, y) {
  this->damage = damage;
}

void Enemy::update() {
  if (!isActive) return;

  // Move toward origin
  float moveSpeed = 0.5f;

  if (position.x > 0) position.x -= moveSpeed;
  if (position.y > 0) position.y -= moveSpeed;

  // Clamp to zero
  if (position.x < 0) position.x = 0;
  if (position.y < 0) position.y = 0;
}

void Enemy::render() {
  if (!isActive) return;

  std::cout << " Damage: " << damage << std::endl;
}

int Enemy::getDamage() const {
  return damage;
}
