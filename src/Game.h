#pragma once

#include "Common.h"
#include "Enemy.h"
#include "Input.h"
#include "Player.h"
#include "Shader.h"
#include "Texture.h"
#include "Window.h"
#include <memory>

class Game {
private:
  std::unique_ptr<Input> input;
  std::unique_ptr<Shader> spriteShader;
  std::unique_ptr<Window> window;

  std::unique_ptr<Texture> playerTexture;
  std::unique_ptr<Texture> enemyTexture;

  std::unique_ptr<Player> player;
  std::vector<std::unique_ptr<Enemy>> enemies;

  bool isRunning;
  Uint32 lastFrameTime;

  void processInput();
  void update(float deltaTime);
  void render();

  void spawnEnemy(const std::string& name, float x, float y, int damage, float speed);
  void removeDeadEntities();

public:
  Game();
  ~Game();

  void run();
  void stop();
  bool getIsRunning() const;
};
