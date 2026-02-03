# Camera Systems and World Boundaries

This document covers how camera systems work in 2D games, including viewport management and world boundary clamping.

## Table of Contents
- [What is a Camera?](#what-is-a-camera)
- [Coordinate Systems](#coordinate-systems)
- [Camera Position and Following](#camera-position-and-following)
- [World to Screen Conversion](#world-to-screen-conversion)
- [World Boundaries (Clamping)](#world-boundaries-clamping)
- [Integration with Rendering](#integration-with-rendering)
- [Tile-Based Considerations](#tile-based-considerations)

---

## What is a Camera?

In web development, you scroll a page and the viewport moves over content. In games, it's the same concept but reversed: **the camera is a virtual "window" into your game world**.

```
┌─────────────────────────────────────────────────┐
│                  GAME WORLD                      │
│                                                  │
│     ┌─────────────────┐                         │
│     │    VIEWPORT     │  ← What the player sees │
│     │   (Camera view) │                         │
│     │      [P]        │                         │
│     └─────────────────┘                         │
│                                                  │
│                            [E]   [E]            │
│                                                  │
└─────────────────────────────────────────────────┘
```

### Without a Camera

Without a camera system, everything is rendered in **screen coordinates**:
- Position (0, 0) = top-left of window
- Position (400, 300) = center of 800x600 window

This means:
- The player moves, but the view stays fixed
- Once the player walks off-screen, they disappear
- The world is limited to the window size

### With a Camera

A camera introduces **world coordinates** separate from screen coordinates. The camera's job is to **translate** between them:

```
screenPosition = worldPosition - cameraPosition + halfViewport
```

---

## Coordinate Systems

Understanding coordinate systems is crucial for camera implementation.

### World Coordinates

Where things actually exist in your game world:

```
World Space (example: 1600x1200 pixel world)

(0,0)─────────────────────────────────────(1600,0)
  │                                            │
  │         Tree at (200, 150)                 │
  │              [T]                           │
  │                                            │
  │                    Player at (800, 600)    │
  │                         [P]                │
  │                                            │
  │                              Enemy (1400, 900)
  │                                   [E]      │
(0,1200)──────────────────────────────────(1600,1200)
```

World coordinates are **absolute** - they don't change based on what's visible.

### Screen Coordinates

Where things appear on the player's monitor:

```
Screen Space (800x600 window)

(0,0)───────────────────────(800,0)
  │                              │
  │                              │
  │       Visible area only      │
  │                              │
  │                              │
(0,600)─────────────────────(800,600)
```

Screen coordinates are **relative** to the viewport.

### Web Development Analogy

| Game Concept | Web Equivalent |
|--------------|----------------|
| World coordinates | Document coordinates (`pageX`, `pageY`) |
| Screen coordinates | Client coordinates (`clientX`, `clientY`) |
| Camera position | Scroll position (`scrollTop`, `scrollLeft`) |
| Viewport | Browser window / visible area |

---

## Camera Position and Following

### Camera Properties

A 2D camera typically tracks:

```Camera.h
class Camera {
    Vector2 position;      // Center of the view in world coordinates
    int viewportWidth;     // Width of visible area (usually window width)
    int viewportHeight;    // Height of visible area
    Vector2 worldMin;      // Top-left bound of world
    Vector2 worldMax;      // Bottom-right bound of world

    ...
};
```

### Following a Target

To keep the player centered:

```Camera.cpp
void Camera::centerOn(const Vector2& target) {
    position = target;
}
```

Call this each frame with the player's position:

```Game.cpp
void Game::update(float deltaTime) {
    ...

    camera.centerOn(player->getPosition());

    ...
}
```

### What the Camera "Sees"

If the camera is at position (500, 400) with an 800x600 viewport:

```
Visible world region:

Left edge:   500 - (800/2) = 100
Right edge:  500 + (800/2) = 900
Top edge:    400 - (600/2) = 100
Bottom edge: 400 + (600/2) = 700

Camera "sees" world coordinates (100,100) to (900,700)
```

---

## World to Screen Conversion

### The Formula

To convert a world position to where it should be drawn on screen:

```Camera.cpp
Vector2 Camera::worldToScreen(Vector2 worldPos) {
    Vector2 screenPos;
    screenPos.x = worldPos.x - position.x + (viewportWidth / 2.0f);
    screenPos.y = worldPos.y - position.y + (viewportHeight / 2.0f);
    return screenPos;
}
```

### Step-by-Step Example

```
Given:
- Camera position: (500, 400)
- Viewport: 800x600
- Tree world position: (600, 500)

Calculation:
screenX = 600 - 500 + 400 = 500
screenY = 500 - 400 + 300 = 400

Result: Tree appears at screen position (500, 400)
        which is near the center of the screen!
```

### Why Add Half Viewport?

The camera position represents the **center** of what's visible. Adding half the viewport size shifts the coordinate system so that:
- Camera center appears at screen center
- Objects at `cameraPos` render at `(viewportWidth/2, viewportHeight/2)`

```
Without half viewport offset:
- Camera at (500, 400)
- Object at (500, 400)
- Screen position: (0, 0) ← Wrong! Should be center

With half viewport offset:
- Camera at (500, 400)
- Object at (500, 400)
- Screen position: (400, 300) ← Correct! Screen center
```

---

## World Boundaries (Clamping)

### The Problem

If the camera follows the player everywhere without limits:

```
World: 0,0 to 1600,1200
Viewport: 800x600

Player walks to corner (100, 100):
Camera centers on (100, 100)

Visible region would be:
Left: 100 - 400 = -300  ← OUTSIDE WORLD!
Top:  100 - 300 = -200  ← OUTSIDE WORLD!

┌────────────────────────────────────────┐
│▓▓▓▓▓▓▓▓│ World starts here            │
│▓▓▓▓▓▓▓▓│                              │
│▓▓▓▓▓▓▓▓│  Player                      │
│▓▓▓▓▓▓▓▓│    [P]                       │
└────────────────────────────────────────┘
   ↑
Void/garbage - undefined area!
```

### The Solution: Clamping

**Clamp** restricts a value to stay within a range:

```Camera.cpp
float Camera::clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}
```

### Calculating Valid Camera Range

The camera center must stay far enough from edges that the viewport doesn't show outside:

```Camera.cpp
void Camera::centerOn(const Vector2& target) {
    position = target;

    // Calculate valid range for camera center
    float halfWidth = viewportWidth / 2.0f;
    float halfHeight = viewportHeight / 2.0f;

    float minX = worldMin.x + halfWidth;   // Can't go further left
    float maxX = worldMax.x - halfWidth;   // Can't go further right
    float minY = worldMin.y + halfHeight;  // Can't go further up
    float maxY = worldMax.y - halfHeight;  // Can't go further down

    // Clamp camera position
    position.x = clamp(position.x, minX, maxX);
    position.y = clamp(position.y, minY, maxY);
}
```

### Visual Example

```
World: 0,0 to 1600,1200
Viewport: 800x600

Valid camera center range:
  minX = 0 + 400 = 400
  maxX = 1600 - 400 = 1200
  minY = 0 + 300 = 300
  maxY = 1200 - 300 = 900

Camera can move from (400,300) to (1200,900)

Player at corner (50, 50):
- Desired camera: (50, 50)
- Clamped camera: (400, 300)

Result:
┌────────────────────────────────────────┐
│ [P] ← Player in corner                 │
│                                        │
│        [●] ← Camera center (clamped)   │
│                                        │
│                                        │
└────────────────────────────────────────┘
Player appears off-center, but no void is visible!
```

### Edge Case: World Smaller Than Viewport

If the world is smaller than the viewport, center the world:

```Camera.cpp
void Camera::centerOn(const Vector2& target) {
    ...

    if (worldMax.x - worldMin.x < viewportWidth) {
        // World narrower than viewport - center horizontally
        position.x = (worldMin.x + worldMax.x) / 2.0f;
    }
    if (worldMax.y - worldMin.y < viewportHeight) {
        // World shorter than viewport - center vertically
        position.y = (worldMin.y + worldMax.y) / 2.0f;
    }
}
```

---

## Integration with Rendering

### Current Rendering (No Camera)

```Sprite.cpp
void Sprite::draw(Shader& shader, int screenWidth, int screenHeight) {
    ...

    float model[16] = {
        size.x, 0, 0, 0,
        0, size.y, 0, 0,
        0, 0, 1, 0,
        position.x, position.y, 0, 1  // World position used directly
    };

    ...
}
```

This works only when world coordinates = screen coordinates.

### With Camera Offset

```Sprite.cpp
void Sprite::draw(Shader& shader, const Camera& camera) {
    // Convert world position to screen position
    Vector2 screenPos = camera.worldToScreen(position);

    float model[16] = {
        size.x, 0, 0, 0,
        0, size.y, 0, 0,
        0, 0, 1, 0,
        screenPos.x, screenPos.y, 0, 1  // Screen position
    };

    ...
}
```

### Alternative: View Matrix

More advanced engines use a **view matrix** instead of manual offset:

```
Final Position = Projection × View × Model × Vertex

- Model Matrix: Positions object in world
- View Matrix: Inverse of camera transform (moves world opposite to camera)
- Projection Matrix: Converts to clip space
```

For 2D games, manual offset is simpler and equally effective.

### Render Order with Camera

```Game.cpp
void Game::render() {
    window->clear(0.1f, 0.1f, 0.2f);

    // Update camera first
    camera->centerOn(player->getPosition());

    // Render all entities with camera
    for (auto& enemy : enemies) {
        enemy->render(*spriteShader, *camera);
    }
    player->render(*spriteShader, *camera);

    window->swapBuffers();
}
```

---

## Tile-Based Considerations

For Stardew Valley-style games, the world is built from tiles.

### World Size from Tiles

```TileMap.h
class TileMap {
    int widthInTiles;   // e.g., 50 tiles
    int heightInTiles;  // e.g., 40 tiles
    int tileSize;       // e.g., 32 pixels

    float getWorldWidth() {
        return widthInTiles * tileSize;  // 50 * 32 = 1600
    }
    float getWorldHeight() {
        return heightInTiles * tileSize; // 40 * 32 = 1280
    }
};
```

### Per-Level Bounds

Different areas can have different sizes:

```Level.h
struct Level {
    std::string name;
    int tilesWide;
    int tilesHigh;
    int tileSize;
};
```

```Game.cpp
// Farm: 100x80 tiles at 32px = 3200x2560 world
Level farm = {"Farm", 100, 80, 32};

// House interior: 20x15 tiles at 32px = 640x480 world
Level house = {"Farmhouse", 20, 15, 32};

void Game::enterLevel(const Level& level) {
    float worldW = level.tilesWide * level.tileSize;
    float worldH = level.tilesHigh * level.tileSize;
    camera->setWorldBounds(0, 0, worldW, worldH);
}
```

### Smooth vs Tile-Locked Camera

**Smooth following** (used in action games):
```Game.cpp
void Game::update(float deltaTime) {
    ...
    camera->centerOn(player->getPosition());
    ...
}
```

**Tile-locked** (classic RPG style):
```Game.cpp
void Game::update(float deltaTime) {
    ...
    // Snap camera to tile grid
    int tileX = (int)(player->getPosition().x / tileSize);
    int tileY = (int)(player->getPosition().y / tileSize);
    camera->setPosition(tileX * tileSize, tileY * tileSize);
    ...
}
```

**Lerped/smooth transition** (modern approach):
```Game.cpp
void Game::update(float deltaTime) {
    ...
    Vector2 target = player->getPosition();
    Vector2 current = camera->getPosition();
    float smoothing = 0.1f;  // Lower = smoother/slower

    camera->setPosition(
        lerp(current.x, target.x, smoothing),
        lerp(current.y, target.y, smoothing)
    );
    ...
}
```

---

## Summary

| Concept | Purpose |
|---------|---------|
| Camera position | Where in the world we're looking |
| World coordinates | Absolute position in game world |
| Screen coordinates | Position on the player's monitor |
| World-to-screen conversion | `screenPos = worldPos - cameraPos + halfViewport` |
| Clamping | Prevent camera from showing outside world |
| View matrix | Advanced method for camera transform |

### Key Formulas

```cpp
// World to screen
screenX = worldX - cameraX + (viewportWidth / 2)
screenY = worldY - cameraY + (viewportHeight / 2)

// Camera bounds
minCameraX = worldMinX + (viewportWidth / 2)
maxCameraX = worldMaxX - (viewportWidth / 2)

// Clamp
value = max(min, min(max, value))
```
