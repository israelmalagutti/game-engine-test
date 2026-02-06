# Game Engine Documentation

This folder contains comprehensive documentation of the concepts and fundamentals used in this 2D game engine project.

## Documents

### Foundation

### [01 - C++ Fundamentals](./01-cpp-fundamentals.md)
Core C++ language features used throughout the project:
- Classes and access specifiers
- Inheritance and polymorphism
- Virtual functions and `override`
- Smart pointers (`std::unique_ptr`)
- References vs pointers
- Forward declarations
- `const` correctness

### [02 - Build System](./02-build-system.md)
Compilation and build configuration:
- C++ compilation stages
- Makefile syntax and targets
- Compiler flags and warnings
- Linking libraries
- Include paths
- Common build issues

### [03 - SDL2 Integration](./03-sdl2-integration.md)
Using SDL2 for platform abstraction:
- Initialization and subsystems
- Window creation
- OpenGL context setup
- Event handling
- Keyboard input
- Image loading with SDL_image
- Timing and delta time

### [04 - OpenGL Rendering](./04-opengl-rendering.md)
Graphics programming with OpenGL:
- The rendering pipeline
- Vertex Buffer Objects (VBO)
- Vertex Array Objects (VAO)
- Shaders (GLSL)
- Textures and texture units
- Projection and model matrices
- Blending for transparency

### [05 - OpenGL Loaders](./05-opengl-loaders.md)
Loading OpenGL functions at runtime:
- Why loaders are needed
- GLEW vs GLAD comparison
- The `glewExperimental` hack
- Migrating from GLEW to GLAD
- Troubleshooting

### [06 - Game Architecture](./06-game-architecture.md)
Overall structure of the game engine:
- Project organization
- The game loop (input → update → render)
- Entity system and inheritance
- Component responsibilities
- Data flow
- Memory management strategy

### Implemented Features

### [07 - 2D Math](./07-2d-math.md)
Mathematical foundations for 2D games:
- Vectors and vector operations
- Matrix transformations
- Coordinate systems
- Orthographic projection
- Common game math (distance, lerp, clamp)
- Frame-rate independent movement

### [08 - Camera Systems](./08-camera-systems.md)
Camera and viewport management:
- What is a camera in games
- World vs screen coordinate systems
- Camera following and positioning
- World boundary clamping
- Integration with rendering pipeline
- Tile-based game considerations

### [09 - Tilemap Fundamentals](./09-tilemap-fundamentals.md)
Tile-based rendering:
- What is a tilemap
- Tile indices and tilesets
- Grid to pixel coordinate conversion
- Tilemap rendering with OpenGL

### [10 - Texture Atlas](./10-texture-atlas.md)
Efficient texture management:
- What is a texture atlas
- UV coordinate calculation
- Sub-region sampling from spritesheets

### [11 - Hot-Reloading](./11-hot-reloading.md)
Live asset updating during development:
- File watching for changes
- Shader hot-reload
- Tilemap hot-reload
- Graceful failure handling

### [12 - Location System](./12-location-system.md)
Map switching and scene management:
- Discrete location architecture
- Warp zones and transitions
- Location lifecycle (enter/exit)
- Two-phase location switching

### [13 - HUD System](./13-hud-system.md)
Heads-up display for UI elements:
- Screen space vs world space rendering
- Rendering colored rectangles (health/stamina bars)
- Text rendering with SDL_ttf
- HUD class architecture and ownership model

### Conceptual / Future

### [14 - Terrain Architecture](./14-terrain-architecture.md)
Multi-layer terrain systems:
- Terrain layer types
- Layer rendering order
- Collision layers

### [15 - Autotiling](./15-autotiling.md)
Automatic tile selection:
- Bitmask autotiling
- Neighbor-based tile selection
- Seamless terrain transitions

### [16 - Rendering Optimization](./16-rendering-optimization.md)
Performance techniques for 2D rendering:
- Batch rendering
- Frustum culling
- Draw call reduction

### [17 - HD-2D Aesthetics](./17-hd2d-aesthetics.md)
Visual style inspired by Octopath Traveller:
- HD-2D visual concepts
- Shader effects for pixel art
- Depth of field and lighting

### [18 - Save System](./18-save-system.md)
Persisting game state:
- Serialization concepts
- Save file formats
- Loading and restoring state

### Sprite Animation (subdirectory)

See [sprites/DOCS.md](./sprites/DOCS.md) for the full sprite animation documentation covering:
- Frame-by-frame animation
- Layered composite sprites
- Palette swapping / CLUT
- Procedural animation
- Skeletal animation
- Mesh deformation
- Tweening and easing
- Animation principles
- Particle systems
- Shader animation

---

## Quick Reference

### Build Commands
```bash
make        # Build the project
make run    # Build and run
make clean  # Remove built files
```

### Project Structure
```
src/           # Source files
include/       # GLAD headers
shaders/       # GLSL shaders
assets/        # Textures and sprites
docs/          # This documentation
```

### Key Files
| File | Purpose |
|------|---------|
| `Game.cpp` | Main game loop and orchestration |
| `Window.cpp` | SDL2 window and OpenGL context |
| `Sprite.cpp` | 2D rendering with OpenGL |
| `Entity.cpp` | Base class for game objects |
| `Shader.cpp` | GLSL shader compilation |

---

## Learning Path

**Recommended reading order:**

1. **[C++ Fundamentals](./01-cpp-fundamentals.md)** - Understand the language features first
2. **[Build System](./02-build-system.md)** - How to compile the project
3. **[SDL2 Integration](./03-sdl2-integration.md)** - Window and input handling
4. **[OpenGL Rendering](./04-opengl-rendering.md)** - Graphics programming
5. **[OpenGL Loaders](./05-opengl-loaders.md)** - GLAD specifics
6. **[Game Architecture](./06-game-architecture.md)** - See how the pieces fit together
7. **[2D Math](./07-2d-math.md)** - Vectors and transformations
8. **[Camera Systems](./08-camera-systems.md)** - Viewport and world boundaries
