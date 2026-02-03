# OpenGL Rendering Fundamentals

This document covers the OpenGL concepts used for 2D sprite rendering in this engine.

## Table of Contents
- [OpenGL Overview](#opengl-overview)
- [The Rendering Pipeline](#the-rendering-pipeline)
- [Buffers: VAO, VBO, EBO](#buffers-vao-vbo-ebo)
- [Shaders](#shaders)
- [Textures](#textures)
- [Coordinate Systems and Matrices](#coordinate-systems-and-matrices)
- [Blending and Transparency](#blending-and-transparency)

---

## OpenGL Overview

OpenGL is a graphics API that communicates with the GPU to render 2D/3D graphics. Key characteristics:

- **State Machine**: OpenGL maintains global state. You bind objects, set state, then draw.
- **Client-Server Model**: Your code (client) sends commands to the GPU (server).
- **Core Profile**: Modern OpenGL (3.3+) removes deprecated features, requires shaders.

### OpenGL Context

Before using OpenGL, you need a context (created by SDL in this project):

```Window.cpp
Window::Window(const std::string& title, int width, int height) {
    ...

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    glContext = SDL_GL_CreateContext(window);

    ...
}
```

### GLAD Loader

OpenGL function pointers must be loaded at runtime. GLAD handles this:

```Window.cpp
Window::Window(const std::string& title, int width, int height) {
    ...

    // Load all OpenGL functions
    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
    }

    ...
}
```

---

## The Rendering Pipeline

Data flows through several stages from your vertices to pixels on screen:

```
Vertex Data → Vertex Shader → Primitive Assembly → Rasterization → Fragment Shader → Framebuffer
```

### Stages (Simplified)

1. **Vertex Shader**: Transforms each vertex position (runs per-vertex)
2. **Primitive Assembly**: Groups vertices into triangles
3. **Rasterization**: Converts triangles to fragments (potential pixels)
4. **Fragment Shader**: Determines color of each fragment (runs per-pixel)
5. **Framebuffer**: Final image displayed on screen

In this 2D engine, we primarily work with:
- **Vertex Shader**: Applies projection and model transforms
- **Fragment Shader**: Samples texture and outputs color

---

## Buffers: VAO, VBO, EBO

OpenGL uses buffer objects to store vertex data on the GPU.

### Vertex Buffer Object (VBO)

Stores raw vertex data (positions, texture coordinates, etc.):

```Sprite.cpp
void Sprite::setupMesh() {
    float vertices[] = {
        // pos      // tex coords
        0.0f, 1.0f, 0.0f, 1.0f,  // top-left
        1.0f, 0.0f, 1.0f, 0.0f,  // bottom-right
        0.0f, 0.0f, 0.0f, 0.0f,  // bottom-left
        0.0f, 1.0f, 0.0f, 1.0f,  // top-left
        1.0f, 1.0f, 1.0f, 1.0f,  // top-right
        1.0f, 0.0f, 1.0f, 0.0f,  // bottom-right
    };

    glGenBuffers(1, &VBO);                    // Create buffer
    glBindBuffer(GL_ARRAY_BUFFER, VBO);       // Bind as active
    glBufferData(GL_ARRAY_BUFFER,             // Upload data
                 sizeof(vertices),
                 vertices,
                 GL_STATIC_DRAW);

    ...
}
```

`GL_STATIC_DRAW` hints that data won't change frequently.

### Vertex Array Object (VAO)

Stores the configuration of vertex attributes - how to interpret VBO data:

```Sprite.cpp
void Sprite::setupMesh() {
    ...

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Configure attribute 0: position (2 floats)
    glVertexAttribPointer(0,                    // Attribute index
                          2,                    // Number of components
                          GL_FLOAT,             // Data type
                          GL_FALSE,             // Normalize?
                          4 * sizeof(float),    // Stride (bytes between vertices)
                          (void*)0);            // Offset
    glEnableVertexAttribArray(0);

    // Configure attribute 1: texture coordinates (2 floats)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float),
                          (void*)(2 * sizeof(float)));  // Offset past position
    glEnableVertexAttribArray(1);

    ...
}
```

### Memory Layout

```
VBO Data:
[x, y, u, v] [x, y, u, v] [x, y, u, v] ...
 ├─pos─┤ ├─tex─┤
 └─────stride─────┘

Attribute 0 (position): offset 0, size 2
Attribute 1 (texcoord): offset 2*sizeof(float), size 2
```

### Element Buffer Object (EBO)

Stores indices to avoid duplicating vertices. Not used in current implementation but would optimize the quad:

```cpp
// Without EBO: 6 vertices for 2 triangles (quad)
// With EBO: 4 vertices + 6 indices
unsigned int indices[] = {
  0, 1, 2,  // First triangle
  0, 3, 1   // Second triangle
};
```

---

## Shaders

Shaders are programs that run on the GPU, written in GLSL (OpenGL Shading Language).

### Vertex Shader (sprite.vert)

Transforms vertex positions from model space to screen space:

```glsl
#version 330 core

layout (location = 0) in vec2 aPos;       // Attribute 0: position
layout (location = 1) in vec2 aTexCoord;  // Attribute 1: texture coord

out vec2 TexCoord;  // Pass to fragment shader

uniform mat4 model;       // Model matrix (position, scale, rotation)
uniform mat4 projection;  // Projection matrix (world to screen)

void main() {
    // Transform position
    gl_Position = projection * model * vec4(aPos, 0.0, 1.0);

    // Pass texture coordinate unchanged
    TexCoord = aTexCoord;
}
```

### Fragment Shader (sprite.frag)

Determines the final color of each pixel:

```glsl
#version 330 core

in vec2 TexCoord;  // From vertex shader

out vec4 FragColor;  // Output color

uniform sampler2D spriteTexture;  // Texture unit

void main() {
    FragColor = texture(spriteTexture, TexCoord);
}
```

### Shader Class Usage

```Shader.h
class Shader {
public:
    GLuint ID;  // Program ID

    Shader(const char* vertexPath, const char* fragmentPath);

    void use() { glUseProgram(ID); }

    void setInt(const std::string& name, int value);
    void setMat4(const std::string& name, const float* value);
};
```

```Sprite.cpp
void Sprite::draw(Shader& shader, int screenWidth, int screenHeight) {
    ...

    // Setting uniforms
    shader.setMat4("projection", projection);
    shader.setMat4("model", model);
    shader.setInt("spriteTexture", 0);  // Texture unit 0

    ...
}
```

### Uniform Variables

Uniforms are global variables passed from CPU to shader:

```Shader.cpp
void Shader::setMat4(const std::string& name, const float* value) {
    GLint location = glGetUniformLocation(ID, name.c_str());
    glUniformMatrix4fv(location, 1, GL_FALSE, value);
}
```

---

## Textures

Textures are images mapped onto geometry.

### Loading a Texture

```Texture.cpp
Texture::Texture(const std::string& filepath) {
    SDL_Surface* surface = IMG_Load(filepath.c_str());

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Upload pixel data
    glTexImage2D(GL_TEXTURE_2D,
                 0,                    // Mipmap level
                 GL_RGBA,              // Internal format
                 surface->w, surface->h,
                 0,                    // Border (must be 0)
                 GL_RGBA,              // Source format
                 GL_UNSIGNED_BYTE,     // Source data type
                 surface->pixels);

    // Set filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    ...
}
```

### Texture Units

OpenGL has multiple texture units (slots) for using multiple textures:

```Texture.cpp
void Texture::bind(unsigned int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);  // Activate unit
    glBindTexture(GL_TEXTURE_2D, textureID);
}
```

In the shader, `sampler2D` reads from a texture unit:

```Sprite.cpp
void Sprite::draw(Shader& shader, int screenWidth, int screenHeight) {
    ...

    shader.setInt("spriteTexture", 0);  // Use texture unit 0
    texture->bind(0);                    // Bind our texture to unit 0

    ...
}
```

### Texture Coordinates

UV coordinates map texture pixels to vertices:

```
(0,1)──────(1,1)
  │          │
  │  IMAGE   │
  │          │
(0,0)──────(1,0)
```

---

## Coordinate Systems and Matrices

### The Problem

- **OpenGL clip space**: -1 to 1 on all axes
- **Screen space**: 0 to width, 0 to height in pixels

We need matrices to transform between them.

### Orthographic Projection Matrix

Maps pixel coordinates to OpenGL's -1 to 1 range (for 2D rendering):

```Sprite.cpp
void Sprite::draw(Shader& shader, int screenWidth, int screenHeight) {
    ...

    float projection[16] = {
        2.0f / screenWidth,  0.0f,                  0.0f, 0.0f,
        0.0f,               -2.0f / screenHeight,   0.0f, 0.0f,
        0.0f,                0.0f,                 -1.0f, 0.0f,
       -1.0f,                1.0f,                  0.0f, 1.0f
    };

    ...
}
```

This matrix:
- Scales X from [0, width] to [-1, 1]
- Scales Y from [0, height] to [1, -1] (flipped - 0 at top)
- Translates origin to top-left corner

### Model Matrix

Positions and scales individual sprites:

```Sprite.cpp
void Sprite::draw(Shader& shader, int screenWidth, int screenHeight) {
    ...

    float model[16] = {
        size.x,     0.0f,       0.0f, 0.0f,  // Scale X
        0.0f,       size.y,     0.0f, 0.0f,  // Scale Y
        0.0f,       0.0f,       1.0f, 0.0f,  // Scale Z (unused in 2D)
        position.x, position.y, 0.0f, 1.0f   // Translation
    };

    ...
}
```

### Matrix Multiplication Order

In the vertex shader:

```glsl
gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
```

Order matters! Applied right-to-left:
1. `aPos` is in model space (0-1 for sprite quad)
2. `model` transforms to world space (position, size)
3. `projection` transforms to clip space (-1 to 1)

---

## Blending and Transparency

### Enabling Blending

```Window.cpp
Window::Window(const std::string& title, int width, int height) {
    ...

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ...
}
```

### Blend Equation

```
finalColor = srcColor * srcFactor + dstColor * dstFactor
```

With `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`:
- `srcFactor = source alpha`
- `dstFactor = 1 - source alpha`

This achieves standard alpha blending:
- Fully opaque (alpha=1): shows source only
- Fully transparent (alpha=0): shows destination only
- Semi-transparent: blends both

---

## The Draw Call

Putting it all together in `Sprite::draw()`:

```Sprite.cpp
void Sprite::draw(Shader& shader, int screenWidth, int screenHeight) {
    shader.use();                              // 1. Activate shader program

    // 2. Set uniforms
    shader.setMat4("projection", projection);
    shader.setMat4("model", model);
    shader.setInt("spriteTexture", 0);

    texture->bind(0);                          // 3. Bind texture to unit 0

    glBindVertexArray(VAO);                    // 4. Bind VAO (vertex config)
    glDrawArrays(GL_TRIANGLES, 0, 6);          // 5. Draw 6 vertices as triangles
    glBindVertexArray(0);                      // 6. Unbind (good practice)

    texture->unbind();                         // 7. Unbind texture
}
```

### glDrawArrays

```cpp
glDrawArrays(GL_TRIANGLES,  // Primitive type
             0,              // Starting index
             6);             // Number of vertices
```

Draws 6 vertices as 2 triangles (forming a quad).

---

## Resource Cleanup

OpenGL resources must be explicitly deleted:

```Sprite.cpp
Sprite::~Sprite() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}
```

```Texture.cpp
Texture::~Texture() {
    glDeleteTextures(1, &textureID);
}
```

```Shader.cpp
Shader::~Shader() {
    glDeleteProgram(ID);
}
```

Using RAII (destructor cleanup) ensures no GPU memory leaks.
