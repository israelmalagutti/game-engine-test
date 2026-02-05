#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_timer.h>
#include <algorithm>
#include <memory>

#include "Game.h"
#include "Camera.h"
#include "Tilemap.h"

Game::Game() {
  isRunning = false;

  //  Create window
  window = std::make_unique<Window>("Game Engine", 1920, 1080);
  if (!window->isOpen()) {
    std::cerr << "Failed to create window!" << std::endl;
    return;
  }

  // Create camera
  camera = std::make_unique<Camera>(1920, 1080);
  camera->setWorldBounds(0.0f, 0.0f, 2400.0f, 1280.0f);

  // Create shaders
  tileShader = std::make_unique<Shader>("shaders/tile.vert", "shaders/tile.frag");
  spriteShader = std::make_unique<Shader>("shaders/sprite.vert", "shaders/sprite.frag");

  // Create input handler
  input = std::make_unique<Input>();

  // Load Textures
  tilesetTexture = std::make_unique<Texture>("assets/tile.png");

  playerTexture = std::make_unique<Texture>("assets/player.png");
  enemyTexture = std::make_unique<Texture>("assets/enemy.png");

  // Create player
  tilemap = std::make_unique<Tilemap>(10, 10, 32, tilesetTexture.get());
  player = std::make_unique<Player>(400.0f, 300.0f, playerTexture.get());

  std::cout << "=== Game Intiliazed ===" << std::endl;
};

Game::~Game() {
  std::cout << "=== Game destroyed ===" << std::endl;
}

void Game::spawnEnemy(const std::string& name, float x, float y, int damage, float speed) {
    // Create a new enemy and add it to our list
    auto enemy = std::make_unique<Enemy>(name, x, y, damage, speed, enemyTexture.get());
    // Move the enemy into our vector
    enemies.push_back(std::move(enemy));
    std::cout << "Spawned: " << name << " at (" << x << ", " << y << ")" << std::endl;
}

void Game::removeDeadEntities() {
  // Remove all interactive enemies using erase-remove idiom
  enemies.erase(
      std::remove_if(
        enemies.begin(),
        enemies.end(),
        [](const std::unique_ptr<Enemy>& e) {
          return !e->getIsActive();
        }
      ),

      enemies.end()
  );
}

void Game::run() {
  isRunning = true;
  lastFrameTime = SDL_GetTicks();

  // Spawn some enemies
  spawnEnemy("Goblin", 100.0f, 100.0f, 10, 30.0f);
  spawnEnemy("Orc", 700.0f, 500.0f, 25, 20.0f);
  spawnEnemy("Skeleton", 50.0f, 400.0f, 15, 40.0f);

  while (window->isOpen() && isRunning) {
    // Calculate delta time
    Uint32 currentTime = SDL_GetTicks();
    float deltaTime = (currentTime - lastFrameTime) / 1000.f;
    lastFrameTime = currentTime;

    if (deltaTime > 0.1f) {
      deltaTime = 0.1f;
    }

    processInput();
    update(deltaTime);
    render();
  }
}

void Game::stop() {
  isRunning = false;
}

void Game::processInput() {
  input->update();

  if (input->isQuitRequested()) {
    stop();
    return;
  }

  if (input->wasWindowResized()) {
    int newWidth = input->getNewWindowWidth();
    int newHeight = input->getNewWindowHeight();

    window->handleResize(newWidth, newHeight);
    camera->setViewportSize(newWidth, newHeight);
  }

  // Get movement from WASD/Arrow keys
  Vector2 movement = input->getMovementInput();
  player->move(movement);
}

void Game::update(float deltaTime) {
  // Hot-reload shaders
  tileShader->checkReload();
  spriteShader->checkReload();

  // Hot-reload textures
  tilesetTexture->checkReload();
  playerTexture->checkReload();
  enemyTexture->checkReload();

  player->update(deltaTime);

  // Update all enemies and make them chase the player
  for (auto& enemy : enemies) {
    enemy->setTarget(player->getPosition());
    enemy->update(deltaTime);
  }

  // Center camera on player (with boundary clamping)
  camera->centerOn(player->getPosition());

  // Clean up dead enemies
  removeDeadEntities();

  if (player->isDead()) {
    std::cout << "GAME OVER!" << std::endl;
    stop();
  }
}

void Game::render() {
  window->clear(0.1f, 0.1f, 0.2f);
  tilemap->render(*tileShader, *camera);

  // Draw all enemies
  for (auto& enemy : enemies) {
    enemy->render(*spriteShader, *camera);
  }

  // Draw player on top
  player->render(*spriteShader, *camera);
  window->swapBuffers();
}

bool Game::getIsRunning() const {
  return isRunning;
}
