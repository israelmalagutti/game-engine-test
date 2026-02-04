# Camera Systems and World Boundaries

This document covers how camera systems work in 2D games, including viewport management and world boundary clamping.

## Table of Contents
- [GLM Setup](#glm-setup)
- [What is a Camera?](#what-is-a-camera)
- [Coordinate Systems](#coordinate-systems)
- [Camera Position and Following](#camera-position-and-following)
- [World to Screen Conversion](#world-to-screen-conversion)
- [World Boundaries (Clamping)](#world-boundaries-clamping)
- [Integration with Rendering](#integration-with-rendering)
- [View Matrix Approach (MVP Pattern)](#view-matrix-approach-mvp-pattern)
- [Tile-Based Considerations](#tile-based-considerations)

---

## GLM Setup

**GLM** (OpenGL Mathematics) is the industry-standard math library for graphics programming. It provides matrix and vector types that match GLSL (shader language) types, making it much easier than manual `float[16]` arrays.

### Installation

On Arch Linux:
```bash
sudo pacman -S glm
```

On Ubuntu/Debian:
```bash
sudo apt install libglm-dev
```

### Including GLM

Add to your `Common.h`:

```cpp
// Common.h
#pragma once

// Standard libs
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <fstream>
#include <sstream>

// OpenGL (must come before SDL_opengl)
#include <glad/gl.h>

// SDL2
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_image.h>

// Math for OpenGL
#include <glm/glm.hpp>              // Core types: vec2, vec3, vec4, mat4
#include <glm/gtc/matrix_transform.hpp>  // glm::ortho, glm::translate, glm::scale
#include <glm/gtc/type_ptr.hpp>     // glm::value_ptr for OpenGL uniform uploads
```

### Key GLM Types

| GLM Type | GLSL Equivalent | Purpose |
|----------|-----------------|---------|
| `glm::vec2` | `vec2` | 2D position, size, direction |
| `glm::vec3` | `vec3` | 3D position, RGB color |
| `glm::vec4` | `vec4` | 4D position, RGBA color |
| `glm::mat4` | `mat4` | 4x4 transformation matrix |

### Key GLM Functions

```cpp
// Orthographic projection (2D)
glm::mat4 proj = glm::ortho(left, right, bottom, top);

// Translation matrix
glm::mat4 trans = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));

// Scale matrix
glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(sx, sy, 1.0f));

// Combined: translate first, then scale
glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
model = glm::scale(model, glm::vec3(width, height, 1.0f));

// Get raw pointer for OpenGL uniform upload
glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
```

### Updating Shader Class for GLM

Add an overload in your Shader class to accept `glm::mat4`:

```cpp
// Shader.h
#include "Common.h"

class Shader {
public:
    // Existing method for raw float arrays
    void setMat4(const std::string& name, const float* value);

    // New method for glm::mat4
    void setMat4(const std::string& name, const glm::mat4& matrix);
};
```

```cpp
// Shader.cpp
void Shader::setMat4(const std::string& name, const glm::mat4& matrix) {
    glUniformMatrix4fv(
        glGetUniformLocation(ID, name.c_str()),
        1,
        GL_FALSE,
        glm::value_ptr(matrix)
    );
}
```

### Why GLM Over Manual Arrays?

| Manual `float[16]` | GLM `glm::mat4` |
|--------------------|-----------------|
| Error-prone index math | Named operations (`translate`, `scale`) |
| Must remember column-major order | Handles ordering automatically |
| No operator overloads | `*` for matrix multiplication |
| Verbose and hard to read | Clean, readable code |
| Matches no standard | Matches GLSL types exactly |

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

```cpp
// Camera.h
#include "Common.h"  // For glm types
#include "Vector2.h"

class Camera {
private:
    Vector2 position;      // Center of the view in world coordinates
    int viewportWidth;     // Width of visible area (usually window width)
    int viewportHeight;    // Height of visible area
    Vector2 worldMin;      // Top-left bound of world
    Vector2 worldMax;      // Bottom-right bound of world

public:
    Camera(int viewportWidth, int viewportHeight);

    void setWorldBounds(float minX, float minY, float maxX, float maxY);
    void setViewportSize(int width, int height);
    void setPosition(const Vector2& pos);
    void centerOn(const Vector2& target);

    Vector2 getPosition() const;
    int getViewportWidth() const;
    int getViewportHeight() const;
    Vector2 worldToScreen(const Vector2& worldPos) const;
    glm::mat4 getViewMatrix() const;  // Returns view matrix using GLM
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

```cpp
// Game.cpp
void Game::update(float deltaTime) {
    player->update(deltaTime);

    for (auto& enemy : enemies) {
        enemy->setTarget(player->getPosition());
        enemy->update(deltaTime);
    }

    // Center camera on player (with boundary clamping)
    camera->centerOn(player->getPosition());

    removeDeadEntities();
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

If the world is smaller than the viewport, center the world. Here's the complete `centerOn()` function with edge case handling:

```cpp
// Camera.cpp
void Camera::centerOn(const Vector2& target) {
    position = target;

    // Calculate valid range for camera center
    float halfWidth = viewportWidth / 2.0f;
    float halfHeight = viewportHeight / 2.0f;

    float minX = worldMin.x + halfWidth;
    float maxX = worldMax.x - halfWidth;
    float minY = worldMin.y + halfHeight;
    float maxY = worldMax.y - halfHeight;

    // Clamp camera position to valid range
    position.x = clamp(position.x, minX, maxX);
    position.y = clamp(position.y, minY, maxY);

    // Edge case: World narrower than viewport - center horizontally
    if (worldMax.x - worldMin.x < viewportWidth) {
        position.x = (worldMin.x + worldMax.x) / 2.0f;
    }

    // Edge case: World shorter than viewport - center vertically
    if (worldMax.y - worldMin.y < viewportHeight) {
        position.y = (worldMin.y + worldMax.y) / 2.0f;
    }
}
```

---

## Integration with Rendering

### Current Rendering (No Camera)

```cpp
// Sprite.cpp
void Sprite::draw(Shader& shader, int screenWidth, int screenHeight) {
    shader.use();

    // Orthographic projection using GLM
    // Maps pixel coordinates (0,0 top-left) to OpenGL's -1 to 1 range
    // glm::ortho(left, right, bottom, top) - note bottom > top for Y-down
    glm::mat4 projection = glm::ortho(
        0.0f, static_cast<float>(screenWidth),   // left, right
        static_cast<float>(screenHeight), 0.0f   // bottom, top (flipped for Y-down)
    );

    // Model matrix: translate to position, then scale
    // GLM builds matrices by chaining operations
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, 0.0f));
    model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f));

    shader.setMat4("projection", projection);
    shader.setMat4("model", model);
    shader.setInt("spriteTexture", 0);

    texture->bind(0);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    texture->unbind();
}
```

This works only when world coordinates = screen coordinates.

### With Camera Offset

```cpp
// Sprite.cpp - with camera offset (CPU-side conversion)
void Sprite::draw(Shader& shader, const Camera& camera) {
    shader.use();

    int viewportWidth = camera.getViewportWidth();
    int viewportHeight = camera.getViewportHeight();

    // Orthographic projection using GLM
    glm::mat4 projection = glm::ortho(
        0.0f, static_cast<float>(viewportWidth),
        static_cast<float>(viewportHeight), 0.0f
    );

    // Convert world position to screen position
    Vector2 screenPos = camera.worldToScreen(position);

    // Model matrix uses SCREEN position (after CPU conversion)
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(screenPos.x, screenPos.y, 0.0f));
    model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f));

    shader.setMat4("projection", projection);
    shader.setMat4("model", model);
    shader.setInt("spriteTexture", 0);

    texture->bind(0);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    texture->unbind();
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

## View Matrix Approach (MVP Pattern)

The "Alternative: View Matrix" mentioned above is actually the **standard approach** used in professional game engines. This section explains how it works and why it's preferable for larger projects.

### The MVP Pipeline

**MVP** stands for **Model-View-Projection**, which is the standard transformation pipeline in 3D and 2D graphics:

```
Final Position = Projection × View × Model × Vertex
                     ↑          ↑       ↑
                     │          │       └── Object's position/scale/rotation in world
                     │          └────────── Camera's inverse transform
                     └───────────────────── Converts to normalized device coordinates
```

#### Web Development Analogy

Think of it like CSS transforms, but applied in a specific order:

| MVP Stage | CSS Equivalent | Purpose |
|-----------|----------------|---------|
| Model | `transform: translate(100px, 50px) scale(2)` | Position object in world |
| View | `transform: translate(-scrollX, -scrollY)` | "Move world" opposite to camera |
| Projection | `viewport` meta tag / CSS viewport units | Map to screen coordinates |

### Current Approach: CPU-Side Conversion

Our current implementation uses `worldToScreen()` to convert positions on the CPU before sending to the GPU:

```
Current Pipeline:
1. Entity has world position (e.g., 500, 400)
2. CPU calls worldToScreen() → gets screen position (e.g., 320, 240)
3. Model matrix built with SCREEN position
4. GPU computes: Projection × Model × Vertex
```

```cpp
// Sprite.cpp - Current approach (CPU-side conversion)
void Sprite::draw(Shader& shader, const Camera& camera) {
    shader.use();

    int viewportWidth = camera.getViewportWidth();
    int viewportHeight = camera.getViewportHeight();

    // Orthographic projection using GLM
    glm::mat4 projection = glm::ortho(
        0.0f, static_cast<float>(viewportWidth),
        static_cast<float>(viewportHeight), 0.0f
    );

    // CPU converts world position to screen position
    Vector2 screenPos = camera.worldToScreen(position);

    // Model matrix uses SCREEN position (after CPU conversion)
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(screenPos.x, screenPos.y, 0.0f));
    model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f));

    shader.setMat4("projection", projection);
    shader.setMat4("model", model);
    shader.setInt("spriteTexture", 0);

    texture->bind(0);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    texture->unbind();
}
```

### View Matrix Approach: GPU-Side Conversion

With a view matrix, the GPU handles the camera transformation:

```
MVP Pipeline:
1. Entity has world position (e.g., 500, 400)
2. Model matrix built with WORLD position (no conversion)
3. View matrix represents camera transform
4. GPU computes: Projection × View × Model × Vertex
```

```cpp
// Sprite.cpp - MVP approach (GPU-side conversion)
void Sprite::draw(Shader& shader, const Camera& camera) {
    shader.use();

    int viewportWidth = camera.getViewportWidth();
    int viewportHeight = camera.getViewportHeight();

    // Orthographic projection using GLM
    glm::mat4 projection = glm::ortho(
        0.0f, static_cast<float>(viewportWidth),
        static_cast<float>(viewportHeight), 0.0f
    );

    // View matrix from camera (handles camera transformation)
    glm::mat4 view = camera.getViewMatrix();

    // Model matrix uses WORLD position (no CPU conversion needed!)
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, 0.0f));
    model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f));

    shader.setMat4("projection", projection);
    shader.setMat4("view", view);
    shader.setMat4("model", model);
    shader.setInt("spriteTexture", 0);

    texture->bind(0);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    texture->unbind();
}
```

### Constructing the View Matrix

For a 2D camera, the view matrix is a **translation matrix** that moves everything opposite to the camera position, then offsets by half the viewport (to center the camera):

```cpp
// Camera.cpp
glm::mat4 Camera::getViewMatrix() const {
    // Translation to apply:
    // tx = -cameraX + viewportWidth/2
    // ty = -cameraY + viewportHeight/2

    float tx = -position.x + viewportWidth / 2.0f;
    float ty = -position.y + viewportHeight / 2.0f;

    // GLM handles column-major order automatically
    return glm::translate(glm::mat4(1.0f), glm::vec3(tx, ty, 0.0f));
}
```

This is **much simpler** than manual array construction. GLM's `glm::translate()` creates the exact same matrix that we previously built by hand:

```cpp
// What GLM creates internally (column-major):
// [1  0  0  tx]
// [0  1  0  ty]
// [0  0  1  0 ]
// [0  0  0  1 ]
```

#### Why This Formula?

The view matrix translation is **exactly equivalent** to the `worldToScreen()` formula:

```
worldToScreen formula:
    screenX = worldX - cameraX + viewportWidth/2
    screenY = worldY - cameraY + viewportHeight/2

View matrix translation:
    tx = -cameraX + viewportWidth/2
    ty = -cameraY + viewportHeight/2

When GPU multiplies: view * model * vertex
    resultX = worldX + tx = worldX - cameraX + viewportWidth/2  ✓ Same!
    resultY = worldY + ty = worldY - cameraY + viewportHeight/2  ✓ Same!
```

### Updated Vertex Shader

To use the view matrix, the shader needs to include it in the transformation:

```glsl
// sprite.vert
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;        // NEW: Camera transform
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
```

### CPU vs GPU Approach Comparison

| Aspect | CPU (worldToScreen) | GPU (View Matrix) |
|--------|---------------------|-------------------|
| Where conversion happens | C++ code | Vertex shader |
| Model matrix contains | Screen position | World position |
| Uniforms sent per frame | projection, model | projection, view, model |
| View matrix updates | N/A | Once per frame |
| Code clarity | Simple for beginners | Industry standard |

### Why Use View Matrix?

1. **Cleaner Separation of Concerns**
   - Model matrix = "where is this object in the world?"
   - View matrix = "where is the camera?"
   - Projection matrix = "how do we map to the screen?"

2. **Single Uniform Update for Camera**
   - CPU approach: Every sprite recalculates screen position
   - GPU approach: View matrix set once, applies to all sprites

3. **Enables Batch Rendering**
   - All sprites with the same texture can share uniforms
   - GPU can draw thousands of sprites with one draw call

4. **Required for Tilemaps**
   - Drawing 100+ tiles? Setting view once is far more efficient
   - Instanced rendering requires world positions, not screen positions

5. **Standard Pattern**
   - Every professional engine uses MVP
   - Learning this prepares you for Unity, Unreal, Godot, etc.

### Performance Example: Tilemap Rendering

Consider rendering a 50x40 tile map (2000 tiles):

**CPU Approach:**
```cpp
// Every tile needs CPU conversion - 2000 worldToScreen() calls!
for (int y = 0; y < 40; y++) {
    for (int x = 0; x < 50; x++) {
        Vector2 worldPos(x * 32, y * 32);
        Vector2 screenPos = camera.worldToScreen(worldPos);  // CPU conversion

        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(screenPos.x, screenPos.y, 0.0f));
        model = glm::scale(model, glm::vec3(32.0f, 32.0f, 1.0f));

        shader.setMat4("model", model);
        glBindVertexArray(tileVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}
```

**GPU Approach:**
```cpp
// View matrix set once - GPU handles all camera transformations
glm::mat4 view = camera.getViewMatrix();
shader.setMat4("view", view);  // Set ONCE for all 2000 tiles

for (int y = 0; y < 40; y++) {
    for (int x = 0; x < 50; x++) {
        // Model uses WORLD position - no CPU conversion needed!
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x * 32.0f, y * 32.0f, 0.0f));
        model = glm::scale(model, glm::vec3(32.0f, 32.0f, 1.0f));

        shader.setMat4("model", model);
        glBindVertexArray(tileVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}
```

With instanced rendering (advanced), this becomes even more efficient - one draw call for all tiles.

### When to Use Each Approach

| Use Case | Recommended Approach |
|----------|---------------------|
| Learning / prototyping | CPU (worldToScreen) |
| Small number of entities (<50) | Either works fine |
| Tilemap rendering | GPU (View Matrix) |
| Batch/instanced rendering | GPU (View Matrix) |
| Professional projects | GPU (View Matrix) |

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
```cpp
// Game.cpp - Camera immediately follows player position
void Game::update(float deltaTime) {
    player->update(deltaTime);

    for (auto& enemy : enemies) {
        enemy->setTarget(player->getPosition());
        enemy->update(deltaTime);
    }

    // Camera instantly centers on player
    camera->centerOn(player->getPosition());

    removeDeadEntities();
}
```

**Tile-locked** (classic RPG style):
```cpp
// Game.cpp - Camera snaps to tile grid positions
void Game::update(float deltaTime) {
    player->update(deltaTime);

    for (auto& enemy : enemies) {
        enemy->setTarget(player->getPosition());
        enemy->update(deltaTime);
    }

    // Snap camera to tile grid (e.g., 32x32 tiles)
    int tileSize = 32;
    int tileX = static_cast<int>(player->getPosition().x / tileSize);
    int tileY = static_cast<int>(player->getPosition().y / tileSize);
    camera->setPosition(Vector2(tileX * tileSize, tileY * tileSize));

    removeDeadEntities();
}
```

**Lerped/smooth transition** (modern approach):
```cpp
// Game.cpp - Camera smoothly interpolates toward player
void Game::update(float deltaTime) {
    player->update(deltaTime);

    for (auto& enemy : enemies) {
        enemy->setTarget(player->getPosition());
        enemy->update(deltaTime);
    }

    // Smooth camera follow using linear interpolation
    Vector2 target = player->getPosition();
    Vector2 current = camera->getPosition();
    float smoothing = 0.1f;  // Lower = smoother/slower

    float newX = current.x + (target.x - current.x) * smoothing;
    float newY = current.y + (target.y - current.y) * smoothing;
    camera->setPosition(Vector2(newX, newY));

    removeDeadEntities();
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
