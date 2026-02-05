# Texture Atlases and Tilesets

This document explains how texture atlases work - the technique that allows tilemaps to render efficiently by storing all tiles in a single texture.

## Table of Contents
- [What is a Texture Atlas?](#what-is-a-texture-atlas)
- [Why Use Texture Atlases?](#why-use-texture-atlases)
- [UV Coordinate Calculation](#uv-coordinate-calculation)
- [Texture Bleeding and Solutions](#texture-bleeding-and-solutions)
- [Atlas Organization](#atlas-organization)
- [Integration with Existing Code](#integration-with-existing-code)

---

## What is a Texture Atlas?

A **texture atlas** (also called a **sprite sheet** or **tileset**) is a single large texture that contains multiple smaller images arranged in a grid or packed together.

```
Individual Textures (Inefficient):        Texture Atlas (Efficient):
┌────┐ ┌────┐ ┌────┐ ┌────┐              ┌────┬────┬────┬────┐
│grass│ │water│ │dirt│ │stone│            │grass│water│dirt│stone│
└────┘ └────┘ └────┘ └────┘              ├────┼────┼────┼────┤
  ↓       ↓       ↓       ↓               │path│tree│rock│bush │
 bind    bind    bind    bind             ├────┼────┼────┼────┤
  ↓       ↓       ↓       ↓               │wall│door│roof│fence│
 draw    draw    draw    draw             └────┴────┴────┴────┘
                                                    ↓
4 texture binds, 4 draw calls              1 texture bind, can batch draws
```

### Web Development Analogy

This is exactly the same as **CSS sprite sheets** used in web development:

```css
/* Web: CSS Sprite Sheet */
.icon {
    background-image: url('icons-spritesheet.png');
    width: 32px;
    height: 32px;
}

.icon-home {
    background-position: 0px 0px;    /* First icon */
}

.icon-settings {
    background-position: -32px 0px;  /* Second icon */
}

.icon-user {
    background-position: -64px 0px;  /* Third icon */
}
```

The `background-position` property selects which part of the sprite sheet to show - this is conceptually identical to UV coordinates in OpenGL.

---

## Why Use Texture Atlases?

### 1. Reduced Texture Binds

Changing the active texture (`glBindTexture`) is expensive:

```cpp
// Without atlas: Bind texture for EACH tile (slow)
for (int i = 0; i < 1000; i++) {
    int tileType = tiles[i];
    textures[tileType]->bind();  // GPU state change!
    drawTile(i);
}

// With atlas: Bind ONCE for all tiles (fast)
tilesetTexture->bind();  // Single bind
for (int i = 0; i < 1000; i++) {
    drawTile(i);  // Just change UV coordinates
}
```

### 2. GPU Texture Cache

Modern GPUs cache recently used textures. A single atlas:
- Stays in cache (it's always the same texture)
- Has better memory locality
- Reduces VRAM fragmentation

### 3. Enables Batch Rendering

With all tiles in one texture, you can combine multiple tiles into a single draw call:

```cpp
// Without atlas: 1000 draw calls
for (tile : tiles) {
    textures[tile.type]->bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);  // One quad
}

// With atlas + batching: 1 draw call
tilesetTexture->bind();
glDrawArrays(GL_TRIANGLES, 0, 6000);  // 1000 quads combined!
```

### Performance Comparison

| Approach | Texture Binds | Draw Calls | Relative Speed |
|----------|---------------|------------|----------------|
| Individual textures | 1000 | 1000 | 1x (baseline) |
| Atlas, no batching | 1 | 1000 | ~2-3x faster |
| Atlas + batching | 1 | 1 | ~10-50x faster |

---

## UV Coordinate Calculation

**UV coordinates** (also called texture coordinates) specify which part of a texture to sample. They range from 0.0 to 1.0:

```
Texture (256x256 pixels):
(0,0)────────────────────(1,0)
  │                        │
  │                        │
  │                        │
(0,1)────────────────────(1,1)

UV (0,0) = top-left pixel
UV (1,1) = bottom-right pixel
UV (0.5, 0.5) = center pixel
```

### Current Sprite UV Coordinates

In our current `Sprite.cpp`, we use the full texture (0-1 range):

```cpp
// Sprite.cpp - setupMesh()
float vertices[] = {
    // pos      // tex (UV coordinates)
    0.0f, 1.0f, 0.0f, 1.0f, // top-left     → UV (0,1)
    1.0f, 0.0f, 1.0f, 0.0f, // bottom-right → UV (1,0)
    0.0f, 0.0f, 0.0f, 0.0f, // bottom-left  → UV (0,0)
    0.0f, 1.0f, 0.0f, 1.0f, // top-left     → UV (0,1)
    1.0f, 1.0f, 1.0f, 1.0f, // top-right    → UV (1,1)
    1.0f, 0.0f, 1.0f, 0.0f, // bottom-right → UV (1,0)
};
```

This maps the **entire texture** onto the quad. For a tileset, we need to map just a **sub-region**.

### Calculating Tile UVs

Given a tileset with tiles arranged in a grid:

```
Tileset (256x256 pixels, 32x32 tiles = 8x8 grid):
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │ 6  │ 7  │  Row 0
├────┼────┼────┼────┼────┼────┼────┼────┤
│ 8  │ 9  │ 10 │ 11 │ 12 │ 13 │ 14 │ 15 │  Row 1
├────┼────┼────┼────┼────┼────┼────┼────┤
│ 16 │ 17 │ 18 │ 19 │ 20 │ 21 │ 22 │ 23 │  Row 2
├────┼────┼────┼────┼────┼────┼────┼────┤
│... │    │    │    │    │    │    │    │
└────┴────┴────┴────┴────┴────┴────┴────┘
```

The UV calculation formulas:

```cpp
// Given:
int tileID;           // The tile to render (e.g., 10)
int tileSize;         // Size of each tile in pixels (e.g., 32)
int atlasWidth;       // Atlas width in pixels (e.g., 256)
int atlasHeight;      // Atlas height in pixels (e.g., 256)

// Calculate tiles per row
int tilesPerRow = atlasWidth / tileSize;  // 256/32 = 8

// Get tile's grid position
int tileX = tileID % tilesPerRow;  // 10 % 8 = 2 (column)
int tileY = tileID / tilesPerRow;  // 10 / 8 = 1 (row)

// Calculate UV coordinates (normalized 0-1 range)
float u0 = (tileX * tileSize) / static_cast<float>(atlasWidth);
float v0 = (tileY * tileSize) / static_cast<float>(atlasHeight);
float u1 = ((tileX + 1) * tileSize) / static_cast<float>(atlasWidth);
float v1 = ((tileY + 1) * tileSize) / static_cast<float>(atlasHeight);

// For tile 10:
// tileX = 2, tileY = 1
// u0 = (2 * 32) / 256 = 64/256 = 0.25
// v0 = (1 * 32) / 256 = 32/256 = 0.125
// u1 = (3 * 32) / 256 = 96/256 = 0.375
// v1 = (2 * 32) / 256 = 64/256 = 0.25
```

### Visual UV Example

```
Tile 10 in a 256x256 atlas (8x8 grid of 32px tiles):

Full atlas UV space:
(0,0)──────────────────────────────────────(1,0)
  │    0    1    2    3    4    5    6    7  │
  │  ┌────┬────┬────┬────┬────┬────┬────┬────┤
  │  │    │    │    │    │    │    │    │    │ 0
  │  ├────┼────╔════╗────┼────┼────┼────┼────┤
  │  │    │    ║ 10 ║    │    │    │    │    │ 1
  │  ├────┼────╚════╝────┼────┼────┼────┼────┤
  │  │    │    │    │    │    │    │    │    │ 2
(0,1)──────────────────────────────────────(1,1)

Tile 10 UV bounds:
  u0 = 0.25   (left edge)
  u1 = 0.375  (right edge)
  v0 = 0.125  (top edge)
  v1 = 0.25   (bottom edge)
```

### Web Development Analogy

The UV calculation is identical to `background-position` in CSS, but normalized:

```javascript
// CSS uses pixels:
backgroundPosition: `-${tileX * tileSize}px -${tileY * tileSize}px`

// OpenGL uses normalized (0-1) coordinates:
u = (tileX * tileSize) / atlasWidth;
v = (tileY * tileSize) / atlasHeight;
```

---

## Texture Bleeding and Solutions

**Texture bleeding** (or "texture seams") occurs when pixels from adjacent tiles in the atlas bleed into each other during rendering.

### The Problem

```
What you expect:           What you might see:
┌────┬────┐               ┌────┬────┐
│grass│water│              │gras│wate│  ← Thin line of wrong color
├────┼────┤               │s───│r───│     at tile boundaries
│dirt│stone│              │dirt│ston│
└────┴────┘               └────┴────┘
```

### Why It Happens

1. **Floating-point precision**: UV coordinates are floats, leading to tiny rounding errors
2. **Texture filtering**: `GL_LINEAR` interpolates between adjacent pixels
3. **Mipmapping**: Lower mipmap levels blend adjacent tiles together

### Solution 1: Use GL_NEAREST Filtering

For pixel art games, disable interpolation:

```cpp
// Texture.cpp - Use nearest-neighbor filtering
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
```

```
GL_LINEAR (smooth, causes bleeding):    GL_NEAREST (crisp, no bleeding):
┌──────────────┐                        ┌──────────────┐
│ ░░░▒▒▒▓▓▓███ │ ← Smooth gradients     │ ░░░░░░██████ │ ← Sharp pixels
│ ░░░▒▒▒▓▓▓███ │                        │ ░░░░░░██████ │
│ ░░░▒▒▒▓▓▓███ │                        │ ░░░░░░██████ │
└──────────────┘                        └──────────────┘
```

### Solution 2: Add Padding Between Tiles

Add 1-2 pixels of transparent or edge-color padding between tiles:

```
Without padding:              With 1px padding:
┌────┬────┬────┐             ┌────┐ ┌────┐ ┌────┐
│ A  │ B  │ C  │             │ A  │ │ B  │ │ C  │
├────┼────┼────┤             └────┘ └────┘ └────┘
│ D  │ E  │ F  │
└────┴────┴────┘             ┌────┐ ┌────┐ ┌────┐
                             │ D  │ │ E  │ │ F  │
                             └────┘ └────┘ └────┘
```

Adjust UV calculations to account for padding:

```cpp
int padding = 1;  // 1 pixel between tiles

// Adjusted UV calculation
float u0 = (tileX * (tileSize + padding) + padding) / static_cast<float>(atlasWidth);
float v0 = (tileY * (tileSize + padding) + padding) / static_cast<float>(atlasHeight);
float u1 = u0 + tileSize / static_cast<float>(atlasWidth);
float v1 = v0 + tileSize / static_cast<float>(atlasHeight);
```

### Solution 3: Extrude Tile Edges

Duplicate the edge pixels of each tile outward:

```
Original tile:          Extruded (1px border):
┌────────┐              ┌──────────┐
│ABCDEFGH│              │AABCDEFGHH│
│IJKLMNOP│              │AABCDEFGHH│
│QRSTUVWX│              │IIJKLMNOPP│
└────────┘              │QQRSTUVWXX│
                        │QQRSTUVWXX│
                        └──────────┘
```

This way, when filtering samples beyond the tile edge, it gets the same color.

### Solution 4: Half-Pixel Offset

Inset UV coordinates by half a pixel to sample tile centers:

```cpp
float halfPixel = 0.5f / atlasWidth;

float u0 = (tileX * tileSize) / static_cast<float>(atlasWidth) + halfPixel;
float v0 = (tileY * tileSize) / static_cast<float>(atlasHeight) + halfPixel;
float u1 = ((tileX + 1) * tileSize) / static_cast<float>(atlasWidth) - halfPixel;
float v1 = ((tileY + 1) * tileSize) / static_cast<float>(atlasHeight) - halfPixel;
```

### Recommended Approach for Pixel Art

For Stardew Valley-style pixel art:

1. Use `GL_NEAREST` filtering (no interpolation)
2. Keep tiles tightly packed (no padding needed with GL_NEAREST)
3. Ensure atlas dimensions are powers of 2 (256, 512, 1024)

---

## Atlas Organization

### Grid-Based Layout

The simplest organization - tiles arranged in a regular grid:

```
256x256 atlas with 32x32 tiles (8x8 = 64 tiles):

ID:  0   1   2   3   4   5   6   7
   ┌───┬───┬───┬───┬───┬───┬───┬───┐
 0 │ G │ G │ G │ W │ W │ W │ D │ D │  G = Grass variants
   ├───┼───┼───┼───┼───┼───┼───┼───┤  W = Water variants
 8 │ S │ S │ P │ P │ P │ P │ T │ T │  D = Dirt, S = Stone
   ├───┼───┼───┼───┼───┼───┼───┼───┤  P = Path, T = Tree
16 │ F │ F │ F │ F │ H │ H │ H │ H │  F = Flowers, H = House
   └───┴───┴───┴───┴───┴───┴───┴───┘
```

### Logical Grouping

Group related tiles together for easier management:

```
Rows 0-1: Ground tiles (grass, dirt, water, sand)
Rows 2-3: Path tiles (stone path, wood path, edges)
Rows 4-5: Nature (trees, bushes, flowers, rocks)
Rows 6-7: Buildings (walls, roofs, doors, windows)
```

### Auto-Tile Layouts

For tiles that connect (like walls or paths), arrange them in a standard pattern:

```
Wang Tiles / Blob Pattern (common in RPG Maker):
┌───┬───┬───┬───┐
│ TL│ T │ TR│ ● │  TL = Top-left corner
├───┼───┼───┼───┤  T  = Top edge
│ L │ C │ R │   │  TR = Top-right corner
├───┼───┼───┼───┤  L  = Left edge
│ BL│ B │ BR│   │  C  = Center (filled)
├───┼───┼───┼───┤  R  = Right edge
│   │   │   │   │  B  = Bottom edge
└───┴───┴───┴───┘  ● = Single/isolated
```

---

## Integration with Existing Code

### Current Texture Class

Our existing `Texture` class loads a single image:

```cpp
// Texture.cpp (existing)
Texture::Texture(const std::string& filepath) {
    SDL_Surface* surface = IMG_Load(filepath.c_str());

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 surface->w, surface->h,
                 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 surface->pixels);

    // Current: GL_LINEAR (smooth)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
```

### For Pixel Art Tilesets

When loading a tileset, use `GL_NEAREST`:

```cpp
// Conceptual: Tileset-specific texture loading
Texture::Texture(const std::string& filepath, bool pixelArt) {
    // ... load image ...

    if (pixelArt) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
}
```

### Tileset Helper Class (Conceptual)

```cpp
class Tileset {
private:
    Texture* texture;    // The atlas texture
    int tileSize;        // Size of each tile (e.g., 32)
    int tilesPerRow;     // Calculated from texture width
    int atlasWidth;
    int atlasHeight;

public:
    Tileset(Texture* tex, int tileSize)
        : texture(tex), tileSize(tileSize) {
        atlasWidth = texture->getWidth();
        atlasHeight = texture->getHeight();
        tilesPerRow = atlasWidth / tileSize;
    }

    // Get UV coordinates for a tile ID
    void getTileUVs(int tileID, float& u0, float& v0, float& u1, float& v1) {
        int tileX = tileID % tilesPerRow;
        int tileY = tileID / tilesPerRow;

        u0 = (tileX * tileSize) / static_cast<float>(atlasWidth);
        v0 = (tileY * tileSize) / static_cast<float>(atlasHeight);
        u1 = ((tileX + 1) * tileSize) / static_cast<float>(atlasWidth);
        v1 = ((tileY + 1) * tileSize) / static_cast<float>(atlasHeight);
    }

    void bind(int unit = 0) {
        texture->bind(unit);
    }
};
```

### Using UVs in Rendering

Instead of hardcoded 0-1 UVs, pass calculated UVs:

```cpp
// Conceptual tile rendering
void drawTile(int tileID, float worldX, float worldY, Tileset& tileset, Shader& shader) {
    float u0, v0, u1, v1;
    tileset.getTileUVs(tileID, u0, v0, u1, v1);

    // Update vertex buffer with tile-specific UVs
    float vertices[] = {
        // pos              // tex
        0.0f, 1.0f,         u0, v1,  // top-left
        1.0f, 0.0f,         u1, v0,  // bottom-right
        0.0f, 0.0f,         u0, v0,  // bottom-left
        0.0f, 1.0f,         u0, v1,  // top-left
        1.0f, 1.0f,         u1, v1,  // top-right
        1.0f, 0.0f,         u1, v0,  // bottom-right
    };

    // ... update VBO, set model matrix, draw ...
}
```

---

## Summary

| Concept | Purpose |
|---------|---------|
| Texture Atlas | Single texture containing multiple tiles |
| UV Coordinates | Normalized (0-1) coordinates to sample texture sub-regions |
| Tile UV Formula | `u = (tileX * tileSize) / atlasWidth` |
| Texture Bleeding | Adjacent tile colors leaking at boundaries |
| GL_NEAREST | Pixel-perfect filtering for pixel art |

### Key Formulas

```cpp
// Tile position in atlas grid
int tileX = tileID % tilesPerRow;
int tileY = tileID / tilesPerRow;

// UV coordinates
float u0 = (tileX * tileSize) / atlasWidth;
float v0 = (tileY * tileSize) / atlasHeight;
float u1 = ((tileX + 1) * tileSize) / atlasWidth;
float v1 = ((tileY + 1) * tileSize) / atlasHeight;
```

### Next Steps

- **11-terrain-architecture.md**: How to structure the Tilemap class (separate from Entity)
- **12-rendering-optimization.md**: Batch rendering with atlases
