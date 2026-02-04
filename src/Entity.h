#pragma once

#include "Camera.h"
#include "Common.h"
#include "Vector2.h"

class Camera;
class Shader;

class Entity {
protected:
  std::string name;
  Vector2 position;
  bool isActive;

public:
  // Constructor declaration
  Entity(const std::string& name, float x, float y);
  virtual ~Entity() = default;

  virtual void update(float deltaTime);
  virtual void render(Shader& shader, const Camera& camera);

  void printInfo() const;

  // Getters
  std::string getName() const;
  Vector2 getPosition() const;
  bool getIsActive() const;

  // Setters
  void setPosition(const Vector2& newPosition);
  void setActive(bool active);
};
