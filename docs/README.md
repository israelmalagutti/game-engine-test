# Game Engine Documentation

This folder contains comprehensive documentation of the concepts and fundamentals used in this 2D game engine project.

## Documents

### [01 - C++ Fundamentals](./01-cpp-fundamentals.md)
Core C++ language features used throughout the project:
- Classes and access specifiers
- Inheritance and polymorphism
- Virtual functions and `override`
- Smart pointers (`std::unique_ptr`)
- References vs pointers
- Forward declarations
- `const` correctness

### [02 - OpenGL Rendering](./02-opengl-rendering.md)
Graphics programming with OpenGL:
- The rendering pipeline
- Vertex Buffer Objects (VBO)
- Vertex Array Objects (VAO)
- Shaders (GLSL)
- Textures and texture units
- Projection and model matrices
- Blending for transparency

### [03 - SDL2 Integration](./03-sdl2-integration.md)
Using SDL2 for platform abstraction:
- Initialization and subsystems
- Window creation
- OpenGL context setup
- Event handling
- Keyboard input
- Image loading with SDL_image
- Timing and delta time

### [04 - Game Architecture](./04-game-architecture.md)
Overall structure of the game engine:
- Project organization
- The game loop (input → update → render)
- Entity system and inheritance
- Component responsibilities
- Data flow
- Memory management strategy

### [05 - Build System](./05-build-system.md)
Compilation and build configuration:
- C++ compilation stages
- Makefile syntax and targets
- Compiler flags and warnings
- Linking libraries
- Include paths
- Common build issues

### [06 - OpenGL Loaders](./06-opengl-loaders.md)
Loading OpenGL functions at runtime:
- Why loaders are needed
- GLEW vs GLAD comparison
- The `glewExperimental` hack
- Migrating from GLEW to GLAD
- Troubleshooting

### [07 - 2D Math](./07-2d-math.md)
Mathematical foundations for 2D games:
- Vectors and vector operations
- Matrix transformations
- Coordinate systems
- Orthographic projection
- Common game math (distance, lerp, clamp)
- Frame-rate independent movement

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
ai-context/    # This documentation
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
2. **[Game Architecture](./04-game-architecture.md)** - See how the pieces fit together
3. **[SDL2 Integration](./03-sdl2-integration.md)** - Window and input handling
4. **[OpenGL Rendering](./02-opengl-rendering.md)** - Graphics programming
5. **[2D Math](./07-2d-math.md)** - Vectors and transformations
6. **[Build System](./05-build-system.md)** - Compilation details
7. **[OpenGL Loaders](./06-opengl-loaders.md)** - GLAD specifics
