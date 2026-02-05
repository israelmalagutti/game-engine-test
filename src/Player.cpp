#include "Player.h"

Player::Player(float x, float y, Texture* texture) : Entity("Player", x, y) {
  health = 100;
  maxHealth = 100;
  speed = 200.0f; // Pixels per second
  velocity = Vector2(0, 0);

  sprite = std::make_unique<Sprite>(texture);
  sprite->setSize(Vector2(64, 64));
}

void Player::update(float deltaTime) {
  if (!isActive) return;

  position = position + velocity * speed * deltaTime;
  velocity = Vector2(0, 0);

  // Update sprite position to match entity
  sprite->setPosition(position);
}

void Player::render(Shader& shader, const Camera& camera) {
  if (!isActive) return;
  sprite->draw(shader, camera);
}

void Player::setPosition(const Vector2& pos) {
  position = pos;
  sprite->setPosition(position);
}

void Player::move(const Vector2& direction) {
  velocity = direction;
}

void Player::takeDamage(int damage) {
  health -= damage;
  std::cout << name << "Player took " << damage << " damage!" << std::endl;

  if (health <= 0) {
    health = 0;
    isActive = false;
    std::cout << "Player has died!" << std::endl;
  }
}

void Player::heal(int amount) {
  health += amount;
  if (health > maxHealth) {
    health = maxHealth;
  }
}

int Player::getHealth() const {
  return health;
}

float Player::getSpeed() const {
  return speed;
}

bool Player::isDead() const {
  return health <= 0;
}
