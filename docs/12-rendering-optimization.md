# Rendering Optimization

This document explains the performance concepts behind efficient rendering, particularly for tilemaps. Understanding these concepts helps you know when optimization matters and what techniques to consider.

## Table of Contents
- [Draw Call Overhead](#draw-call-overhead)
- [What Happens During a Draw Call](#what-happens-during-a-draw-call)
- [Batch Rendering](#batch-rendering)
- [Instanced Rendering](#instanced-rendering)
- [When to Optimize](#when-to-optimize)
- [Practical Guidelines](#practical-guidelines)

---

## Draw Call Overhead

A **draw call** is a command that tells the GPU to render geometry. In OpenGL, this is typically `glDrawArrays()` or `glDrawElements()`.

### Current Rendering (Per-Entity)

Our current `Sprite::draw()` makes one draw call per sprite:

```cpp
// Sprite.cpp (existing)
void Sprite::draw(Shader& shader, const Camera& camera) {
    shader.use();

    // ... set projection, view, model matrices ...

    texture->bind(0);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);  // ← One draw call per sprite
    glBindVertexArray(0);

    texture->unbind();
}
```

### The Problem with Many Draw Calls

Each draw call has overhead:

```
CPU Work per draw call:
1. Validate OpenGL state
2. Prepare command buffer
3. Send command to GPU driver
4. Driver translates to GPU commands
5. Context switch to GPU

Time: ~0.05-0.5ms per draw call (varies by GPU/driver)
```

For 10 sprites: 10 × 0.1ms = **1ms** (fine)
For 2000 tiles: 2000 × 0.1ms = **200ms** (disaster! Only 5 FPS)

### Visual Breakdown

```
Frame with 2000 individual draw calls:

CPU: [prepare][prepare][prepare][prepare]...(2000 times)...[prepare]
     └────────────── Most time spent here! ───────────────────┘

GPU: [draw][draw][draw]...(fast but waiting for CPU)...[draw]
     └─ GPU is actually fast, but CPU can't feed it commands ─┘

Result: CPU-bound, GPU sits idle much of the time
```

### Web Development Analogy

Draw call overhead is like **HTTP request overhead** in web development:

```javascript
// BAD: 1000 HTTP requests (massive overhead)
for (let i = 0; i < 1000; i++) {
    fetch(`/api/item/${i}`);  // Each request has TCP overhead, headers, etc.
}

// GOOD: 1 batched request
fetch('/api/items', { body: JSON.stringify(ids) });  // Single request, all data
```

Just as you batch API calls to reduce HTTP overhead, you batch draw calls to reduce GPU command overhead.

---

## What Happens During a Draw Call

Understanding the full cost helps appreciate why batching matters.

### State Changes

Before each draw call, you often change **GPU state**:

```cpp
// Common state changes (each has cost)
shader.use();                    // Switch shader program
texture->bind(0);                // Switch bound texture
shader.setMat4("model", model);  // Update uniform
glBindVertexArray(VAO);          // Switch vertex configuration
```

### Cost Hierarchy

Not all state changes are equal:

```
Most Expensive (avoid frequent changes):
├── Shader program switch      ~0.1-0.5ms
├── Framebuffer switch         ~0.1-0.3ms
├── Texture bind               ~0.01-0.1ms
│
Medium Cost:
├── Uniform updates            ~0.001-0.01ms
├── VAO bind                   ~0.001-0.01ms
│
Cheap:
└── glDrawArrays() itself      ~0.001ms (surprisingly cheap!)
```

### Key Insight

The draw call (`glDrawArrays`) is not the expensive part - it's the **state changes** and **CPU-GPU synchronization** around it.

```cpp
// Expensive pattern (per tile):
for (tile : tiles) {
    texture->bind();           // State change!
    shader.setMat4("model");   // Uniform update!
    glBindVertexArray(VAO);    // State change!
    glDrawArrays(...);         // Cheap
}

// Cheaper pattern:
texture->bind();               // Once
glBindVertexArray(VAO);        // Once
for (tile : tiles) {
    shader.setMat4("model");   // Just uniform updates
    glDrawArrays(...);
}
```

---

## Batch Rendering

**Batch rendering** combines multiple objects into a single draw call by merging their vertex data.

### The Concept

Instead of:
```
Draw call 1: 6 vertices (tile 0)
Draw call 2: 6 vertices (tile 1)
Draw call 3: 6 vertices (tile 2)
...
Draw call 1000: 6 vertices (tile 999)

Total: 1000 draw calls
```

Batch into:
```
Draw call 1: 6000 vertices (all 1000 tiles combined)

Total: 1 draw call
```

### How It Works

1. **Collect all visible tiles**
2. **Build combined vertex buffer** with all tile vertices
3. **Upload to GPU once**
4. **Single draw call**

```cpp
// Conceptual batch rendering
void Tilemap::renderBatched(Shader& shader, const Camera& camera) {
    // 1. Calculate visible tile range
    int startX, startY, endX, endY;
    getVisibleRange(camera, startX, startY, endX, endY);

    // 2. Build combined vertex data
    std::vector<float> vertices;
    vertices.reserve((endX - startX) * (endY - startY) * 6 * 4);  // 6 verts × 4 floats

    for (int y = startY; y < endY; y++) {
        for (int x = startX; x < endX; x++) {
            int tileID = getTile(x, y);
            if (tileID >= 0) {
                addTileVertices(vertices, x, y, tileID);
            }
        }
    }

    // 3. Upload to GPU
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(float),
                 vertices.data(),
                 GL_DYNAMIC_DRAW);  // Dynamic because it changes each frame

    // 4. Single draw call for all tiles!
    shader.use();
    shader.setMat4("projection", projection);
    shader.setMat4("view", camera.getViewMatrix());
    tileset->bind(0);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 4);  // All tiles at once!
}
```

### Adding Tile Vertices

Each tile becomes 6 vertices (2 triangles) with world position and UV coordinates:

```cpp
void Tilemap::addTileVertices(std::vector<float>& vertices,
                               int tileX, int tileY, int tileID) {
    // World position of tile
    float worldX = tileX * tileSize;
    float worldY = tileY * tileSize;

    // UV coordinates from tileset
    float u0, v0, u1, v1;
    getTileUVs(tileID, u0, v0, u1, v1);

    // 6 vertices for quad (2 triangles)
    // Top-left triangle
    vertices.insert(vertices.end(), {
        worldX,             worldY + tileSize,  u0, v1,  // top-left
        worldX + tileSize,  worldY,             u1, v0,  // bottom-right
        worldX,             worldY,             u0, v0,  // bottom-left
    });

    // Bottom-right triangle
    vertices.insert(vertices.end(), {
        worldX,             worldY + tileSize,  u0, v1,  // top-left
        worldX + tileSize,  worldY + tileSize,  u1, v1,  // top-right
        worldX + tileSize,  worldY,             u1, v0,  // bottom-right
    });
}
```

### Performance Comparison

| Method | Draw Calls | Time (estimate) |
|--------|------------|-----------------|
| Per-tile draw | 2000 | ~100-200ms |
| Batched | 1 | ~1-5ms |

### Trade-offs

**Pros:**
- Dramatically fewer draw calls
- GPU stays busy (not waiting for CPU)
- Massive performance improvement

**Cons:**
- Must rebuild vertex buffer when tiles change
- Higher memory usage (full vertex data vs. indices)
- More complex code

---

## Instanced Rendering

**Instanced rendering** is an even more efficient technique where you define geometry once and provide per-instance data.

### The Concept

Instead of duplicating vertex data for each tile:
```
Regular batching:
Vertex buffer: [tile0 verts][tile1 verts][tile2 verts]...(6000 vertices)
```

Instance rendering:
```
Vertex buffer: [one quad - 6 vertices]
Instance buffer: [pos0, uv0][pos1, uv1][pos2, uv2]...(1000 instances)
```

### How It Works

1. **Define base quad once** (6 vertices, 0-1 range)
2. **Create instance buffer** with per-tile data (position, UV offset)
3. **GPU duplicates geometry** for each instance automatically

```cpp
// Conceptual instanced rendering setup
void Tilemap::setupInstanced() {
    // Base quad (same as Sprite, defined once)
    float quadVertices[] = {
        // pos      // tex
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
    };

    // Instance data: per-tile position and UV offset
    struct TileInstance {
        glm::vec2 worldPos;   // Where to place this tile
        glm::vec2 uvOffset;   // Which tile in the atlas
        glm::vec2 uvSize;     // Size in UV space
    };
}
```

### Vertex Shader for Instancing

```glsl
// instanced_tile.vert
#version 330 core

layout (location = 0) in vec2 aPos;       // Base quad position (0-1)
layout (location = 1) in vec2 aTexCoord;  // Base quad UVs (0-1)

// Per-instance data
layout (location = 2) in vec2 aWorldPos;  // Tile world position
layout (location = 3) in vec2 aUVOffset;  // Tile UV offset in atlas
layout (location = 4) in vec2 aUVSize;    // Tile UV size

out vec2 TexCoord;

uniform mat4 projection;
uniform mat4 view;
uniform float tileSize;  // World size of tiles

void main() {
    // Scale base quad to tile size and offset to world position
    vec2 worldPos = aPos * tileSize + aWorldPos;

    gl_Position = projection * view * vec4(worldPos, 0.0, 1.0);

    // Map base UVs (0-1) to tile's region in atlas
    TexCoord = aUVOffset + aTexCoord * aUVSize;
}
```

### The Draw Call

```cpp
// Single instanced draw call for all visible tiles
glDrawArraysInstanced(GL_TRIANGLES, 0, 6, visibleTileCount);
//                                   ↑    ↑
//                          6 vertices  1000 instances
```

### Performance Comparison

| Method | Vertex Data | Draw Calls | Memory |
|--------|-------------|------------|--------|
| Per-tile | 6 × 1000 = 6000 verts | 1000 | High |
| Batched | 6 × 1000 = 6000 verts | 1 | High |
| Instanced | 6 verts + 1000 instances | 1 | Lower |

### When to Use Instancing

Instancing shines when:
- Rendering **many identical objects** (tiles, particles, grass)
- Objects differ only in **position, scale, or simple properties**
- You want **minimal memory** for large counts

---

## When to Optimize

**Premature optimization is the root of all evil.** - Donald Knuth

### Measure First

Before optimizing, measure your actual performance:

```cpp
// Simple frame time measurement
auto start = std::chrono::high_resolution_clock::now();
tilemap.render(shader, camera);
auto end = std::chrono::high_resolution_clock::now();

float ms = std::chrono::duration<float, std::milli>(end - start).count();
std::cout << "Tilemap render: " << ms << "ms" << std::endl;
```

### Performance Targets

| Frame Time | FPS | Status |
|------------|-----|--------|
| < 16.67ms | 60+ | Excellent |
| 16.67-33.33ms | 30-60 | Acceptable |
| > 33.33ms | < 30 | Needs optimization |

### Decision Matrix

```
Is rendering slow? (< 30 FPS)
├── No → Don't optimize! Keep code simple.
└── Yes → Is it draw-call bound?
    ├── No → Profile other bottlenecks (physics, AI, etc.)
    └── Yes → Consider batching
        ├── Static tilemap? → Pre-build batch at load time
        └── Dynamic tilemap? → Rebuild batch each frame (still fast)
            └── Still slow? → Consider instancing
```

### Start Simple

For a learning project, start with the **simplest working solution**:

```cpp
// Phase 1: Per-tile rendering (simple, correct)
for (int y = startY; y < endY; y++) {
    for (int x = startX; x < endX; x++) {
        int tileID = getTile(x, y);
        if (tileID >= 0) {
            renderTile(x, y, tileID);
        }
    }
}

// Only if this is too slow (measure!), move to Phase 2
```

---

## Practical Guidelines

### For This Project (Learning Focus)

1. **Start with per-tile rendering** - Understand the basics first
2. **Optimize texture binds** - One bind per tileset, not per tile
3. **Cull invisible tiles** - Huge win with minimal complexity
4. **Add batching later** - Only if needed

### Recommended Approach

```cpp
// Level 1: Simple but smart (good enough for most 2D games)
void Tilemap::render(Shader& shader, const Camera& camera) {
    // Calculate visible range (culling)
    int startX, startY, endX, endY;
    getVisibleRange(camera, startX, startY, endX, endY);

    // Setup state ONCE
    shader.use();
    shader.setMat4("projection", projection);
    shader.setMat4("view", camera.getViewMatrix());
    tileset->bind(0);
    glBindVertexArray(VAO);

    // Render visible tiles
    for (int y = startY; y < endY; y++) {
        for (int x = startX; x < endX; x++) {
            int tileID = getTile(x, y);
            if (tileID >= 0) {
                // Only update what changes per tile
                updateTileUniforms(x, y, tileID, shader);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }
    }
}
```

This is often **good enough** for:
- ~500-2000 visible tiles
- 60 FPS on modern hardware
- Clear, maintainable code

### When You Need More

If profiling shows rendering is your bottleneck:

1. **Batch visible tiles** into one draw call
2. **Cache batched geometry** for static maps
3. **Use instancing** for massive tile counts
4. **Consider chunking** - divide map into 16x16 chunks, batch per chunk

---

## Summary

| Technique | Draw Calls | Complexity | When to Use |
|-----------|------------|------------|-------------|
| Per-tile | N | Low | Prototyping, small maps |
| State-optimized per-tile | N | Low | Most 2D games |
| Batched | 1 | Medium | Large maps, many tiles |
| Instanced | 1 | High | Massive counts, particles |

### Key Principles

1. **Measure before optimizing** - Know where time is spent
2. **Reduce state changes** - Bind textures/shaders once
3. **Cull invisible geometry** - Don't render what you can't see
4. **Batch when needed** - Combine draw calls for performance
5. **Keep it simple** - Complexity has maintenance cost

### Performance Hierarchy

```
Most Important (always do):
├── Cull invisible tiles
├── Single texture bind per tileset
├── Minimize shader/VAO switches
│
Nice to Have (if needed):
├── Batch rendering
├── Geometry caching
│
Advanced (rarely needed for 2D):
└── Instanced rendering
```

### Next Steps

- **13-hd2d-aesthetics.md**: How to add visual effects on top of efficient terrain
