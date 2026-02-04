#include "Enemy.h"

Enemy::Enemy(const std::string& name, float x, float y, int damage, float speed, Texture* texture)
    : Entity(name, x, y) {
    this->damage = damage;
    this->speed = speed;
    this->targetPosition = Vector2(0, 0);

    sprite = std::make_unique<Sprite>(texture);
    sprite->setSize(Vector2(48, 48));
}

void Enemy::update(float deltaTime) {
  if (!isActive) return;

  // Move toward origin
  Vector2 direction = targetPosition - position;
  float  distance = direction.length();

  // Only move if we're not already at the target
  if (distance > 1.0f) {
    // Normalize direction and move
    Vector2 normalized = direction.normalized();
    position = position + normalized * speed * deltaTime;
    sprite->setPosition(position);
  }
}

void Enemy::render(Shader& shader, const Camera& camera) {
  if (!isActive) return;
  sprite->draw(shader, camera);
}

void Enemy::setTarget(const Vector2& targetPos) {
  targetPosition = targetPos;
}

int Enemy::getDamage() const {
  return damage;
}

float Enemy::getSpeed() const {
  return speed;
}
