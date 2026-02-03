# OpenGL Loaders: GLAD vs GLEW

This document explains why OpenGL loaders are needed and compares GLAD and GLEW.

## Table of Contents
- [Why OpenGL Loaders Exist](#why-opengl-loaders-exist)
- [GLEW](#glew)
- [GLAD](#glad)
- [Comparison](#comparison)
- [Migration Guide](#migration-guide)

---

## Why OpenGL Loaders Exist

### The Problem

OpenGL is not a library you link against directly. It's a specification that GPU vendors implement. The actual OpenGL functions are provided by your graphics driver at runtime.

On most platforms, only OpenGL 1.1 functions are directly available. Modern OpenGL (3.3+) functions must be loaded dynamically:

```cpp
// This is what you'd have to do manually:
typedef void (*PFNGLbindvertexarrayproc)(GLuint array);
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;

// Load from driver
glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)
    SDL_GL_GetProcAddress("glBindVertexArray");

// Now you can use it
glBindVertexArray(vao);
```

Doing this for hundreds of functions is tedious and error-prone.

### The Solution

OpenGL loaders automate this process:

```cpp
// With GLAD
gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);

// Now all functions are available
glBindVertexArray(vao);  // Just works
```

---

## GLEW

GLEW (OpenGL Extension Wrangler) is an older, widely-used loader.

### Setup

1. Install system library:
   ```bash
   # Arch Linux
   sudo pacman -S glew

   # Ubuntu/Debian
   sudo apt install libglew-dev
   ```

2. Include and link:
   ```cpp
   #include <GL/glew.h>
   ```
   ```makefile
   LIBS = -lGLEW -lGL
   ```

### Initialization

```cpp
// Must be called after creating OpenGL context
glewExperimental = GL_TRUE;  // Required for core profile!
GLenum err = glewInit();
if (err != GLEW_OK) {
    std::cerr << "GLEW error: " << glewGetErrorString(err) << std::endl;
}
```

### The "Experimental" Hack

`glewExperimental = GL_TRUE` is required for OpenGL 3.2+ core profile. Without it, GLEW fails to load many functions.

This is because GLEW uses the old extension mechanism by default, which doesn't work with core profile contexts. The "experimental" flag enables direct function loading.

**Problems with this:**
- Unintuitive (why is core profile "experimental"?)
- Generates a GL error on initialization (harmless but confusing)
- Easy to forget, causing mysterious crashes

---

## GLAD

GLAD is a modern OpenGL loader that generates code specifically for your needs.

### Setup

1. Generate files at https://glad.dav1d.de/ or use command-line:
   ```bash
   glad --api gl:core=3.3 --out-path ./glad_output c
   ```

2. Add generated files to your project:
   ```
   include/glad/gl.h
   include/KHR/khrplatform.h
   src/glad.c
   ```

3. Include and compile:
   ```cpp
   #include <glad/gl.h>
   ```
   ```makefile
   SOURCES = ... src/glad.c
   CXXFLAGS = ... -I./include
   # No -lGLAD needed!
   ```

### Initialization

```Window.cpp
Window::Window(const std::string& title, int width, int height) {
    ...

    // Simple, no hacks needed
    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
    }

    ...
}
```

### Generator Options

| Option | Description |
|--------|-------------|
| `--api gl:core=3.3` | OpenGL 3.3 Core Profile |
| `--api gl:compatibility=4.6` | OpenGL 4.6 Compatibility |
| `--extensions GL_ARB_...` | Include specific extensions |
| `c` | Generate C code |
| `--header-only` | Single-header library |

---

## Comparison

| Feature | GLEW | GLAD |
|---------|------|------|
| Installation | System library | Generated source files |
| Core profile support | Requires hack | Native |
| Customizable | No | Yes (generate only what you need) |
| Dependencies | Binary library | None (self-contained) |
| Build | Link `-lGLEW` | Compile `glad.c` |
| Maintenance | Less active | Actively maintained |
| Binary size | Loads everything | Only requested functions |

### When to Use GLEW

- Quick prototyping (already installed)
- Legacy projects
- When you need all extensions

### When to Use GLAD

- New projects (recommended)
- Cross-platform builds (no external dependency)
- When you want minimal code
- When you need specific OpenGL version/extensions

---

## Migration Guide

### From GLEW to GLAD

**1. Generate GLAD files:**
```bash
glad --api gl:core=3.3 --out-path ./glad_output c
```

**2. Add files to project:**
```
cp glad_output/include/glad ./include/
cp glad_output/include/KHR ./include/
cp glad_output/src/gl.c ./src/glad.c
```

**3. Update includes:**
```cpp
// Before
#include <GL/glew.h>

// After
#include <glad/gl.h>
```

**4. Update initialization:**
```cpp
// Before
glewExperimental = GL_TRUE;
GLenum err = glewInit();
if (err != GLEW_OK) {
    std::cerr << glewGetErrorString(err) << std::endl;
}

// After
if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
    std::cerr << "Failed to initialize GLAD" << std::endl;
}
```

**5. Update build:**
```makefile
# Before
LIBS = -lSDL2 -lSDL2_image -lGLEW -lGL
SOURCES = $(wildcard src/*.cpp)

# After
LIBS = -lSDL2 -lSDL2_image -lGL
SOURCES = $(wildcard src/*.cpp) src/glad.c
CXXFLAGS = ... -I./include
```

**6. Include order:**

Both GLAD and GLEW must be included before any other OpenGL headers:

```Common.h
// Correct
#include <glad/gl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
```

```Common.h
// Wrong - will cause conflicts
#include <SDL2/SDL_opengl.h>
#include <glad/gl.h>
```

---

## How GLAD Works Internally

### Generated Code Structure

```c
// glad/gl.h - Declarations
extern PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray;
#define glBindVertexArray glad_glBindVertexArray

// glad.c - Loading
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray = NULL;

int gladLoadGL(GLADloadfunc load) {
    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)
        load("glBindVertexArray");
    // ... hundreds more
    return 1;
}
```

### The Load Function

GLAD needs a function to query OpenGL function pointers. SDL provides this:

```cpp
// SDL_GL_GetProcAddress returns function pointers
gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
```

Other windowing libraries have equivalents:
- GLFW: `glfwGetProcAddress`
- Windows: `wglGetProcAddress`
- X11/GLX: `glXGetProcAddress`

---

## Checking OpenGL Version

After loading, you can query the OpenGL version:

```cpp
// With GLAD
int major = GLVersion.major;
int minor = GLVersion.minor;
printf("OpenGL %d.%d loaded\n", major, minor);

// Or use OpenGL directly
const char* version = (const char*)glGetString(GL_VERSION);
printf("OpenGL version: %s\n", version);
```

---

## Extension Checking

### With GLAD

```cpp
// Check if extension is available
if (GLAD_GL_ARB_debug_output) {
    // Use debug output extension
    glDebugMessageCallback(debugCallback, nullptr);
}
```

### With GLEW

```cpp
if (GLEW_ARB_debug_output) {
    glDebugMessageCallback(debugCallback, nullptr);
}
```

---

## Troubleshooting

### "gladLoadGL returned 0"

**Causes:**
- No OpenGL context created
- Context not made current
- Wrong load function passed

**Fix:**
```Window.cpp
Window::Window(const std::string& title, int width, int height) {
    ...

    // Ensure context exists and is current
    SDL_GL_CreateContext(window);  // Create
    // SDL automatically makes it current

    // Then load GLAD
    gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);

    ...
}
```

### Functions are NULL

**Cause:** Requested OpenGL version not supported

**Fix:** Check supported version:
```cpp
gladLoadGL(load);
if (GLVersion.major < 3 || (GLVersion.major == 3 && GLVersion.minor < 3)) {
    std::cerr << "OpenGL 3.3 not supported!" << std::endl;
}
```

### Conflicting Definitions

**Cause:** Multiple OpenGL headers included

**Fix:** Include glad first, ensure consistent header order:
```Common.h
#pragma once

#include <glad/gl.h>    // Always first!
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>  // After GLAD
```
