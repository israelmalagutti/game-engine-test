#include "Game.h"
#include <iterator>

// Member initializer list- initializes player and enemy before constructor body runs
Game::Game()
  : player(100.0f, 100.0f),
    enemy("Goblin", 50.0f, 40.0f, 15)

{
  isRunning = false;
  frameCount = 0;
  std::cout << "=== Game Initialized ===" << std::endl;
}

void Game::run() {
  isRunning = true;
  std::cout << "=== Game Starting ===" << std::endl;

  while (isRunning) {
    frameCount++;
    std::cout << "\n--- Frame " << frameCount << " ---" << std::endl;

    processInput();
    update();
    render();

    if (frameCount >= 5) {
      stop();
    }
  }

  std::cout << "\n=== Game Endend ===" << std::endl;
  std::cout << "Total frames: " << frameCount << std::endl;
}

void Game::stop() {
  isRunning = false;
}

void Game::processInput() {
  player.move(Vector2(0.2f, 0.0f));
}

void Game::update() {
  player.update();
  enemy.update();

  if (!player.getIsActive()) {
    std::cout << "Game over!" << std::endl;
    stop();
  }
}

void Game::render() {
  player.render();
  enemy.render();
}

bool Game::getIsRunning() const {
  return isRunning;
}
