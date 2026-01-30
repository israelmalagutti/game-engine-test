#pragma once

#include "Entity.h"

class Enemy : public Entity {
private:
  int damage;

public:
  Enemy(const std::string& name, float x, float y, int damage);

  void update() override;
  void render() override;

  int getDamage() const;
};
