#pragma once

#include "Player.h"
#include "Enemy.h"

class Game {
private:
  void processInput();
  void update();
  void render();

  bool isRunning;
  int frameCount;

  // Game objects
  Player player;
  Enemy enemy;
public:
  Game();
  
  void run();
  void stop();

  bool getIsRunning() const;
};
