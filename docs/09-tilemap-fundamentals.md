# Tilemap Fundamentals

This document explains the core concepts behind tilemaps - the foundation of terrain rendering in 2D games like Stardew Valley.

## Table of Contents
- [What is a Tilemap?](#what-is-a-tilemap)
- [Why Use Tilemaps?](#why-use-tilemaps)
- [Grid Representation](#grid-representation)
- [Tile Coordinates vs World Coordinates](#tile-coordinates-vs-world-coordinates)
- [Tilemap Layers](#tilemap-layers)
- [Integration with Camera](#integration-with-camera)

---

## What is a Tilemap?

A **tilemap** is a technique for building game worlds from small, reusable image pieces called **tiles**. Instead of creating one massive image for your entire game world, you create a small set of tiles (grass, water, dirt, walls) and arrange them in a grid.

```
Tile Set (Individual Pieces):
┌────┐ ┌────┐ ┌────┐ ┌────┐
│grass│ │water│ │dirt│ │stone│
└────┘ └────┘ └────┘ └────┘
  0       1       2       3

Tilemap (Arranged Grid):
┌────┬────┬────┬────┬────┐
│  0 │  0 │  1 │  1 │  1 │  ← grass, grass, water, water, water
├────┼────┼────┼────┼────┤
│  0 │  2 │  2 │  1 │  1 │  ← grass, dirt, dirt, water, water
├────┼────┼────┼────┼────┤
│  3 │  3 │  2 │  0 │  0 │  ← stone, stone, dirt, grass, grass
└────┴────┴────┴────┴────┘

Rendered Result:
┌────┬────┬────┬────┬────┐
│ ░░ │ ░░ │ ~~ │ ~~ │ ~~ │
├────┼────┼────┼────┼────┤
│ ░░ │ .. │ .. │ ~~ │ ~~ │
├────┼────┼────┼────┼────┤
│ ## │ ## │ .. │ ░░ │ ░░ │
└────┴────┴────┴────┴────┘
```

### Web Development Analogy

Think of a tilemap like a **CSS Grid** or an **HTML table** where each cell references a tile type:

```html
<!-- Web equivalent: A grid where each cell has a class -->
<div class="tilemap" style="display: grid; grid-template-columns: repeat(5, 32px);">
    <div class="tile grass"></div>
    <div class="tile grass"></div>
    <div class="tile water"></div>
    <div class="tile water"></div>
    <div class="tile water"></div>
    <!-- ...more tiles... -->
</div>
```

Or like a **spreadsheet** where each cell contains an ID that references a tile image:

| | A | B | C | D | E |
|---|---|---|---|---|---|
| 1 | 0 | 0 | 1 | 1 | 1 |
| 2 | 0 | 2 | 2 | 1 | 1 |
| 3 | 3 | 3 | 2 | 0 | 0 |

The tilemap is just data (numbers). The **tileset** (texture atlas) contains the actual images those numbers reference.

---

## Why Use Tilemaps?

### 1. Memory Efficiency

A 1600x1200 pixel world stored as a single image:
- **Uncompressed**: 1600 × 1200 × 4 bytes (RGBA) = **7.68 MB**

The same world as a 50x37 tilemap with 32x32 tiles:
- **Tileset texture**: 256 × 256 pixels = **256 KB** (for 64 unique tiles)
- **Tile data**: 50 × 37 × 1 byte = **1.85 KB**
- **Total**: ~**258 KB** (33x smaller!)

### 2. Design Flexibility

Level designers can modify worlds without creating new art:

```
Original Level:                Modified Level (no new art needed):
┌────┬────┬────┬────┐         ┌────┬────┬────┬────┐
│  0 │  0 │  1 │  1 │         │  3 │  3 │  1 │  1 │
│  0 │  2 │  2 │  1 │  →      │  3 │  2 │  2 │  1 │
│  3 │  3 │  2 │  0 │         │  0 │  0 │  2 │  0 │
└────┴────┴────┴────┘         └────┴────┴────┴────┘
Just change the numbers!
```

### 3. GPU Batching

All tiles use the same texture (the tileset), enabling:
- **Single texture bind** for the entire map
- **Batch rendering** (one draw call for many tiles)
- **GPU cache efficiency** (same texture stays in VRAM)

### 4. Collision and Logic

Tile IDs carry meaning beyond visuals:

```cpp
enum TileType {
    GRASS = 0,   // Walkable
    WATER = 1,   // Not walkable (unless swimming)
    DIRT  = 2,   // Walkable
    STONE = 3    // Solid collision
};

bool isWalkable(int tileID) {
    return tileID == GRASS || tileID == DIRT;
}
```

---

## Grid Representation

A tilemap is fundamentally a **2D array** of tile indices.

### 2D Array Representation

The most intuitive representation:

```cpp
// 2D array version (conceptual)
int tiles[height][width];

// Accessing tile at column 3, row 2:
int tileID = tiles[2][3];
```

### 1D Array Representation (Preferred)

In practice, a **flattened 1D array** is used for better cache performance:

```cpp
// 1D array version (preferred)
int* tiles = new int[width * height];

// Access pattern: index = y * width + x
int getTile(int x, int y) {
    return tiles[y * width + x];
}

void setTile(int x, int y, int tileID) {
    tiles[y * width + x] = tileID;
}
```

```
Memory Layout (contiguous):
[row0: 0,1,2,3,4] [row1: 5,6,7,8,9] [row2: 10,11,12,13,14]

Index Formula:
Tile at (x=3, y=1) in a 5-wide map:
index = y * width + x
index = 1 * 5 + 3 = 8
```

### Web Development Analogy

This is similar to how a 1D array represents a 2D canvas in JavaScript:

```javascript
// JavaScript Canvas: 1D array for 2D pixel data
const imageData = ctx.getImageData(0, 0, width, height);
const pixels = imageData.data; // 1D array: [R,G,B,A, R,G,B,A, ...]

// Access pixel at (x, y):
const index = (y * width + x) * 4; // *4 for RGBA
const red = pixels[index];
```

---

## Tile Coordinates vs World Coordinates

### Tile Coordinates

**Tile coordinates** are integer grid positions:

```
Tile Space (integer grid):
      0     1     2     3     4
    ┌─────┬─────┬─────┬─────┬─────┐
  0 │(0,0)│(1,0)│(2,0)│(3,0)│(4,0)│
    ├─────┼─────┼─────┼─────┼─────┤
  1 │(0,1)│(1,1)│(2,1)│(3,1)│(4,1)│
    └─────┴─────┴─────┴─────┴─────┘
```

### World Coordinates

**World coordinates** are floating-point pixel positions:

```
World Space (pixel coordinates, tileSize = 32):
    0      32     64     96     128
    ┌──────┬──────┬──────┬──────┬──────┐
  0 │      │      │      │      │      │
 32 ├──────┼──────┼──────┼──────┼──────┤
    │      │ [P]  │      │      │      │  ← Player at world (50, 45)
 64 └──────┴──────┴──────┴──────┴──────┘

Player at (50.0, 45.0) is within tile (1, 1)
```

### Conversion Formulas

```cpp
// World position to tile coordinate
int worldToTileX(float worldX, int tileSize) {
    return static_cast<int>(worldX / tileSize);
}

int worldToTileY(float worldY, int tileSize) {
    return static_cast<int>(worldY / tileSize);
}

// Tile coordinate to world position (top-left corner of tile)
float tileToWorldX(int tileX, int tileSize) {
    return static_cast<float>(tileX * tileSize);
}

float tileToWorldY(int tileY, int tileSize) {
    return static_cast<float>(tileY * tileSize);
}
```

### Conversion Examples

```
Given: tileSize = 32

Example 1: Player at world (50, 45)
  tileX = 50 / 32 = 1 (integer division)
  tileY = 45 / 32 = 1
  Player is on tile (1, 1)

Example 2: Tile (2, 1) world position
  worldX = 2 * 32 = 64.0
  worldY = 1 * 32 = 32.0
  Tile's top-left corner is at (64, 32)
```

---

## Tilemap Layers

Real games use **multiple tilemap layers** rendered on top of each other.

### Common Layer Structure

```
Layer Stack (rendered bottom to top):

Layer 3: Above-Player (tree tops, overhangs)
    ↓
Layer 2: Objects (furniture, decorations)
    ↓
Layer 1: Ground Details (paths, flowers)
    ↓
Layer 0: Ground Base (grass, water, dirt)
```

### Visual Example

```
Layer 0: Ground          Layer 1: Details         Combined:
┌────┬────┬────┬────┐   ┌────┬────┬────┬────┐   ┌────┬────┬────┬────┐
│ ░░ │ ░░ │ ~~ │ ~~ │   │    │ ** │    │    │   │ ░░ │**░░│ ~~ │ ~~ │
├────┼────┼────┼────┤ + ├────┼────┼────┼────┤ = ├────┼────┼────┼────┤
│ ░░ │ .. │ .. │ ~~ │   │    │    │ ── │    │   │ ░░ │ .. │ ── │ ~~ │
└────┴────┴────┴────┘   └────┴────┴────┴────┘   └────┴────┴────┴────┘
```

### Data Representation

Each layer is a separate array. A special value (-1) indicates "empty/transparent":

```cpp
class LayeredTilemap {
private:
    int width, height, tileSize;
    std::vector<std::vector<int>> layers;  // layers[layerIndex][tileIndex]

public:
    int getTile(int layer, int x, int y) {
        int index = y * width + x;
        return layers[layer][index];
    }

    void render() {
        // Render from bottom layer to top
        for (int layer = 0; layer < layers.size(); layer++) {
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    int tileID = getTile(layer, x, y);
                    if (tileID >= 0) {  // -1 means empty
                        drawTile(x, y, tileID);
                    }
                }
            }
        }
    }
};
```

### Web Development Analogy

Tilemap layers work exactly like **CSS z-index stacking**:

```html
<div class="game-container" style="position: relative;">
    <div class="layer ground" style="z-index: 0;"></div>
    <div class="layer details" style="z-index: 1;"></div>
    <div class="player" style="z-index: 2;"></div>
    <div class="layer above" style="z-index: 3;"></div>
</div>
```

---

## Integration with Camera

The tilemap must integrate with the camera system from `08-camera-systems.md`. Only visible tiles should be rendered.

### Visible Region Calculation

```cpp
void getVisibleTileRange(const Camera& camera, int tileSize,
                         int& startX, int& startY,
                         int& endX, int& endY,
                         int mapWidth, int mapHeight) {
    Vector2 camPos = camera.getPosition();
    int viewWidth = camera.getViewportWidth();
    int viewHeight = camera.getViewportHeight();

    // Calculate world bounds of visible area
    float worldLeft = camPos.x - viewWidth / 2.0f;
    float worldTop = camPos.y - viewHeight / 2.0f;
    float worldRight = camPos.x + viewWidth / 2.0f;
    float worldBottom = camPos.y + viewHeight / 2.0f;

    // Convert to tile coordinates (with 1-tile buffer)
    startX = std::max(0, static_cast<int>(worldLeft / tileSize) - 1);
    startY = std::max(0, static_cast<int>(worldTop / tileSize) - 1);
    endX = std::min(mapWidth, static_cast<int>(worldRight / tileSize) + 2);
    endY = std::min(mapHeight, static_cast<int>(worldBottom / tileSize) + 2);
}
```

### Why Culling Matters

```
Full Map: 100x80 tiles
Viewport: 25x19 tiles visible

Without culling: 8000 tiles rendered per frame
With culling: ~500 tiles rendered per frame (16x faster!)
```

---

## Summary

| Concept | Purpose |
|---------|---------|
| Tilemap | Grid of tile IDs referencing a tileset |
| Tile coordinates | Integer grid positions (x, y) |
| World coordinates | Floating-point pixel positions |
| Conversion | `tileX = worldX / tileSize` |
| Layers | Multiple grids rendered in order for depth |
| Culling | Only render tiles visible to camera |

### Key Formulas

```cpp
// Coordinate conversions
tileX = static_cast<int>(worldX / tileSize);
worldX = tileX * tileSize;

// 1D array indexing
index = y * width + x;
```

### Next Steps

- **10-texture-atlas.md**: How to store all tiles in one texture and calculate UVs
- **11-terrain-architecture.md**: Why Tilemap is NOT an Entity
- **12-rendering-optimization.md**: Batch rendering tiles for performance
