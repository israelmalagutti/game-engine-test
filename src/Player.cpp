#include "Player.h"
#include "Vector2.h"
#include <ostream>

Player::Player(float x, float y) : Entity("Player", x, y) {
  health = 100;
  maxHealth = 100;
  speed = 5.0f;
}

void Player::update() {
  if (!isActive) return;

  // Player update logic would go here
  // (input handling, physics, etc.)
}

void Player::render() {
  if (!isActive) return;

  std::cout << "[RENDER] ";
  printInfo();
  std::cout << "Health: " << health << "/" << maxHealth << std::endl;
}

void Player::move(const Vector2& direction) {
  // direction * speed gives us velocity
  // then we add it to position
  position = position + (direction * speed);
}

void Player::takeDamage(int damage) {
  health -= damage;
  std::cout << name << " took " << damage << " damage!" << std::endl;

  if (health <= 0) {
    health = 0;
    isActive = false;
    std::cout << name << " has died!" << std::endl;
  }
}

void Player::heal(int amount) {
  health += amount;
  if (health > maxHealth) {
    health = maxHealth;
  }
  std::cout << name << " healed for " << amount << "!" << std::endl;
}

int Player::getHealth() const {
  return health;
}

float Player::getSpeed() const {
  return speed;
}
