# Game Architecture

This document covers the overall architecture of the game engine, including the game loop, entity system, and component organization.

## Table of Contents
- [Project Structure](#project-structure)
- [The Game Loop](#the-game-loop)
- [Entity System](#entity-system)
- [Component Overview](#component-overview)
- [Data Flow](#data-flow)
- [Memory Management](#memory-management)

---

## Project Structure

```
game-engine-test/
├── src/
│   ├── main.cpp          # Entry point
│   ├── Game.cpp/h        # Main game class
│   ├── Window.cpp/h      # Window & OpenGL context
│   ├── Input.cpp/h       # Input handling
│   ├── Shader.cpp/h      # Shader compilation & usage
│   ├── Texture.cpp/h     # Texture loading
│   ├── Sprite.cpp/h      # Sprite rendering
│   ├── Entity.cpp/h      # Base entity class
│   ├── Player.cpp/h      # Player entity
│   ├── Enemy.cpp/h       # Enemy entity
│   ├── Vector2.h         # 2D vector math
│   ├── Common.h          # Common includes
│   └── glad.c            # OpenGL loader
├── include/
│   ├── glad/             # GLAD headers
│   └── KHR/              # Khronos headers
├── shaders/
│   ├── sprite.vert       # Vertex shader
│   └── sprite.frag       # Fragment shader
├── assets/
│   ├── player.png        # Player sprite
│   └── enemy.png         # Enemy sprite
├── Makefile              # Build configuration
└── .clangd               # IDE configuration
```

### Separation of Concerns

| Component | Responsibility |
|-----------|---------------|
| `Game` | Orchestrates everything, owns resources |
| `Window` | Window management, OpenGL context |
| `Input` | Event polling, keyboard state |
| `Shader` | GLSL program management |
| `Texture` | Image loading, GPU texture management |
| `Sprite` | Quad geometry, drawing |
| `Entity` | Base game object with position |
| `Player/Enemy` | Specific game object behavior |

---

## The Game Loop

The game loop is the heart of any game. It runs continuously until the game exits.

### Basic Structure

```cpp
// In Game::run()
void Game::run() {
  isRunning = true;
  lastFrameTime = SDL_GetTicks();

  while (window->isOpen() && isRunning) {
    // Calculate delta time
    Uint32 currentTime = SDL_GetTicks();
    float deltaTime = (currentTime - lastFrameTime) / 1000.0f;
    lastFrameTime = currentTime;

    // Clamp to prevent physics issues
    if (deltaTime > 0.1f) {
      deltaTime = 0.1f;
    }

    processInput();      // 1. Handle input
    update(deltaTime);   // 2. Update game state
    render();            // 3. Draw everything
  }
}
```

### The Three Phases

```
┌─────────────────────────────────────────────────────────┐
│                      GAME LOOP                          │
│                                                         │
│   ┌─────────┐    ┌─────────┐    ┌─────────┐           │
│   │  INPUT  │───▶│ UPDATE  │───▶│ RENDER  │───┐       │
│   └─────────┘    └─────────┘    └─────────┘   │       │
│        ▲                                       │       │
│        └───────────────────────────────────────┘       │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

#### 1. Process Input
```cpp
void Game::processInput() {
  input->update();  // Poll SDL events

  if (input->isQuitRequested()) {
    stop();
    return;
  }

  // Get movement and apply to player
  Vector2 movement = input->getMovementInput();
  player->move(movement);
}
```

#### 2. Update
```cpp
void Game::update(float deltaTime) {
  // Update player
  player->update(deltaTime);

  // Update all enemies
  for (auto& enemy : enemies) {
    enemy->setTarget(player->getPosition());
    enemy->update(deltaTime);
  }

  // Clean up dead entities
  removeDeadEntities();

  // Check win/lose conditions
  if (player->isDead()) {
    std::cout << "GAME OVER!" << std::endl;
    stop();
  }
}
```

#### 3. Render
```cpp
void Game::render() {
  window->clear(0.1f, 0.1f, 0.2f);  // Clear with dark blue

  int screenWidth = window->getWidth();
  int screenHeight = window->getHeight();

  // Draw enemies first (behind player)
  for (auto& enemy : enemies) {
    enemy->render(*spriteShader, screenWidth, screenHeight);
  }

  // Draw player on top
  player->render(*spriteShader, screenWidth, screenHeight);

  window->swapBuffers();  // Display the frame
}
```

### Delta Time

Delta time ensures consistent behavior regardless of frame rate:

```cpp
// Without delta time (BAD)
position.x += speed;  // Faster on faster computers!

// With delta time (GOOD)
position.x += speed * deltaTime;  // Same speed everywhere
```

### Frame Rate Clamping

```cpp
if (deltaTime > 0.1f) {
  deltaTime = 0.1f;  // Cap at 100ms (10 FPS minimum)
}
```

This prevents "spiral of death" where:
1. Frame takes too long
2. Delta time is huge
3. Physics does huge step
4. More entities collide
5. More work, frame takes even longer
6. Repeat until crash

---

## Entity System

### Base Entity Class

```cpp
class Entity {
protected:
  std::string name;
  Vector2 position;
  bool isActive;

public:
  Entity(const std::string& name, float x, float y);
  virtual ~Entity() = default;

  virtual void update(float deltaTime);
  virtual void render(Shader& shader, int screenWidth, int screenHeight);

  // Common functionality
  Vector2 getPosition() const;
  void setPosition(const Vector2& newPosition);
  bool getIsActive() const;
  void setActive(bool active);
};
```

### Derived Entities

**Player:**
```cpp
class Player : public Entity {
private:
  int health, maxHealth;
  float speed;
  Vector2 velocity;
  std::unique_ptr<Sprite> sprite;

public:
  void update(float deltaTime) override;
  void render(Shader& shader, int w, int h) override;

  void move(const Vector2& direction);
  void takeDamage(int damage);
  bool isDead() const;
};
```

**Enemy:**
```cpp
class Enemy : public Entity {
private:
  int damage;
  float speed;
  Vector2 targetPosition;
  std::unique_ptr<Sprite> sprite;

public:
  void update(float deltaTime) override;
  void render(Shader& shader, int w, int h) override;

  void setTarget(const Vector2& targetPos);
};
```

### Entity Lifecycle

```
┌──────────┐
│  Create  │  Game::spawnEnemy() / constructor
└────┬─────┘
     │
     ▼
┌──────────┐
│  Active  │  isActive = true, updated and rendered
└────┬─────┘
     │
     ▼ (health <= 0, off screen, etc.)
┌──────────┐
│ Inactive │  isActive = false, marked for removal
└────┬─────┘
     │
     ▼
┌──────────┐
│ Removed  │  Game::removeDeadEntities()
└──────────┘
```

### Entity Removal

Using the erase-remove idiom:

```cpp
void Game::removeDeadEntities() {
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
```

Why this pattern?
- `remove_if` moves inactive entities to the end
- `erase` deletes them
- Single pass through the vector
- Efficient for contiguous memory

---

## Component Overview

### Game Class

The central coordinator that owns all major systems:

```cpp
class Game {
private:
  // Systems
  std::unique_ptr<Input> input;
  std::unique_ptr<Shader> spriteShader;
  std::unique_ptr<Window> window;

  // Resources (textures are shared)
  std::unique_ptr<Texture> playerTexture;
  std::unique_ptr<Texture> enemyTexture;

  // Entities
  std::unique_ptr<Player> player;
  std::vector<std::unique_ptr<Enemy>> enemies;

  // State
  bool isRunning;
  Uint32 lastFrameTime;

public:
  void run();   // Start game loop
  void stop();  // Request exit
};
```

### Ownership Hierarchy

```
Game (owns everything)
├── Window
├── Input
├── Shader (spriteShader)
├── Texture (playerTexture)
├── Texture (enemyTexture)
├── Player
│   └── Sprite (owns)
│       └── Texture* (references playerTexture)
└── std::vector<Enemy>
    └── Enemy
        └── Sprite (owns)
            └── Texture* (references enemyTexture)
```

---

## Data Flow

### Input Flow

```
SDL Event Queue
      │
      ▼
Input::update()
      │
      ▼
Game::processInput()
      │
      ▼
Player::move(direction)
      │
      ▼
Player::velocity = direction * speed
```

### Update Flow

```
Game::update(deltaTime)
      │
      ├──▶ Player::update(deltaTime)
      │         │
      │         ▼
      │    position += velocity * deltaTime
      │
      ├──▶ Enemy::update(deltaTime)
      │         │
      │         ▼
      │    Move toward player position
      │
      └──▶ removeDeadEntities()
```

### Render Flow

```
Game::render()
      │
      ├──▶ Window::clear()
      │         │
      │         ▼
      │    glClear(COLOR_BUFFER)
      │
      ├──▶ Enemy::render() [for each]
      │         │
      │         ▼
      │    Sprite::draw()
      │         │
      │         ▼
      │    glDrawArrays()
      │
      ├──▶ Player::render()
      │         │
      │         ▼
      │    Sprite::draw()
      │
      └──▶ Window::swapBuffers()
                │
                ▼
           SDL_GL_SwapWindow()
```

---

## Memory Management

### Smart Pointer Strategy

| Ownership | Pointer Type |
|-----------|--------------|
| Exclusive ownership | `std::unique_ptr<T>` |
| Shared ownership | `std::shared_ptr<T>` (not used here) |
| Non-owning reference | `T*` (raw pointer) |

### Resource Lifetime

```cpp
// Game constructor - create resources
Game::Game() {
  window = std::make_unique<Window>(...);
  spriteShader = std::make_unique<Shader>(...);
  playerTexture = std::make_unique<Texture>(...);

  // Player references texture but doesn't own it
  player = std::make_unique<Player>(..., playerTexture.get());
}

// Game destructor - automatic cleanup
Game::~Game() {
  // unique_ptrs automatically delete in reverse order
  // Player deleted first, then textures, then window
}
```

### Why Raw Pointers for Textures?

Sprites hold `Texture*` (raw pointer) because:

1. **Textures outlive sprites**: Textures are created before sprites and destroyed after
2. **Shared resources**: Multiple sprites could use the same texture
3. **No ownership transfer**: Sprites don't delete textures
4. **Performance**: No reference counting overhead

```cpp
// Texture owned by Game
std::unique_ptr<Texture> playerTexture;

// Sprite just references it
class Sprite {
  Texture* texture;  // Non-owning
};

// Safe because:
// 1. Game creates texture
// 2. Game creates sprite with texture.get()
// 3. Game destroys sprite
// 4. Game destroys texture
```

---

## Spawning Entities

### Factory Method Pattern

```cpp
void Game::spawnEnemy(const std::string& name, float x, float y,
                      int damage, float speed) {
  auto enemy = std::make_unique<Enemy>(
    name, x, y, damage, speed, enemyTexture.get()
  );
  enemies.push_back(std::move(enemy));

  std::cout << "Spawned: " << name << " at (" << x << ", " << y << ")" << std::endl;
}
```

### Usage

```cpp
void Game::run() {
  // ...
  spawnEnemy("Goblin", 100.0f, 100.0f, 10, 30.0f);
  spawnEnemy("Orc", 700.0f, 500.0f, 25, 20.0f);
  spawnEnemy("Skeleton", 50.0f, 400.0f, 15, 40.0f);
  // ...
}
```

This centralizes entity creation, making it easy to:
- Add initialization logic
- Track spawned entities
- Log or debug spawning
