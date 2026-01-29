#include <iostream>

struct Vector2 {
  float x;
  float y;
};

class Player {
private:
  Vector2 position;
  float speed;
  int health;

public:
  // Constructor - runs when you create a Player
  Player(float startX, float startY, float startSpeed) {
    position.x = startX;
    position.y = startY;
    speed = startSpeed;
    health = 100;
  }

  void move(float deltaX, float deltaY) {
    position.x += deltaX * speed;
    position.y += deltaY * speed;
  }

  void printStatus() {
    std::cout << "Position: (" << position.x << ", " << position.y << ")" << std::endl;
    std::cout << "Healt: " << health << std::endl;
  }

  void takeDamage(int damage) {
    health -= damage;
    if (health <= 0) {
      health = 0;
    }
  }

  int getHealth () {
    return health;
  }
};
