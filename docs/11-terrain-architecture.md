# Terrain Architecture: Why Tilemap is NOT an Entity

This document explains the architectural distinction between the Entity system and the Tilemap system, and how to design a tilemap class that integrates with the existing engine.

## Table of Contents
- [Entity vs System Distinction](#entity-vs-system-distinction)
- [Why Tilemap Should NOT Be an Entity](#why-tilemap-should-not-be-an-entity)
- [Tilemap Class Design](#tilemap-class-design)
- [Memory Layout Considerations](#memory-layout-considerations)
- [Visible Region Culling](#visible-region-culling)
- [Integration with Game Class](#integration-with-game-class)

---

## Entity vs System Distinction

Before designing the tilemap, it's crucial to understand the difference between **Entities** and **Systems** in game architecture.

### What is an Entity?

In our current architecture (`Entity.h`), an Entity represents a **discrete game object** with:

```cpp
// Entity.h (existing)
class Entity {
protected:
    std::string name;
    Vector2 position;
    bool isActive;

public:
    virtual void update(float deltaTime);
    virtual void render(Shader& shader, const Camera& camera);
};
```

Entities are:
- **Individual objects** (one player, one enemy, one NPC)
- **Independently updated** (each has its own `update()` call)
- **Independently rendered** (each has its own `render()` call)
- **Stateful** (position changes, AI decisions, animations)
- **Interactive** (can collide, be clicked, respond to events)

Examples: Player, Enemy, NPC, Projectile, Item pickup

### What is a System?

A **System** is infrastructure that manages many similar things efficiently:

- **Batch-processed** (handles hundreds/thousands of items together)
- **Uniform behavior** (all items follow the same rules)
- **Data-oriented** (stores arrays of data, not individual objects)
- **Static or predictable** (terrain doesn't make decisions)

Examples: Tilemap, Particle system, Audio manager, Physics world

### Web Development Analogy

| Game Concept | Web Equivalent |
|--------------|----------------|
| Entity (Player) | React Component with complex state and lifecycle |
| Entity (Enemy) | Another Component instance with its own state |
| System (Tilemap) | Virtual list / windowed rendering (renders 1000s of items efficiently) |
| System (Particles) | Canvas-based particle effect (batch draws thousands) |

An Entity is like a full React component. A System is like a highly optimized renderer that handles thousands of simple elements without creating individual component instances for each.

---

## Why Tilemap Should NOT Be an Entity

### Argument 1: Scale Difference

```
Entities in a typical scene:
- 1 Player
- 5-20 Enemies
- 10-30 NPCs
- 20-50 Interactive objects
Total: ~100 entities

Tiles in a typical scene:
- 50x40 visible tiles = 2,000 tiles
- Full map: 200x150 = 30,000 tiles
Total: 2,000-30,000 "things"
```

If each tile were an Entity:
- 30,000 `update()` calls per frame (most doing nothing)
- 30,000 `render()` calls (massive overhead)
- 30,000 Entity objects in memory (~1MB+ just for base class overhead)

### Argument 2: Behavioral Difference

```
Entity (Player):                      Tile (Grass):
┌─────────────────────────┐          ┌─────────────────────────┐
│ - Reads input           │          │ - Exists at fixed pos   │
│ - Updates velocity      │          │ - Never moves           │
│ - Checks collisions     │          │ - No state changes      │
│ - Plays animations      │          │ - No AI or logic        │
│ - Has inventory         │          │ - Just visual data      │
│ - Interacts with world  │          │ - Same as every grass   │
└─────────────────────────┘          └─────────────────────────┘
      Complex behavior                    Pure data
```

Tiles don't need:
- Individual `update()` methods (they don't change)
- Virtual function calls (they all render the same way)
- Polymorphism (grass tile = grass tile)
- Individual memory allocations

### Argument 3: Rendering Efficiency

Entities render individually:

```cpp
// Current Entity rendering (fine for ~100 entities)
for (auto& entity : entities) {
    entity->render(shader, camera);  // Virtual call
    // Inside render(): bind texture, set uniforms, draw 1 quad
}
```

Tilemaps should batch render:

```cpp
// Tilemap rendering (necessary for thousands of tiles)
tilemap.render(shader, camera);
// Inside render(): bind texture ONCE, set view ONCE, draw ALL visible tiles
```

### Argument 4: Collision Handling

Entity collision: Check entity against other entities (O(n²) or spatial hash)

```cpp
// Entity collision
for (entity : entities) {
    for (other : entities) {
        if (entity.collidesWith(other)) { ... }
    }
}
```

Tile collision: Direct grid lookup (O(1))

```cpp
// Tile collision - instant lookup!
int tileX = playerX / tileSize;
int tileY = playerY / tileSize;
int tileID = tilemap.getTile(tileX, tileY);
if (tileID == WALL) {
    // Blocked!
}
```

---

## Tilemap Class Design

Based on the above, here's a conceptual design for the Tilemap class.

### Core Class Structure

```cpp
// Tilemap.h (conceptual)
#pragma once

#include "Common.h"
#include "Camera.h"
#include "Shader.h"
#include "Texture.h"

class Tilemap {
private:
    // Map dimensions
    int width;          // Width in tiles
    int height;         // Height in tiles
    int tileSize;       // Pixels per tile (e.g., 32)

    // Tile data (1D array for cache efficiency)
    std::vector<int> tiles;

    // Tileset (texture atlas)
    Texture* tileset;   // Non-owning pointer (Game owns textures)
    int tilesPerRow;    // Tiles per row in the atlas

    // OpenGL resources
    GLuint VAO, VBO;

public:
    Tilemap(int width, int height, int tileSize, Texture* tileset);
    ~Tilemap();

    // Tile access
    int getTile(int x, int y) const;
    void setTile(int x, int y, int tileID);

    // Collision helpers
    bool isSolid(int x, int y) const;
    bool isWalkable(int tileX, int tileY) const;

    // Coordinate conversion
    int worldToTileX(float worldX) const;
    int worldToTileY(float worldY) const;

    // Rendering
    void render(Shader& shader, const Camera& camera);

    // Getters
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getTileSize() const { return tileSize; }
    float getWorldWidth() const { return width * tileSize; }
    float getWorldHeight() const { return height * tileSize; }
};
```

### Key Design Decisions

1. **NOT inheriting from Entity** - Tilemap is its own class
2. **No `update()` method** - Static terrain doesn't update
3. **Single `render()` call** - Handles all visible tiles internally
4. **Direct tile access** - `getTile(x, y)` for collision queries
5. **Non-owning texture pointer** - Game class manages texture lifetime

### Comparison with Entity

```cpp
// Entity.h (existing)
class Entity {
protected:
    std::string name;       // Each entity has a name
    Vector2 position;       // Each entity has ONE position
    bool isActive;

public:
    virtual void update(float deltaTime);   // Per-entity update
    virtual void render(...);               // Per-entity render
};

// Tilemap.h (new, NOT an Entity)
class Tilemap {
private:
    int width, height;          // Dimensions of the grid
    std::vector<int> tiles;     // MANY positions (implicit in grid)
    Texture* tileset;           // Shared texture for all tiles

public:
    // No update() - terrain doesn't change
    void render(...);           // Renders ALL visible tiles at once
    int getTile(int x, int y);  // Grid-based access
};
```

---

## Memory Layout Considerations

### Why 1D Array Over 2D Array

```cpp
// Option 1: 2D array (intuitive but slower)
int tiles[height][width];  // or std::vector<std::vector<int>>

// Option 2: 1D array (less intuitive but faster)
std::vector<int> tiles(width * height);
```

### Cache Efficiency

Modern CPUs load data in **cache lines** (typically 64 bytes). Sequential memory access is much faster than random access.

```
2D array (vector of vectors):
tiles[0] → [ptr] → [ 0, 1, 2, 3, 4 ]  ← Separate allocation
tiles[1] → [ptr] → [ 5, 6, 7, 8, 9 ]  ← Separate allocation (could be anywhere!)
tiles[2] → [ptr] → [10,11,12,13,14]  ← Separate allocation

1D array:
tiles → [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 ]
         └─────────────── Contiguous memory ───────────────┘
```

### Row-Major Access Pattern

When iterating tiles, access in row-major order (all of row 0, then row 1, etc.):

```cpp
// GOOD: Row-major iteration (cache-friendly)
for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
        int tile = tiles[y * width + x];  // Sequential memory access
    }
}

// BAD: Column-major iteration (cache-unfriendly)
for (int x = 0; x < width; x++) {
    for (int y = 0; y < height; y++) {
        int tile = tiles[y * width + x];  // Jumping around in memory
    }
}
```

```
Memory access pattern visualization (width=5):

Row-major (GOOD):              Column-major (BAD):
[0][1][2][3][4]                [0][ ][ ][ ][ ]
[5][6][7][8][9]                [5][ ][ ][ ][ ]
→ Sequential: 0,1,2,3,4,5...   [10][ ][ ][ ][ ]
                               ↓ Jumping: 0,5,10,1,6,11...
```

### Web Development Analogy

This is similar to how `ImageData` works in JavaScript Canvas:

```javascript
// JavaScript Canvas: 1D array for 2D image
const imageData = ctx.getImageData(0, 0, width, height);
const pixels = imageData.data;  // Uint8ClampedArray (1D)

// Access pixel at (x, y) - same pattern as tilemap!
const index = (y * width + x) * 4;  // *4 for RGBA
const red = pixels[index];
```

---

## Visible Region Culling

Rendering only visible tiles is critical for performance.

### The Problem

```
Full map: 200x150 tiles (30,000 tiles)
Visible area: 25x19 tiles (~500 tiles)

Without culling: Render 30,000 tiles (60x more work than needed!)
With culling: Render ~500 tiles
```

### Culling Implementation

```cpp
// Tilemap.cpp (conceptual)
void Tilemap::render(Shader& shader, const Camera& camera) {
    // Calculate visible tile range
    Vector2 camPos = camera.getPosition();
    int viewW = camera.getViewportWidth();
    int viewH = camera.getViewportHeight();

    // World bounds of visible area
    float worldLeft = camPos.x - viewW / 2.0f;
    float worldTop = camPos.y - viewH / 2.0f;
    float worldRight = camPos.x + viewW / 2.0f;
    float worldBottom = camPos.y + viewH / 2.0f;

    // Convert to tile coordinates (add 1-tile buffer for partial tiles)
    int startX = std::max(0, static_cast<int>(worldLeft / tileSize) - 1);
    int startY = std::max(0, static_cast<int>(worldTop / tileSize) - 1);
    int endX = std::min(width, static_cast<int>(worldRight / tileSize) + 2);
    int endY = std::min(height, static_cast<int>(worldBottom / tileSize) + 2);

    // Setup rendering state once
    shader.use();
    shader.setMat4("projection", glm::ortho(0.0f, (float)viewW, (float)viewH, 0.0f));
    shader.setMat4("view", camera.getViewMatrix());
    tileset->bind(0);

    // Render only visible tiles
    for (int y = startY; y < endY; y++) {
        for (int x = startX; x < endX; x++) {
            int tileID = getTile(x, y);
            if (tileID >= 0) {  // -1 = empty/transparent
                renderTile(x, y, tileID, shader);
            }
        }
    }
}
```

### Visual Explanation

```
Full map (200x150):
┌──────────────────────────────────────────────────────────────────────┐
│                                                                      │
│                                                                      │
│                    ┌─────────────────────────┐                       │
│                    │                         │                       │
│                    │    Camera Viewport      │                       │
│                    │    (25x19 tiles)        │                       │
│                    │         [P]             │                       │
│                    │                         │                       │
│                    └─────────────────────────┘                       │
│                         ↑                                            │
│                    Only render this region!                          │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘

startX, startY = top-left tile of visible region
endX, endY = bottom-right tile of visible region (exclusive)
```

### Why the 1-Tile Buffer?

Tiles at the edge of the viewport might be partially visible:

```
Viewport edge:
      │
──────┼──────
  ████│████   ← Tile is half-visible
  ████│████     Without buffer, this tile wouldn't render
──────┼──────
      │
```

Adding `- 1` to start and `+ 2` to end ensures partially visible tiles are included.

---

## Integration with Game Class

### Current Game Structure

```cpp
// Game.h (existing structure)
class Game {
private:
    std::unique_ptr<Window> window;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<Shader> spriteShader;
    std::unique_ptr<Player> player;
    std::vector<std::unique_ptr<Enemy>> enemies;
    std::vector<std::unique_ptr<Texture>> textures;

public:
    void update(float deltaTime);
    void render();
};
```

### Adding Tilemap

```cpp
// Game.h (with tilemap added - conceptual)
class Game {
private:
    std::unique_ptr<Window> window;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<Shader> spriteShader;

    // NEW: Tilemap (separate from entities)
    std::unique_ptr<Tilemap> tilemap;
    // Could have multiple: ground layer, detail layer, etc.
    // std::unique_ptr<Tilemap> groundLayer;
    // std::unique_ptr<Tilemap> detailLayer;

    // Entities (unchanged)
    std::unique_ptr<Player> player;
    std::vector<std::unique_ptr<Enemy>> enemies;

    std::vector<std::unique_ptr<Texture>> textures;

public:
    void update(float deltaTime);
    void render();
};
```

### Render Order

Tilemap renders BEFORE entities (so entities appear on top of terrain):

```cpp
// Game.cpp (conceptual render order)
void Game::render() {
    window->clear(0.1f, 0.1f, 0.2f);

    // 1. Update camera
    camera->centerOn(player->getPosition());

    // 2. Render terrain FIRST (bottom layer)
    tilemap->render(*spriteShader, *camera);

    // 3. Render entities ON TOP of terrain
    for (auto& enemy : enemies) {
        enemy->render(*spriteShader, *camera);
    }
    player->render(*spriteShader, *camera);

    window->swapBuffers();
}
```

### Setting World Bounds from Tilemap

The tilemap defines the world size:

```cpp
// Game.cpp (conceptual initialization)
void Game::init() {
    // ... window, shader setup ...

    // Load tileset texture
    auto tilesetTex = std::make_unique<Texture>("assets/tileset.png", true);  // pixelArt=true

    // Create tilemap (50x40 tiles, 32px each)
    tilemap = std::make_unique<Tilemap>(50, 40, 32, tilesetTex.get());

    // Set camera bounds based on tilemap world size
    camera->setWorldBounds(
        0, 0,
        tilemap->getWorldWidth(),   // 50 * 32 = 1600
        tilemap->getWorldHeight()   // 40 * 32 = 1280
    );

    textures.push_back(std::move(tilesetTex));
}
```

### Collision Integration

Player uses tilemap for collision checks:

```cpp
// Player.cpp (conceptual collision)
void Player::update(float deltaTime, const Tilemap& tilemap) {
    // Calculate intended movement
    Vector2 newPos = position + velocity * deltaTime;

    // Check tile at new position
    int tileX = tilemap.worldToTileX(newPos.x);
    int tileY = tilemap.worldToTileY(newPos.y);

    if (tilemap.isWalkable(tileX, tileY)) {
        position = newPos;  // Move allowed
    } else {
        velocity = Vector2(0, 0);  // Blocked by terrain
    }
}
```

---

## Summary

### Entity vs Tilemap

| Aspect | Entity | Tilemap |
|--------|--------|---------|
| Quantity | Few (~100) | Many (~10,000+) |
| Behavior | Complex, individual | Simple, uniform |
| Update | Per-entity `update()` | No update needed |
| Render | Per-entity `render()` | Batch all at once |
| Collision | Entity-to-entity | Grid lookup (O(1)) |
| Memory | Individual allocations | Single contiguous array |

### Key Design Principles

1. **Tilemap is NOT an Entity** - Different problem, different solution
2. **Use 1D arrays** - Better cache performance than 2D
3. **Cull invisible tiles** - Only render what the camera sees
4. **Render terrain first** - Entities draw on top
5. **Grid-based collision** - O(1) lookup instead of O(n)

### Integration Points

```cpp
// Game owns tilemap (like it owns entities)
std::unique_ptr<Tilemap> tilemap;

// Tilemap renders before entities
tilemap->render(shader, camera);
player->render(shader, camera);

// Camera bounds come from tilemap
camera->setWorldBounds(0, 0, tilemap->getWorldWidth(), tilemap->getWorldHeight());

// Collision uses tilemap directly
if (tilemap->isWalkable(tileX, tileY)) { ... }
```

### Next Steps

- **12-rendering-optimization.md**: Batch rendering for better tilemap performance
- **13-hd2d-aesthetics.md**: Layered effects for HD-2D style
