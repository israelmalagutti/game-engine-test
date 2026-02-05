# Autotiling

This document explains autotiling — the technique for automatically selecting tile variants based on neighboring tiles to create seamless terrain transitions.

## Table of Contents
- [What is Autotiling?](#what-is-autotiling)
- [The Bitmask Approach](#the-bitmask-approach)
- [4-Neighbor (Cardinal) Autotiling](#4-neighbor-cardinal-autotiling)
- [8-Neighbor (With Corners) Autotiling](#8-neighbor-with-corners-autotiling)
- [When to Calculate](#when-to-calculate)
- [Tileset Organization](#tileset-organization)

---

## What is Autotiling?

**Autotiling** automatically selects the correct tile variant based on what tiles surround it. Instead of manually placing edge pieces, corners, and transitions, the system figures it out.

```
Without Autotiling (manual placement):
Designer must place each edge/corner piece by hand

With Autotiling (automatic):
Designer places "water" → system picks correct edge/corner variant
```

### Visual Example

```
Logical tile data:          Rendered result (autotiled):
┌───┬───┬───┬───┐           ┌───┬───┬───┬───┐
│ G │ G │ W │ W │           │ ░ │ ░ │┌~~│~~~│
├───┼───┼───┼───┤           ├───┼───┼───┼───┤
│ G │ W │ W │ W │     →     │ ░ │┌~~│~~~│~~~│
├───┼───┼───┼───┤           ├───┼───┼───┼───┤
│ G │ W │ W │ G │           │ ░ │└~~│~~┘│ ░ │
└───┴───┴───┴───┘           └───┴───┴───┴───┘

G = Grass (type 0)          Edges and corners automatically selected
W = Water (type 1)          based on neighbor relationships
```

### Web Development Analogy

Think of CSS border properties. You don't have one "bordered box" — you have combinations:

```css
/* Each combination is a different "variant" */
.corner-tl { border-radius: 8px 0 0 0; }
.corner-tr { border-radius: 0 8px 0 0; }
.corner-tl-tr { border-radius: 8px 8px 0 0; }
.all-corners { border-radius: 8px; }
/* ...16 combinations for 4 corners */
```

Autotiling systematically picks which "CSS class" (tile variant) to use based on neighbors.

---

## The Bitmask Approach

The most common autotiling technique assigns a **bit value** to each neighbor direction. Sum the bits where neighbors match your tile type to get a unique index.

### Why Bitmasks?

Each neighbor is either "same type" (1) or "different type" (0). With 4 neighbors, that's 4 bits = 16 combinations. With 8 neighbors, 8 bits = 256 combinations (though reduced by corner rules).

A bitmask elegantly encodes all combinations as a single integer that directly indexes your tileset.

---

## 4-Neighbor (Cardinal) Autotiling

The simpler system — only checks North, East, South, West.

### Bit Assignments

```
        [1] North
          ↓
[8] West  ◼  East [2]
          ↑
        [4] South

Bit values (powers of 2):
- North = 1  (2^0)
- East  = 2  (2^1)
- South = 4  (2^2)
- West  = 8  (2^3)
```

### Calculating the Bitmask

```cpp
int calculateBitmask4(int x, int y, int tileType) {
    int bitmask = 0;

    // Check each cardinal neighbor
    if (getTile(x, y - 1) == tileType) bitmask += 1;  // North
    if (getTile(x + 1, y) == tileType) bitmask += 2;  // East
    if (getTile(x, y + 1) == tileType) bitmask += 4;  // South
    if (getTile(x - 1, y) == tileType) bitmask += 8;  // West

    return bitmask;  // Returns 0-15
}
```

### Example Calculations

```
Example 1: Water surrounded by grass on all sides
  [G]
[G] W [G]    →  No neighbors match  →  bitmask = 0 (isolated)
  [G]

Example 2: Water with water to North and East
  [W]        N matches: +1
[G] W [W]    E matches: +2         →  bitmask = 3 (corner piece)
  [G]

Example 3: Water with water on all sides
  [W]        N: +1, E: +2
[W] W [W]    S: +4, W: +8          →  bitmask = 15 (center piece)
  [W]
```

### All 16 Combinations

```
Bitmask | Neighbors      | Visual Description
--------|----------------|-------------------
   0    | none           | Isolated tile
   1    | N              | South edge
   2    | E              | West edge
   3    | N+E            | Southwest corner (inner)
   4    | S              | North edge
   5    | N+S            | Vertical corridor
   6    | E+S            | Northwest corner (inner)
   7    | N+E+S          | West edge only exposed
   8    | W              | East edge
   9    | N+W            | Southeast corner (inner)
  10    | E+W            | Horizontal corridor
  11    | N+E+W          | South edge only exposed
  12    | S+W            | Northeast corner (inner)
  13    | N+S+W          | East edge only exposed
  14    | E+S+W          | North edge only exposed
  15    | all            | Center (no edges)
```

---

## 8-Neighbor (With Corners) Autotiling

For higher quality results, also check diagonal neighbors. This creates smoother corner transitions.

### Bit Assignments

```
[32] NW  [1] N  [64] NE
          ↘   ↓   ↙
  [8] W     ◼     E [2]
          ↗   ↑   ↖
[128] SW  [4] S  [16] SE

Cardinal bits: N=1, E=2, S=4, W=8
Diagonal bits: SE=16, NW=32, NE=64, SW=128
```

### The Corner Rule

Corners only matter if **both adjacent cardinal directions match**. Otherwise you get visual artifacts.

```
Corner relevance:
- NE corner counts only if N AND E both match
- NW corner counts only if N AND W both match
- SE corner counts only if S AND E both match
- SW corner counts only if S AND W both match
```

### Implementation with Corner Rule

```cpp
int calculateBitmask8(int x, int y, int tileType) {
    // First check cardinals
    bool n = getTile(x, y - 1) == tileType;
    bool e = getTile(x + 1, y) == tileType;
    bool s = getTile(x, y + 1) == tileType;
    bool w = getTile(x - 1, y) == tileType;

    int bitmask = 0;
    if (n) bitmask += 1;
    if (e) bitmask += 2;
    if (s) bitmask += 4;
    if (w) bitmask += 8;

    // Corners only count if both adjacent cardinals match
    if (n && e && getTile(x + 1, y - 1) == tileType) bitmask += 16;  // NE
    if (n && w && getTile(x - 1, y - 1) == tileType) bitmask += 32;  // NW
    if (s && e && getTile(x + 1, y + 1) == tileType) bitmask += 64;  // SE
    if (s && w && getTile(x - 1, y + 1) == tileType) bitmask += 128; // SW

    return bitmask;  // Returns 0-255, but only 47 are visually distinct
}
```

### Why 47 Tiles, Not 256?

The corner rule means many bitmask values produce identical visuals. For example:
- Bitmask 16 (only SE corner) is impossible — SE requires S+E cardinals
- Many corner combinations collapse to the same appearance

The 47-tile set (sometimes called "blob tileset") covers all visually distinct combinations.

### Lookup Table Approach

Instead of 256 tiles with gaps, use a lookup table:

```cpp
// Maps 8-bit bitmask to actual tile index (0-46)
const int BITMASK_TO_TILE[256] = {
    0,  1,  2,  3,  4,  5,  6,  7,   // 0-7
    8,  9, 10, 11, 12, 13, 14, 15,   // 8-15
    // ... full 256-entry table mapping to 47 unique tiles
};

int getTileVariant(int bitmask) {
    return BITMASK_TO_TILE[bitmask];
}
```

---

## When to Calculate

### Option A: Calculate at Render Time

Compute bitmask every frame in the render loop.

```cpp
void Tilemap::render(Shader& shader, const Camera& camera) {
    // ... setup code ...

    for (int i = 0; i < width * height; i++) {
        int tileType = tiles[i];
        if (tileType < 0) continue;

        int x = i % width;
        int y = i / width;

        // Calculate variant each frame
        int bitmask = calculateBitmask4(x, y, tileType);
        int tilesetIndex = getTilesetIndex(tileType, bitmask);

        // ... render with tilesetIndex ...
    }
}
```

**Pros**: Simple, always correct
**Cons**: Recalculates every frame (wasteful for static maps)

### Option B: Bake on Load/Change (Preferred)

Store pre-computed variant indices. Recalculate only when tiles change.

```cpp
class Tilemap {
private:
    std::vector<int> tiles;        // Logical type (GRASS, WATER, etc.)
    std::vector<int> displayTiles; // Pre-computed tileset indices

public:
    void setTile(int x, int y, int tileType) {
        tiles[y * width + x] = tileType;

        // Recalculate this tile and its neighbors
        recalculateAutotile(x, y);
        recalculateAutotile(x - 1, y);
        recalculateAutotile(x + 1, y);
        recalculateAutotile(x, y - 1);
        recalculateAutotile(x, y + 1);
    }

    void recalculateAutotile(int x, int y) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;

        int tileType = tiles[y * width + x];
        int bitmask = calculateBitmask4(x, y, tileType);
        displayTiles[y * width + x] = getTilesetIndex(tileType, bitmask);
    }

    void bakeAllAutotiles() {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                recalculateAutotile(x, y);
            }
        }
    }
};
```

**Pros**: Fast rendering, only recalculates when needed
**Cons**: More complex, must remember to update on changes

---

## Tileset Organization

### 4-Neighbor Layout (16 tiles per terrain)

Arrange tiles so bitmask directly indexes:

```
Tileset row for WATER (16 tiles):
┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
│  0 │  1 │  2 │  3 │  4 │  5 │  6 │  7 │  8 │  9 │ 10 │ 11 │ 12 │ 13 │ 14 │ 15 │
└────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘
  ↑                                                                            ↑
isolated                                                                   center
```

### Index Calculation

```cpp
int getTilesetIndex(int tileType, int bitmask) {
    // Each terrain type has 16 consecutive tiles
    int baseIndex = tileType * 16;
    return baseIndex + bitmask;
}

// Example:
// GRASS = 0, bitmask 5 → tile 5
// WATER = 1, bitmask 5 → tile 21
// DIRT  = 2, bitmask 5 → tile 37
```

### 8-Neighbor Layout (47 tiles per terrain)

Use the lookup table approach since bitmasks don't map directly:

```cpp
int getTilesetIndex(int tileType, int bitmask) {
    int baseIndex = tileType * 47;
    return baseIndex + BITMASK_TO_TILE[bitmask];
}
```

---

## Summary

| Concept | Description |
|---------|-------------|
| Autotiling | Automatic tile variant selection based on neighbors |
| Bitmask | Sum of bit values for matching neighbors |
| 4-neighbor | Cardinal only, 16 variants per terrain |
| 8-neighbor | Cardinals + diagonals, 47 variants per terrain |
| Corner rule | Diagonals only count if adjacent cardinals match |
| Baking | Pre-compute variants on load/change for performance |

### Key Formulas

```cpp
// 1D index to 2D coordinates
int x = index % width;
int y = index / width;

// Bitmask to tileset index
int tilesetIndex = (tileType * tilesPerType) + bitmask;

// Or with lookup table
int tilesetIndex = (tileType * tilesPerType) + BITMASK_TO_TILE[bitmask];
```
