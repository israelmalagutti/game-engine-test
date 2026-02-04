#include "Entity.h"

// Constructor Definition
Entity::Entity(const std::string& name, float x, float y) {
  this->name = name;
  this->position = Vector2(x, y);
  this->isActive = true;
}

void Entity::update(float deltaTime) {
  (void)deltaTime;
}

void Entity::render(Shader& shader, const Camera& camera) {
  // Base implementation does nothing
  (void)shader;
  (void)camera;
}

void Entity::printInfo() const {
  std::cout << name << " at ";
  position.print();

  std::cout << (isActive ? " [Active]" : " [Inactive]");
  std::cout << std::endl;
}

// Getters
std::string Entity::getName() const {
  return name;
}

Vector2 Entity::getPosition() const {
  return position;
}

bool Entity::getIsActive() const {
  return isActive;
}

void Entity::setPosition(const Vector2& newPosition) {
  position = newPosition;
}

void Entity::setActive(bool active) {
  isActive = active;
}
