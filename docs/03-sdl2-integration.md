# SDL2 Integration

This document covers how SDL2 is used for window management, input handling, and OpenGL context creation.

## Table of Contents
- [What is SDL2?](#what-is-sdl2)
- [Initialization](#initialization)
- [Window Creation](#window-creation)
- [OpenGL Context](#opengl-context)
- [Event Handling](#event-handling)
- [Input System](#input-system)
- [Image Loading](#image-loading)
- [Cleanup](#cleanup)

---

## What is SDL2?

SDL (Simple DirectMedia Layer) is a cross-platform library that provides:

- **Window management**: Creating and managing application windows
- **Input handling**: Keyboard, mouse, gamepad input
- **OpenGL context**: Creating OpenGL rendering contexts
- **Audio**: Sound playback (not used in this project)
- **Image loading**: Via SDL_image extension

SDL2 abstracts platform differences, so the same code works on Windows, Linux, and macOS.

---

## Initialization

SDL must be initialized before use:

```cpp
// In Window.cpp
if (SDL_Init(SDL_INIT_VIDEO) < 0) {
  std::cerr << "Failed to Initialize SDL: " << SDL_GetError() << std::endl;
  return;
}
```

### Subsystems

| Flag | Purpose |
|------|---------|
| `SDL_INIT_VIDEO` | Video/window support |
| `SDL_INIT_AUDIO` | Audio playback |
| `SDL_INIT_TIMER` | Timer support |
| `SDL_INIT_EVENTS` | Event handling |
| `SDL_INIT_EVERYTHING` | All subsystems |

`SDL_INIT_VIDEO` implicitly initializes events.

### Error Handling

`SDL_GetError()` returns the last error message:

```cpp
if (SDL_Init(...) < 0) {
  std::cerr << "Error: " << SDL_GetError() << std::endl;
}
```

---

## Window Creation

### Setting OpenGL Attributes

Before creating a window, configure OpenGL settings:

```cpp
// Request OpenGL 3.3 Core Profile
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

// Enable double buffering (prevents tearing)
SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
```

### Core vs Compatibility Profile

| Profile | Description |
|---------|-------------|
| `SDL_GL_CONTEXT_PROFILE_CORE` | Modern OpenGL only, no deprecated functions |
| `SDL_GL_CONTEXT_PROFILE_COMPATIBILITY` | Includes legacy OpenGL (glBegin, etc.) |

Core profile is preferred for new projects.

### Creating the Window

```cpp
window = SDL_CreateWindow(
    title.c_str(),              // Window title
    SDL_WINDOWPOS_CENTERED,     // X position
    SDL_WINDOWPOS_CENTERED,     // Y position
    width,                      // Width in pixels
    height,                     // Height in pixels
    SDL_WINDOW_OPENGL |         // Enable OpenGL
    SDL_WINDOW_SHOWN            // Show immediately
);

if (!window) {
  std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
}
```

### Window Flags

| Flag | Purpose |
|------|---------|
| `SDL_WINDOW_OPENGL` | Create OpenGL context |
| `SDL_WINDOW_SHOWN` | Window is visible |
| `SDL_WINDOW_RESIZABLE` | Allow resizing |
| `SDL_WINDOW_FULLSCREEN` | Fullscreen mode |
| `SDL_WINDOW_BORDERLESS` | No window decorations |

---

## OpenGL Context

### Creating the Context

```cpp
glContext = SDL_GL_CreateContext(window);
if (!glContext) {
  std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
}
```

The context represents the OpenGL state machine. All OpenGL calls operate on the current context.

### VSync

```cpp
SDL_GL_SetSwapInterval(1);  // Enable VSync
```

| Value | Behavior |
|-------|----------|
| 0 | Disable VSync (unlimited FPS) |
| 1 | Enable VSync (sync to monitor refresh) |
| -1 | Adaptive sync (if supported) |

### Buffer Swapping

Double buffering uses two framebuffers:
- **Front buffer**: Currently displayed
- **Back buffer**: Being rendered to

```cpp
void Window::swapBuffers() {
  SDL_GL_SwapWindow(window);  // Swap front and back buffers
}
```

This prevents tearing (seeing partial frames).

---

## Event Handling

SDL uses an event queue. Events are polled each frame:

```cpp
// In Input.cpp
void Input::update() {
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        quitRequested = true;
        break;

      case SDL_KEYDOWN:
        handleKeyDown(event.key.keysym.scancode);
        break;

      case SDL_KEYUP:
        handleKeyUp(event.key.keysym.scancode);
        break;
    }
  }
}
```

### Common Event Types

| Event | Trigger |
|-------|---------|
| `SDL_QUIT` | Window close button clicked |
| `SDL_KEYDOWN` | Key pressed |
| `SDL_KEYUP` | Key released |
| `SDL_MOUSEMOTION` | Mouse moved |
| `SDL_MOUSEBUTTONDOWN` | Mouse button pressed |
| `SDL_MOUSEWHEEL` | Mouse wheel scrolled |
| `SDL_WINDOWEVENT` | Window state changed |

### Event Structure

```cpp
SDL_Event event;
if (event.type == SDL_KEYDOWN) {
  SDL_Scancode key = event.key.keysym.scancode;
  // SDL_SCANCODE_W, SDL_SCANCODE_A, etc.
}
```

---

## Input System

### Keyboard State

Two approaches to keyboard input:

**1. Event-based** (for discrete actions):
```cpp
case SDL_KEYDOWN:
  if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
    jump();  // Triggered once per press
  }
```

**2. State-based** (for continuous input):
```cpp
const Uint8* keystate = SDL_GetKeyboardState(NULL);
if (keystate[SDL_SCANCODE_W]) {
  // W is held down - move continuously
}
```

### Movement Input Example

```cpp
// In Input.cpp
Vector2 Input::getMovementInput() {
  Vector2 movement(0, 0);
  const Uint8* keystate = SDL_GetKeyboardState(NULL);

  if (keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_UP])
    movement.y -= 1;
  if (keystate[SDL_SCANCODE_S] || keystate[SDL_SCANCODE_DOWN])
    movement.y += 1;
  if (keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_LEFT])
    movement.x -= 1;
  if (keystate[SDL_SCANCODE_D] || keystate[SDL_SCANCODE_RIGHT])
    movement.x += 1;

  return movement;
}
```

### Scancode vs Keycode

| Type | Description | Example |
|------|-------------|---------|
| `SDL_Scancode` | Physical key position | `SDL_SCANCODE_W` (same key regardless of layout) |
| `SDL_Keycode` | Logical key (affected by layout) | `SDLK_w` (might be different key on AZERTY) |

Use scancodes for game controls (consistent physical layout).

---

## Image Loading

SDL_image is an extension library for loading various image formats.

### Initialization

```cpp
// Usually called once at startup
int imgFlags = IMG_INIT_PNG;
if (!(IMG_Init(imgFlags) & imgFlags)) {
  std::cerr << "SDL_image init failed: " << IMG_GetError() << std::endl;
}
```

### Loading Images

```cpp
// In Texture.cpp
SDL_Surface* surface = IMG_Load(filepath.c_str());
if (!surface) {
  std::cerr << "Failed to load image: " << IMG_GetError() << std::endl;
}

// Surface contains:
// - surface->w, surface->h: dimensions
// - surface->pixels: raw pixel data
// - surface->format: pixel format info
```

### Pixel Format Handling

SDL surfaces may have different pixel formats. For OpenGL, you need to know the format:

```cpp
GLenum format;
if (surface->format->BytesPerPixel == 4) {
  format = GL_RGBA;  // 32-bit with alpha
} else {
  format = GL_RGB;   // 24-bit no alpha
}

glTexImage2D(GL_TEXTURE_2D, 0, format,
             surface->w, surface->h, 0,
             format, GL_UNSIGNED_BYTE,
             surface->pixels);
```

### Cleanup

```cpp
SDL_FreeSurface(surface);  // Free CPU-side image data
// OpenGL texture remains on GPU
```

---

## Cleanup

SDL resources must be freed in reverse order of creation:

```cpp
Window::~Window() {
  // 1. Delete OpenGL context
  if (glContext) {
    SDL_GL_DeleteContext(glContext);
  }

  // 2. Destroy window
  if (window) {
    SDL_DestroyWindow(window);
  }

  // 3. Quit SDL
  SDL_Quit();
}
```

### Why Order Matters

- OpenGL context requires the window
- Window requires SDL to be initialized
- Destroying in wrong order can crash or leak resources

---

## Timing

SDL provides timing utilities:

```cpp
// In Game.cpp
Uint32 lastFrameTime = SDL_GetTicks();  // Milliseconds since SDL_Init

while (running) {
  Uint32 currentTime = SDL_GetTicks();
  float deltaTime = (currentTime - lastFrameTime) / 1000.0f;  // Convert to seconds
  lastFrameTime = currentTime;

  // Clamp delta time (prevents physics explosions after lag)
  if (deltaTime > 0.1f) {
    deltaTime = 0.1f;
  }

  update(deltaTime);
  render();
}
```

### Delta Time

Delta time is the time elapsed since the last frame. Using it makes movement frame-rate independent:

```cpp
// Wrong: speed depends on frame rate
position.x += 5;  // 300 units/sec at 60fps, 150 at 30fps

// Correct: consistent speed regardless of frame rate
position.x += 5 * deltaTime;  // Always 5 units/sec
```

---

## SDL2 Libraries Used

| Library | Purpose | Link Flag |
|---------|---------|-----------|
| SDL2 | Core functionality | `-lSDL2` |
| SDL2_image | Image loading | `-lSDL2_image` |

Other common SDL2 extension libraries (not used here):
- SDL2_ttf: TrueType font rendering
- SDL2_mixer: Audio playback
- SDL2_net: Networking
