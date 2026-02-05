#pragma once

#include <map>
#include <memory>

#include "Camera.h"
#include "Common.h"
#include "Enemy.h"
#include "Input.h"
#include "Location.h"
#include "Player.h"
#include "Shader.h"
#include "Texture.h"
#include "Vector2.h"
#include "Window.h"

class Game {
private:
  std::unique_ptr<Input> input;
  std::unique_ptr<Window> window;
  std::map<std::string, std::unique_ptr<Location>> locations;

  std::unique_ptr<Shader> tileShader;
  std::unique_ptr<Shader> spriteShader;

  std::unique_ptr<Texture> tilesetTexture;
  std::unique_ptr<Texture> playerTexture;
  std::unique_ptr<Texture> enemyTexture;

  std::unique_ptr<Player> player;
  std::vector<std::unique_ptr<Enemy>> enemies;

  std::unique_ptr<Camera> camera;
  Location* currentLocation;

  // Game loop
  bool isRunning;
  Uint32 lastFrameTime;

  void processInput();
  void update(float deltaTime);
  void render();

  // Location
  bool locationChangeRequested = false;
  std::string pendingLocationId;
  Vector2 pendingSpawnPosition;
  
  void setupLocations();
  void changeLocation();
  void changeLocationRequest(const std::string& id, const Vector2& spawnPos);
  void checkWarpCollisions();

  // Entities / Sprites
  void spawnEnemy(const std::string& name, float x, float y, int damage, float speed);
  void removeDeadEntities();

public:
  Game();
  ~Game();

  void run();
  void stop();
  bool getIsRunning() const;
};
