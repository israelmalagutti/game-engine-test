#include <iostream>

struct Vector2 {
  float x;
  float y;
};

struct Player {
  Vector2 position;
  float speed;
  int health;
};

int main() {
  bool isOpen = true;
  int frameCount = 0;

  Player player;
  player.position.x = 100.0f;
  player.position.y = 200.0f;
  player.speed = 5.0f;
  player.health = 100;


  std::cout << "Player at (" << player.position.x << ", " << player.position.y << ")" << std::endl;
  std::cout << "Health: " << player.health << std::endl;

  while (isOpen) {
    // Input

    // Update
    frameCount++;

    // Render
    std::cout << "Frame: " << frameCount << std::endl;

    // Stop after 10 frames
    if (frameCount >= 10) {
      isOpen = false;
    }
  }

  std::cout << "Game ended!" << std::endl;
  return 0;
}
