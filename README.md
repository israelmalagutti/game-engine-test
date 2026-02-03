# HD-2D Game Engine

A learning-focused 2D game engine built from scratch using C++, SDL2, and OpenGL 3.3 Core.

## Project Intent

This project is a personal learning journey into game development fundamentals, specifically aimed at understanding:

- **C++17** - Modern C++ features, memory management, OOP patterns
- **OpenGL 3.3 Core** - GPU rendering pipeline, shaders, textures
- **SDL2** - Cross-platform windowing, input handling, image loading
- **Game Architecture** - Game loops, entity systems, coordinate systems

The end goal is to build an **HD-2D clone of Stardew Valley** with visual aesthetics inspired by **Octopath Traveler 2** - combining 2D pixel art with modern shader effects.

## Features

- OpenGL 3.3 Core rendering with GLAD loader
- Sprite-based 2D rendering with orthographic projection
- Entity system with inheritance (Player, Enemy)
- SDL2 window management with resize support
- WASD/Arrow key input handling
- Delta-time based movement (frame-rate independent)

## Requirements

### Linux (Arch-based)

```bash
sudo pacman -S sdl2 sdl2_image mesa
```

### Linux (Debian/Ubuntu)

```bash
sudo apt install libsdl2-dev libsdl2-image-dev libgl1-mesa-dev
```

### Dependencies

| Library | Purpose |
|---------|---------|
| SDL2 | Window creation, input, timing |
| SDL2_image | PNG/JPG texture loading |
| OpenGL 3.3+ | GPU rendering |
| GLAD | OpenGL function loading (included) |

## Building

```bash
# Clone the repository
git clone <repository-url>
cd game-engine-test

# Build
make

# Build and run
make run

# Clean build files
make clean
```

## Project Structure

```
game-engine-test/
├── src/                  # Source files
│   ├── main.cpp          # Entry point
│   ├── Game.cpp/h        # Main game loop
│   ├── Window.cpp/h      # SDL2 window + OpenGL context
│   ├── Input.cpp/h       # Keyboard input handling
│   ├── Entity.cpp/h      # Base game object
│   ├── Player.cpp/h      # Player entity
│   ├── Enemy.cpp/h       # Enemy entity
│   ├── Sprite.cpp/h      # 2D rendering
│   ├── Shader.cpp/h      # GLSL shader management
│   ├── Texture.cpp/h     # Texture loading
│   ├── Vector2.h         # 2D vector math
│   └── glad.c            # OpenGL loader
├── include/              # GLAD headers
├── shaders/              # GLSL shaders
│   ├── sprite.vert       # Vertex shader
│   └── sprite.frag       # Fragment shader
├── assets/               # Game assets
│   ├── player.png
│   └── enemy.png
├── docs/                 # Learning documentation
├── Makefile              # Build configuration
└── CLAUDE.md             # AI assistant guidelines
```

## Documentation

The `docs/` folder contains comprehensive explanations of every concept used in this project, written for someone coming from web development:

1. **C++ Fundamentals** - Classes, inheritance, smart pointers
2. **OpenGL Rendering** - Shaders, VAO/VBO, textures, matrices
3. **SDL2 Integration** - Windows, events, input, timing
4. **Game Architecture** - Game loop, entities, data flow
5. **Build System** - Makefile, compilation, linking
6. **OpenGL Loaders** - GLAD vs GLEW
7. **2D Math** - Vectors, matrices, transformations
8. **Camera Systems** - Viewports, world boundaries, coordinate systems

## Controls

| Key | Action |
|-----|--------|
| W / Up Arrow | Move up |
| S / Down Arrow | Move down |
| A / Left Arrow | Move left |
| D / Right Arrow | Move right |
| Escape | Quit |

## License

This is a personal learning project. Feel free to use it as a reference for your own learning.

## Acknowledgments

- [LearnOpenGL](https://learnopengl.com/) - OpenGL tutorials
- [Lazy Foo' Productions](https://lazyfoo.net/tutorials/SDL/) - SDL2 tutorials
- [Stardew Valley](https://www.stardewvalley.net/) - Gameplay inspiration
- [Octopath Traveler 2](https://www.square-enix-games.com/octopathtraveler2) - Visual style inspiration
