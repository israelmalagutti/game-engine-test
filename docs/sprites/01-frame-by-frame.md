# Frame-by-Frame Sprite Animation -- Complete Reference

This document covers everything you need to understand and implement frame-by-frame sprite animation in a 2D game engine. It starts from first principles and builds up to production-level architecture. If you already have a working tilemap system that indexes into texture atlases using UV math (which this engine does), you are closer to sprite animation than you think -- the core rendering technique is identical. Animation just adds the dimension of **time**.

## Table of Contents

- [1. Sprite Sheets -- Complete Theory](#1-sprite-sheets----complete-theory)
  - [What Is a Sprite Sheet?](#what-is-a-sprite-sheet)
  - [Grid-Based vs. Packed Sprite Sheets](#grid-based-vs-packed-sprite-sheets)
  - [Standard Layout Conventions](#standard-layout-conventions)
  - [Frame Dimensions and Padding](#frame-dimensions-and-padding)
  - [Power-of-Two Texture Sizes](#power-of-two-texture-sizes)
  - [Sprite Sheet Organization Strategies](#sprite-sheet-organization-strategies)
  - [How Professional Pixel Artists Organize Their Sheets](#how-professional-pixel-artists-organize-their-sheets)
  - [Memory Implications](#memory-implications)
- [2. UV Coordinate Calculation -- The Math In Detail](#2-uv-coordinate-calculation----the-math-in-detail)
  - [From Frame Index to Row and Column](#from-frame-index-to-row-and-column)
  - [Computing Normalized UV Coordinates](#computing-normalized-uv-coordinates)
  - [Handling Non-Square Frames](#handling-non-square-frames)
  - [Texture Bleeding -- Complete Analysis](#texture-bleeding----complete-analysis)
  - [Complete Mathematical Derivation](#complete-mathematical-derivation)
  - [Comparison with CSS background-position](#comparison-with-css-background-position)
- [3. Animation Timing -- Deep Dive](#3-animation-timing----deep-dive)
  - [Why Delta Time Matters](#why-delta-time-matters)
  - [The Accumulator Pattern](#the-accumulator-pattern)
  - [Fixed vs. Variable Frame Durations](#fixed-vs-variable-frame-durations)
  - [Animation Speed Curves](#animation-speed-curves)
  - [Slow Motion and Speed-Up Effects](#slow-motion-and-speed-up-effects)
  - [Frame Skipping and Lag Spike Handling](#frame-skipping-and-lag-spike-handling)
  - [Web Development Comparisons](#web-development-comparisons)
- [4. Animation State Machines -- Complete Design](#4-animation-state-machines----complete-design)
  - [What Is a Finite State Machine?](#what-is-a-finite-state-machine)
  - [States for a Top-Down RPG Character](#states-for-a-top-down-rpg-character)
  - [Transitions, Conditions, and Priorities](#transitions-conditions-and-priorities)
  - [One-Shot vs. Looping Animations](#one-shot-vs-looping-animations)
  - [Animation Events and Callbacks](#animation-events-and-callbacks)
  - [Queueing Animations](#queueing-animations)
  - [Blending Considerations](#blending-considerations)
  - [Hierarchical State Machines](#hierarchical-state-machines)
- [5. Direction Handling -- Top-Down Specifics](#5-direction-handling----top-down-specifics)
  - [4-Directional vs. 8-Directional Animation](#4-directional-vs-8-directional-animation)
  - [Last-Facing Direction for Idle](#last-facing-direction-for-idle)
  - [Horizontal Sprite Flipping](#horizontal-sprite-flipping)
  - [Diagonal Movement Priority](#diagonal-movement-priority)
  - [Sprite Sheet Layouts for Directional Animation](#sprite-sheet-layouts-for-directional-animation)
- [6. Implementation Architecture](#6-implementation-architecture)
  - [The Animation Class](#the-animation-class)
  - [The Animator / AnimationController Component](#the-animator--animationcontroller-component)
  - [Separation of Concerns](#separation-of-concerns)
  - [Integration with Entity and Sprite Systems](#integration-with-entity-and-sprite-systems)
  - [Complete Pseudocode for All Classes](#complete-pseudocode-for-all-classes)
- [7. Optimization Techniques](#7-optimization-techniques)
  - [Texture Atlasing Multiple Characters](#texture-atlasing-multiple-characters)
  - [Instanced Rendering](#instanced-rendering)
  - [Animation LOD](#animation-lod)
  - [Pre-Computed UV Tables](#pre-computed-uv-tables)
  - [Off-Screen Culling](#off-screen-culling)
- [8. Real-World Examples](#8-real-world-examples)
  - [Celeste -- Tight, Responsive, State-Driven](#celeste----tight-responsive-state-driven)
  - [Shovel Knight -- Classic Sprite Sheet Organization](#shovel-knight----classic-sprite-sheet-organization)
  - [Undertale -- Minimal Frames, Maximum Expression](#undertale----minimal-frames-maximum-expression)
  - [Dead Cells -- 3D-to-2D Pipeline](#dead-cells----3d-to-2d-pipeline)

---

## 1. Sprite Sheets -- Complete Theory

### What Is a Sprite Sheet?

A sprite sheet is a single image file that contains multiple frames of animation arranged in a grid (or sometimes packed irregularly). It is a texture atlas -- the same concept you already use for your tilemap tileset -- but repurposed for animation rather than terrain.

In your tilemap system, each cell in the tileset represents a different **type** of tile (grass, dirt, water). In a sprite sheet, each cell represents a different **moment in time** of the same character or object.

```
TILESET (what you have):                    SPRITE SHEET (what you need):
Each cell = different tile TYPE              Each cell = different animation FRAME

┌──────┬──────┬──────┬──────┐               ┌──────┬──────┬──────┬──────┐
│ grass│ dirt │ water│ stone│               │frame0│frame1│frame2│frame3│
├──────┼──────┼──────┼──────┤               ├──────┼──────┼──────┼──────┤
│ path │ tree │ rock │ bush │               │frame4│frame5│frame6│frame7│
└──────┴──────┴──────┴──────┘               └──────┴──────┴──────┴──────┘

Same atlas math.                             Same atlas math.
tileID is static (loaded from map data).     frameIndex changes every few milliseconds.
```

The fundamental reason sprite sheets exist is the same reason tilesets exist: **minimizing texture binds on the GPU.** Recall from your texture atlas documentation that calling `glBindTexture()` is expensive. If every frame of every animation was a separate texture file, you would need to rebind textures constantly. A sprite sheet keeps everything in one texture, and you simply change which UV sub-region you sample from -- exactly as your tilemap already does with `uvOffset` and `uvSize` uniforms in your tile vertex shader.

### Grid-Based vs. Packed Sprite Sheets

There are two fundamental ways to arrange frames within a sprite sheet.

**Grid-based (uniform grid):**

Every frame occupies a cell of identical size. The sheet is divided into rows and columns of equal dimensions. This is the simpler of the two approaches, and the one used by virtually all pixel art games.

```
Grid-based sprite sheet (4 columns x 3 rows, each cell 32x48 pixels):

     col 0    col 1    col 2    col 3
    ┌────────┬────────┬────────┬────────┐
    │        │        │        │        │
r0  │ idle 0 │ idle 1 │ idle 2 │ idle 3 │  32px wide
    │        │        │        │        │  48px tall
    ├────────┼────────┼────────┼────────┤
    │        │        │        │        │
r1  │ walk 0 │ walk 1 │ walk 2 │ walk 3 │
    │        │        │        │        │
    ├────────┼────────┼────────┼────────┤
    │        │        │        │        │
r2  │ atk  0 │ atk  1 │ atk  2 │ atk  3 │
    │        │        │        │        │
    └────────┴────────┴────────┴────────┘

Total texture size: 128 x 144 pixels (4*32 x 3*48)

Advantages:
  - UV math is trivial (identical to tilemap UV math)
  - Frame lookup is O(1): frameCol = frameIndex % cols, frameRow = frameIndex / cols
  - Easy to author, easy to debug, easy to visually inspect

Disadvantages:
  - Wasted space if frames have different sizes (e.g., attack frame is wider)
  - Every frame must fit within the largest frame's bounding box
```

**Packed (non-uniform, texture-packed):**

Frames are placed wherever they fit, minimizing wasted transparent pixels. Each frame can have a different size. A separate metadata file describes where each frame is located (x, y, width, height).

```
Packed sprite sheet:

┌──────────────────────────────┐
│ ┌──────┐ ┌────────────┐     │
│ │idle 0│ │  attack 0  │     │
│ │      │ │            │     │
│ └──────┘ └────────────┘     │
│ ┌──────┐ ┌──────┐ ┌──────┐ │
│ │idle 1│ │walk 0│ │walk 1│ │
│ │      │ │      │ │      │ │
│ └──────┘ └──────┘ └──────┘ │
│ ┌────────────┐ ┌──────┐    │
│ │  attack 1  │ │walk 2│    │
│ │            │ │      │    │
│ └────────────┘ └──────┘    │
└──────────────────────────────┘

Metadata file (JSON, XML, or binary):
{
  "frames": {
    "idle_0": { "x": 2,  "y": 2,  "w": 32, "h": 48 },
    "idle_1": { "x": 2,  "y": 52, "w": 32, "h": 48 },
    "walk_0": { "x": 36, "y": 52, "w": 32, "h": 48 },
    "attack_0": { "x": 36, "y": 2, "w": 64, "h": 48 },
    ...
  }
}

Advantages:
  - Minimal wasted space (important for mobile/web games with strict memory budgets)
  - Frames can have different dimensions (attack is wider than idle)
  - Tools like TexturePacker automate the packing

Disadvantages:
  - Requires a metadata file (more file I/O, more parsing, more things to go wrong)
  - UV calculation requires a lookup table instead of simple math
  - Cannot do UV math inline -- must load and parse the metadata first
  - Harder to hand-edit or debug visually
```

**For your project:** Use grid-based. Pixel art games almost universally do. The wasted space is negligible at pixel art resolutions (a 256x256 sprite sheet is 256 KB in VRAM -- nothing), and the UV math is identical to what your tilemap already does. Packed sheets are primarily useful for high-resolution mobile games or web games where download size matters, and tools like TexturePacker or Shoebox generate them automatically.

### Standard Layout Conventions

The game development community has settled on several conventions for how sprite sheets are organized. Understanding these conventions is important because you will encounter them in asset packs, tutorials, and tools.

**Convention 1: Rows = animations, Columns = frames (most common)**

```
Each row is one complete animation. Each column is a frame within that animation.

         Frame 0   Frame 1   Frame 2   Frame 3
        ┌─────────┬─────────┬─────────┬─────────┐
Row 0:  │ idle  0 │ idle  1 │ idle  2 │ idle  3 │  <- Idle animation (4 frames)
        ├─────────┼─────────┼─────────┼─────────┤
Row 1:  │ walk  0 │ walk  1 │ walk  2 │ walk  3 │  <- Walk animation (4 frames)
        ├─────────┼─────────┼─────────┼─────────┤
Row 2:  │ atk   0 │ atk   1 │ atk   2 │ atk   3 │  <- Attack animation (4 frames)
        ├─────────┼─────────┼─────────┼─────────┤
Row 3:  │ die   0 │ die   1 │ die   2 │ die   3 │  <- Death animation (4 frames)
        └─────────┴─────────┴─────────┴─────────┘

To play "walk": iterate through columns 0-3 of row 1.
```

**Convention 2: Rows = directions, Columns = frames (RPG Maker style)**

This is the most common layout for top-down RPGs. Each row represents the character facing a different direction. The column is the frame index within that direction's walk cycle.

```
RPG Maker VX/MV character format:

         Frame 0   Frame 1   Frame 2
        ┌─────────┬─────────┬─────────┐
Row 0:  │ down  0 │ down  1 │ down  2 │  <- Facing down
        ├─────────┼─────────┼─────────┤
Row 1:  │ left  0 │ left  1 │ left  2 │  <- Facing left
        ├─────────┼─────────┼─────────┤
Row 2:  │ right 0 │ right 1 │ right 2 │  <- Facing right
        ├─────────┼─────────┼─────────┤
Row 3:  │ up    0 │ up    1 │ up    2 │  <- Facing up
        └─────────┴─────────┴─────────┘

Direction order: Down, Left, Right, Up (RPG Maker standard)
```

**Convention 3: Blocks of directions per animation (Stardew Valley style)**

Stardew Valley groups its character animations so that each animation has all four directions contiguous, then the next animation starts.

```
Stardew Valley-style layout (simplified):

         Frame 0   Frame 1   Frame 2   Frame 3
        ┌─────────┬─────────┬─────────┬─────────┐
Row 0:  │ walk D0 │ walk D1 │ walk D2 │ walk D3 │  <- Walk Down
        ├─────────┼─────────┼─────────┼─────────┤
Row 1:  │ walk R0 │ walk R1 │ walk R2 │ walk R3 │  <- Walk Right
        ├─────────┼─────────┼─────────┼─────────┤
Row 2:  │ walk U0 │ walk U1 │ walk U2 │ walk U3 │  <- Walk Up
        ├─────────┼─────────┼─────────┼─────────┤
Row 3:  │ idle D0 │ idle D1 │ idle R0 │ idle R1 │  <- Idle (compact)
        ├─────────┼─────────┼─────────┼─────────┤
Row 4:  │ atk  D0 │ atk  D1 │ atk  D2 │ atk  D3 │  <- Attack Down
        ├─────────┼─────────┼─────────┼─────────┤
Row 5:  │ atk  R0 │ atk  R1 │ atk  R2 │ atk  R3 │  <- Attack Right
        └─────────┴─────────┴─────────┴─────────┘

Note: "Walk Left" is produced by flipping "Walk Right" horizontally.
```

The exact convention you choose does not matter technically -- they all produce the same result. What matters is **consistency**. Pick one convention and use it for every character in the game so your animation loading code does not need special cases.

### Frame Dimensions and Padding

**Frame dimensions** are the width and height of a single animation frame in pixels. Common sizes in pixel art:

```
Character sizes by game style:

  16x16  -- Very small, NES-era (early Zelda, early Final Fantasy)
  16x24  -- Small characters, taller than wide (many GBA games)
  24x32  -- Medium characters (early Pokemon-style)
  32x32  -- Standard RPG Maker size (RPG Maker VX/MV)
  32x48  -- Taller characters (common in Stardew Valley-like games)
  48x48  -- Larger characters (more animation detail possible)
  64x64  -- Large sprites (detailed characters, bosses)
  16x16 tiles with 32x48 characters -- The character is taller than the tile grid
                                        (very common in top-down RPGs)

Note: Characters do NOT need to be the same size as tiles.
Stardew Valley uses 16x16 tiles but characters are approximately 16x32.
The character occupies one tile horizontally but overlaps two tiles vertically.
```

**Padding** is extra transparent space between frames in the sprite sheet. Its purpose is to prevent **texture bleeding** -- when the GPU accidentally samples pixels from an adjacent frame.

```
Without padding:                         With 1px padding:

┌────┬────┬────┬────┐                   ┌────┐ ┌────┐ ┌────┐ ┌────┐
│ f0 │ f1 │ f2 │ f3 │                   │ f0 │ │ f1 │ │ f2 │ │ f3 │
├────┼────┼────┼────┤                   └────┘ └────┘ └────┘ └────┘
│ f4 │ f5 │ f6 │ f7 │
└────┴────┴────┴────┘                   ┌────┐ ┌────┐ ┌────┐ ┌────┐
                                        │ f4 │ │ f5 │ │ f6 │ │ f7 │
                                        └────┘ └────┘ └────┘ └────┘

The "padding" is transparent pixels (alpha = 0) between frames.
If the GPU samples slightly outside a frame's UV bounds (due to
floating-point imprecision), it gets a transparent pixel instead
of a pixel from the neighboring frame.
```

Why 1px of padding is specifically useful: The most common source of bleeding is the GPU sampling exactly at the boundary between two frames. With bilinear filtering (`GL_LINEAR`), the GPU blends the 4 nearest texels. If the sample point is at the exact edge of a frame, one of those 4 texels belongs to the neighboring frame. A 1px transparent border ensures that even with bilinear sampling, the blended result includes transparent pixels rather than the neighbor's content.

**For pixel art with `GL_NEAREST` filtering: padding is unnecessary.** `GL_NEAREST` does not blend -- it picks the single nearest texel. There is no blending across boundaries. Your engine already uses `GL_NEAREST` for pixel art tilesets, so your sprite sheets do not need padding. This is the main reason pixel art games almost always use tightly packed grids.

### Power-of-Two Texture Sizes

GPUs are optimized for textures whose dimensions are powers of two: 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 pixels.

**Why this matters historically:**

Older GPU hardware (pre-2005) **required** power-of-two textures. A 100x100 texture would either fail to load or be silently padded to 128x128, wasting VRAM and causing UV coordinate errors. This is no longer a hard requirement on modern hardware (OpenGL 2.0+ supports Non-Power-Of-Two, or NPOT, textures), but power-of-two textures are still faster for several reasons:

```
GPU memory alignment:

Power-of-two:                          Non-power-of-two:
┌───────────────────────────┐          ┌──────────────────────┐
│ 256 x 256 = 65,536 texels│          │ 200 x 200 = 40,000  │
│ Divides evenly into       │          │ Does NOT divide into │
│ cache lines               │          │ cache lines evenly   │
│ Mipmaps divide cleanly:  │          │ Mipmaps are awkward: │
│   256 -> 128 -> 64 -> 32 │          │   200 -> 100 -> 50  │
│   -> 16 -> 8 -> 4 -> 2   │          │   -> 25 -> 12 -> 6  │
│   -> 1 (exact halving)   │          │   -> 3 -> 1 (messy)  │
└───────────────────────────┘          └──────────────────────┘
```

1. **Mipmapping:** Mipmaps are pre-calculated half-resolution versions of a texture used when the texture is viewed from far away. Power-of-two textures halve cleanly at every mipmap level. Non-power-of-two textures require rounding, which can introduce artifacts. (For pixel art with `GL_NEAREST` and no mipmaps, this is irrelevant, but it is good to know.)

2. **Memory alignment:** GPU memory is organized in blocks (typically 64 or 128 bytes). Power-of-two texture rows align perfectly with these blocks. Non-power-of-two rows may cross block boundaries, causing extra memory fetches.

3. **Tiling/wrapping:** `GL_REPEAT` texture wrapping uses modular arithmetic, which is a trivial bitwise AND for power-of-two sizes (`x & (size - 1)`) but requires an actual division for non-power-of-two sizes.

**Practical advice for sprite sheets:**

```
Choose your frame size first, then calculate the needed texture size:

Example: 32x48 frames, 6 frames per animation, 8 animations

  Width needed:  6 frames * 32px = 192px  -->  round up to 256px
  Height needed: 8 rows   * 48px = 384px  -->  round up to 512px

  Sprite sheet size: 256 x 512

  Unused space: (256*512) - (192*384) = 131,072 - 73,728 = 57,344 texels wasted
  That's 43% wasted, but 256x512 at RGBA is only 512 KB. Negligible.

Common sprite sheet sizes:
  128 x 128   --   64 KB    (small characters, few animations)
  256 x 256   --  256 KB    (typical for one character)
  512 x 512   --    1 MB    (detailed character with many animations)
  1024 x 1024 --    4 MB    (large character sheet or shared atlas)
  2048 x 2048 --   16 MB    (maximum reasonable for a single sprite atlas)
```

### Sprite Sheet Organization Strategies

For a game with many characters, you need a strategy for how to organize sprite sheets across files.

**Strategy 1: One sheet per character (simplest, recommended for your project)**

```
assets/sprites/
  player.png          -- 512x512, all player animations
  villager_alex.png   -- 256x256, all animations for NPC "Alex"
  villager_sam.png    -- 256x256, all animations for NPC "Sam"
  slime_green.png     -- 128x128, all animations for green slime enemy
  slime_blue.png      -- 128x128, all animations for blue slime enemy

Advantages:
  - Each character is self-contained
  - Easy to add/remove characters
  - Simple to understand and debug

Disadvantages:
  - If multiple characters are on screen, each needs its own texture bind
  - 10 characters on screen = 10 texture binds per frame
```

**Strategy 2: Shared atlas (multiple characters in one texture)**

```
assets/sprites/
  characters_npcs.png     -- 2048x2048, contains ALL NPC sprites
  characters_enemies.png  -- 1024x1024, contains ALL enemy sprites
  player.png              -- 512x512, player is separate (most frames)

Each character occupies a rectangular region of the atlas:
┌─────────────────────────────────────┐
│ Alex (0,0 to 255,255)              │
│ ┌───────────────┐ ┌───────────────┐│
│ │               │ │               ││
│ │     Alex      │ │     Sam       ││
│ │               │ │               ││
│ └───────────────┘ └───────────────┘│
│ ┌───────────────┐ ┌───────────────┐│
│ │               │ │               ││
│ │    Haley      │ │   Sebastian   ││
│ │               │ │               ││
│ └───────────────┘ └───────────────┘│
└─────────────────────────────────────┘

Advantages:
  - One texture bind for ALL NPCs (or all enemies)
  - Enables batch rendering (draw all NPCs in one draw call)

Disadvantages:
  - Harder to author (artists must coordinate)
  - Adding a character might require rearranging the atlas
  - Large textures may exceed GPU limits on older hardware
  - Metadata file required to track each character's region
```

**Strategy 3: Dynamic atlas (built at runtime)**

The engine loads individual character textures and packs them into a shared atlas at startup. This is the approach used by many professional engines (Unity, Godot).

```
At load time:
  1. Load player.png (256x256)
  2. Load slime.png (128x128)
  3. Load villager.png (256x256)
  4. Allocate a 512x512 atlas texture
  5. Copy all three into the atlas, tracking their positions
  6. All rendering uses the single atlas

This is advanced and not necessary for your project.
```

For an HD-2D Stardew Valley clone, **Strategy 1 (one sheet per character)** is the right choice. Stardew Valley itself uses individual sprite sheets per character. The number of simultaneous characters on screen is small (typically under 20), so the texture bind cost is irrelevant.

### How Professional Pixel Artists Organize Their Sheets

Understanding the artist's workflow helps you design an engine that is easy for artists to work with.

**Aseprite** is the industry-standard tool for pixel art animation. Here is how a professional workflow looks:

```
Aseprite workflow:

1. The artist creates a single .aseprite file per character
2. The file has a TIMELINE with frames arranged horizontally:

   Frame:  1    2    3    4    5    6    7    8    9   10   11   12
          ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
   Canvas │ i0 │ i1 │ i2 │ i3 │ w0 │ w1 │ w2 │ w3 │ a0 │ a1 │ a2 │ a3 │
          └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘

3. The artist uses TAGS to group frames into named animations:
   Tag "idle":   frames 1-4    (duration: 200ms each)
   Tag "walk":   frames 5-8    (duration: 150ms each)
   Tag "attack": frames 9-12   (duration: 80ms, 50ms, 50ms, 120ms)

4. Export as sprite sheet:
   Aseprite can export to a .png sprite sheet + .json metadata file.

   The .json file contains:
   {
     "frames": {
       "character_0.png": { "frame": {"x":0, "y":0, "w":32, "h":48} },
       "character_1.png": { "frame": {"x":32, "y":0, "w":32, "h":48} },
       ...
     },
     "meta": {
       "frameTags": [
         { "name": "idle",   "from": 0, "to": 3, "direction": "forward" },
         { "name": "walk",   "from": 4, "to": 7, "direction": "forward" },
         { "name": "attack", "from": 8, "to": 11, "direction": "forward" }
       ]
     }
   }
```

The key insight: Aseprite's tag system directly maps to the animation state machine concept covered later in this document. Each tag becomes an animation clip. The metadata file tells the engine which frames belong to which animation and how long each frame lasts.

**For a grid-based approach without metadata files**, the convention is simpler: the artist exports a sprite sheet with a known, fixed layout (rows = animations, columns = frames), and the engine hard-codes the row-to-animation mapping. This is less flexible but has zero file parsing overhead.

### Memory Implications

Understanding how much VRAM your sprite sheets consume helps you make informed decisions about texture sizes and atlas strategies.

**The formula:**

```
VRAM usage = width * height * bytes_per_pixel

For RGBA (4 channels, 8 bits each):
  bytes_per_pixel = 4

Examples:
  128 x 128  * 4 =    65,536 bytes =   64 KB
  256 x 256  * 4 =   262,144 bytes =  256 KB
  512 x 512  * 4 = 1,048,576 bytes =    1 MB
  1024 x 1024 * 4 = 4,194,304 bytes =    4 MB
  2048 x 2048 * 4 = 16,777,216 bytes =   16 MB
```

**Calculating total VRAM for a game's sprites:**

```
Stardew Valley-scale game:
  Player character:     1 sheet,  512x512  =   1 MB
  30 NPCs:             30 sheets, 256x256  =  7.5 MB
  20 enemy types:      20 sheets, 256x256  =   5 MB
  10 farm animal types: 10 sheets, 128x128 = 0.6 MB
  Misc (items, effects): 5 sheets, 256x256 = 1.25 MB
                                           ----------
  Total sprite VRAM:                        ~15.4 MB

  For comparison:
  - A modern GPU has 4-16 GB of VRAM
  - A single 4K screenshot is ~33 MB
  - Your sprite sheets are a rounding error

  Conclusion: Do not worry about sprite sheet memory for a pixel art game.
  Even doubling or tripling every sheet would be fine.
```

This is different from web development where image size directly impacts download time and bandwidth costs. In a native game, all assets are loaded from disk into GPU memory once. The bottleneck is never the size of pixel art sprite sheets.

**What DOES matter for memory:** texture bind overhead and GPU cache pressure. Having 100 separate textures means 100 bind operations if they are all visible. This is why shared atlases exist -- not to save memory, but to reduce binds. For a pixel art game with under 20 simultaneously visible characters, this optimization is unnecessary.

---

## 2. UV Coordinate Calculation -- The Math In Detail

### From Frame Index to Row and Column

You already know this math from your tilemap. Here it is again, framed for sprite animation.

Given a linear frame index (0, 1, 2, 3, ...) and a grid with a known number of columns, convert to a (column, row) pair:

```
framesPerRow = total columns in the sprite sheet grid

column = frameIndex % framesPerRow
row    = frameIndex / framesPerRow    (integer division -- floor)

Example: sprite sheet is 6 columns wide, frameIndex = 14

  column = 14 % 6 = 2
  row    = 14 / 6 = 2    (integer division: 14/6 = 2.333... -> 2)

  So frame 14 is at column 2, row 2 (zero-indexed).

  Visual verification:
           col0  col1  col2  col3  col4  col5
  row 0:  [  0 ] [  1 ] [  2 ] [  3 ] [  4 ] [  5 ]
  row 1:  [  6 ] [  7 ] [  8 ] [  9 ] [ 10 ] [ 11 ]
  row 2:  [ 12 ] [ 13 ] [*14*] [ 15 ] [ 16 ] [ 17 ]
                          ^^^
                        column 2, row 2. Correct.
```

However, for animation, you typically do NOT use a single linear frame index across the entire sheet. Instead, you know which **row** the animation lives on, and you iterate through columns within that row. The convention is:

```
// You already know: this animation starts at row 2, and has 4 frames (columns 0-3)
int animationRow = 2;
int currentFrame = 0;  // incremented by the animation timer

// The column IS the current frame
int column = currentFrame;
int row = animationRow;
```

This is simpler than the linear index approach because each animation is confined to a single row.

### Computing Normalized UV Coordinates

UV coordinates in OpenGL are **normalized**: they range from 0.0 (left/top edge of the texture) to 1.0 (right/bottom edge of the texture). To sample a specific frame from a sprite sheet, you need the UV rectangle that encloses that frame.

```
Sprite sheet: 256px wide, 192px tall
Frame size: 32px wide, 48px tall
Grid: 8 columns (256/32), 4 rows (192/48)

Target frame: column 3, row 1

Step 1: Compute the UV width and height of one frame
  uvWidth  = frameWidth  / textureWidth  = 32  / 256 = 0.125
  uvHeight = frameHeight / textureHeight = 48  / 192 = 0.25

Step 2: Compute the UV offset (top-left corner of the frame)
  uvOffsetX = column * uvWidth  = 3 * 0.125 = 0.375
  uvOffsetY = row    * uvHeight = 1 * 0.25  = 0.25

Step 3: The UV rectangle for this frame
  u_left   = uvOffsetX                = 0.375
  u_right  = uvOffsetX + uvWidth      = 0.375 + 0.125 = 0.5
  v_top    = uvOffsetY                = 0.25
  v_bottom = uvOffsetY + uvHeight     = 0.25  + 0.25  = 0.5
```

This is identical to how your tilemap computes `uvOffset` and `uvSize`. Looking at your existing `Tilemap::render()`:

```cpp
// From your Tilemap.cpp -- this is the SAME math:
float uvWidth = static_cast<float>(tileSize) / tileset->getWidth();
float uvHeight = static_cast<float>(tileSize) / tileset->getHeight();
// ...
int tileCol = tileID % tilesPerRow;
int tileRow = tileID / tilesPerRow;
float uvX = tileCol * uvWidth;
float uvY = tileRow * uvHeight;
```

And your tile vertex shader applies it:

```glsl
// From your tile.vert -- this is exactly what animation needs:
TexCoord = uvOffset + aTexCoord * uvSize;
```

For animation, you would use the same shader. The only difference is that `uvOffset` changes every few frames (when the animation advances) instead of being static.

### Handling Non-Square Frames

Characters in top-down RPGs are almost always taller than they are wide. A character might stand on a 16x16 tile grid but be 16x24 pixels (or 32x48, or 24x32). This means the UV width and UV height are different.

```
Non-square frames: 32px wide, 48px tall on a 256x192 texture

  uvWidth  = 32  / 256 = 0.125     (each frame spans 12.5% of the texture width)
  uvHeight = 48  / 192 = 0.25      (each frame spans 25% of the texture height)

  These are DIFFERENT values. This is fine -- the UV math handles it naturally.

  The quad you render should match the frame's aspect ratio:
    Quad width  = 32 pixels (or scaled: 64, 96, etc.)
    Quad height = 48 pixels (or scaled: 96, 144, etc.)

  If you render a 32x48 frame onto a square 32x32 quad, the character
  will appear squished. If you render it onto a 48x48 quad, the character
  will appear stretched horizontally. Always maintain the aspect ratio.

Visual: correct vs. incorrect rendering of a 32x48 frame:

  Correct (32x48 quad):     Squished (32x32 quad):    Stretched (48x48 quad):
  ┌──────────┐              ┌──────────┐              ┌──────────────┐
  │  ○       │              │  ○       │              │    ○         │
  │ /|\\      │              │ /|\\─┘    │              │   /|\\        │
  │ / \\      │              └──────────┘              │   / \\        │
  └──────────┘              (legs are cut off)         └──────────────┘
  (correct proportions)                                (character is wide)
```

In your engine, the `Sprite::setSize()` method controls the quad dimensions. For animated sprites, you would set the size to match the frame dimensions (or a scaled multiple).

### Texture Bleeding -- Complete Analysis

Texture bleeding is when pixels from one frame leak into an adjacent frame during rendering. This is the same problem described in your `10-texture-atlas.md` documentation, but it deserves a thorough treatment here because it can be especially visible in animation (a sliver of the next frame appearing during movement).

**What causes texture bleeding:**

```
Scenario: Two adjacent frames in a sprite sheet

Frame 0 (red character)     Frame 1 (blue character)
┌──────────────────────┐┌──────────────────────┐
│                      ││                      │
│    ████████          ││          ████████    │
│    ██RED ██          ││          ██BLUE██    │
│    ████████          ││          ████████    │
│                      ││                      │
└──────────────────────┘└──────────────────────┘
                        ^^
                        Boundary between frames

When the GPU renders frame 0, it needs UV coordinates that sample ONLY
the left half. But floating-point math is imprecise. If the right edge
of the UV rectangle is calculated as 0.49999999 instead of exactly 0.5,
and the GPU uses bilinear filtering (GL_LINEAR), it samples texels at
and around that boundary -- potentially grabbing blue pixels from frame 1.

Result: a thin blue line appears on the right edge of the red character.
```

**Solution 1: GL_NEAREST filtering (eliminates the problem entirely for pixel art)**

```cpp
// When creating the texture:
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

// GL_NEAREST picks the SINGLE closest texel. No blending occurs.
// Even if the UV is slightly off, it picks one texel or the other --
// never a blend of both. For pixel art, this is always correct.
```

This is the primary solution for pixel art games. Your engine should already be using `GL_NEAREST` for sprite sheets. If it is, texture bleeding is a non-issue and you can skip the other solutions. The remaining solutions are included for completeness and for non-pixel-art scenarios.

**Solution 2: Half-texel inset**

```
Inset the UV rectangle by half a texel on each edge.
This ensures the sample point is always at the CENTER of a texel,
never at the boundary.

Normal UV:                          Half-texel inset UV:
u_left  = col * uvW                u_left  = col * uvW + halfTexelX
u_right = (col+1) * uvW            u_right = (col+1) * uvW - halfTexelX
v_top   = row * uvH                v_top   = row * uvH + halfTexelY
v_bot   = (row+1) * uvH            v_bot   = (row+1) * uvH - halfTexelY

where:
  halfTexelX = 0.5 / textureWidth
  halfTexelY = 0.5 / textureHeight

For a 256x192 texture:
  halfTexelX = 0.5 / 256 = 0.001953125
  halfTexelY = 0.5 / 192 = 0.002604167

This is a tiny inset -- less than one pixel -- but it prevents
the sampler from ever reaching the boundary between frames.
```

**Solution 3: Padding between frames**

```
Add 1-2 transparent pixels between every frame in the sprite sheet.
The artist or an export tool does this during sprite sheet creation.

Without padding (32px frames):     With 1px padding (32px frames + 1px gaps):
┌────┬────┬────┬────┐             ┌────┐ ┌────┐ ┌────┐ ┌────┐
│ f0 │ f1 │ f2 │ f3 │             │ f0 │ │ f1 │ │ f2 │ │ f3 │
└────┴────┴────┴────┘             └────┘ └────┘ └────┘ └────┘
Sheet width: 128                   Sheet width: 131 (128 + 3 gaps)

The UV calculation must account for padding:
  stride = frameWidth + padding    // 32 + 1 = 33
  uvOffsetX = column * stride / textureWidth
  uvWidth   = frameWidth / textureWidth  // still just the frame, not the gap
```

**Solution 4: Edge extrusion**

```
Duplicate the edge pixels of each frame outward by 1px.
The padding pixels are NOT transparent -- they are copies of the
frame's edge pixels.

Original frame:          Extruded frame (1px border):
┌────────┐              ┌──────────┐
│ABCDEFGH│              │AABCDEFGHH│  <- top edge extruded
│IJKLMNOP│              │AABCDEFGHH│  <- duplicate of original top row
│QRSTUVWX│              │IIJKLMNOPP│
└────────┘              │QQRSTUVWXX│
                        │QQRSTUVWXX│  <- bottom edge extruded
                        └──────────┘

If bilinear filtering samples beyond the frame edge,
it blends with a duplicate of the edge pixel instead
of a pixel from the neighboring frame. The result is
visually correct (the edge color is preserved).
```

**Solution 5: Texture arrays (GL_TEXTURE_2D_ARRAY)**

```
Instead of a single 2D texture with frames packed side-by-side,
use a texture array where each "layer" is one complete frame.

OpenGL texture array:
  Layer 0: [frame 0 - full resolution, no neighbors]
  Layer 1: [frame 1 - full resolution, no neighbors]
  Layer 2: [frame 2 - full resolution, no neighbors]
  ...

Each frame occupies the ENTIRE 0.0-1.0 UV range of its layer.
There ARE no neighboring frames to bleed into.

In the shader:
  texture(sampler2DArray, vec3(u, v, layerIndex))

Advantages:
  - Zero bleeding (each frame is isolated)
  - UVs are always 0.0 to 1.0 (simplest possible)

Disadvantages:
  - All layers must be the same dimensions
  - Uses more VRAM (no frame-to-frame sharing of transparent space)
  - More complex OpenGL setup (GL_TEXTURE_2D_ARRAY instead of GL_TEXTURE_2D)
  - Not commonly used for 2D games -- overkill for this problem
```

**Recommended approach for your project:** Use `GL_NEAREST` filtering. Problem solved. No padding, no insets, no texture arrays. This is what Stardew Valley, Undertale, Celeste, and virtually every pixel art game does.

### Complete Mathematical Derivation

Here is the full derivation from sprite sheet dimensions to final UV coordinates, step by step, with a concrete example.

```
=== GIVEN ===
Sprite sheet texture:
  textureWidth  = 256 pixels
  textureHeight = 192 pixels

Frame dimensions:
  frameWidth  = 32 pixels
  frameHeight = 48 pixels

Animation to play: "walk_right"
  Lives on row 2 of the sprite sheet
  Has 6 frames (columns 0 through 5)

Current animation frame: 3  (the 4th frame, zero-indexed)


=== STEP 1: Grid dimensions ===

framesPerRow = textureWidth / frameWidth = 256 / 32 = 8
totalRows    = textureHeight / frameHeight = 192 / 48 = 4

Grid is 8 columns x 4 rows.


=== STEP 2: Frame position in the grid ===

column = currentFrame = 3    (we index directly into the row)
row    = animationRow = 2    (the row assigned to "walk_right")


=== STEP 3: Pixel coordinates of the frame's top-left corner ===

pixelX = column * frameWidth  = 3 * 32 = 96
pixelY = row    * frameHeight = 2 * 48 = 96

The frame starts at pixel (96, 96) in the texture.


=== STEP 4: Normalize to UV space (divide by texture dimensions) ===

u_left   = pixelX / textureWidth                  = 96  / 256 = 0.375
u_right  = (pixelX + frameWidth) / textureWidth   = 128 / 256 = 0.5
v_top    = pixelY / textureHeight                  = 96  / 192 = 0.5
v_bottom = (pixelY + frameHeight) / textureHeight  = 144 / 192 = 0.75


=== STEP 5: Alternatively, using uvSize and uvOffset (your engine's approach) ===

uvWidth  = frameWidth  / textureWidth  = 32  / 256 = 0.125
uvHeight = frameHeight / textureHeight = 48  / 192 = 0.25

uvOffsetX = column * uvWidth  = 3 * 0.125 = 0.375
uvOffsetY = row    * uvHeight = 2 * 0.25  = 0.5

These match: u_left = uvOffsetX = 0.375, v_top = uvOffsetY = 0.5


=== STEP 6: In the vertex shader ===

// Base texture coordinates for the unit quad (0-1 range):
//   top-left:     (0, 1)
//   bottom-right: (1, 0)
//   etc.
//
// The shader transforms them:
//   TexCoord = uvOffset + aTexCoord * uvSize
//
// For the top-left vertex (aTexCoord = 0, 1):
//   TexCoord.x = 0.375 + 0 * 0.125 = 0.375
//   TexCoord.y = 0.5   + 1 * 0.25  = 0.75
//
// For the bottom-right vertex (aTexCoord = 1, 0):
//   TexCoord.x = 0.375 + 1 * 0.125 = 0.5
//   TexCoord.y = 0.5   + 0 * 0.25  = 0.5
//
// This maps exactly to the UV rectangle we computed in Step 4.


=== VISUAL VERIFICATION ===

Full texture UV space:
(0,0)────────────────────────────────────────(1,0)
  │  c0    c1    c2    c3    c4    c5    c6    c7  │
  ├──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┤
  │      │      │      │      │      │      │      │      │ r0 (v: 0.0 - 0.25)
  ├──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┤
  │      │      │      │      │      │      │      │      │ r1 (v: 0.25 - 0.5)
  ├──────┼──────┼──────╔══════╗──────┼──────┼──────┼──────┤
  │      │      │      ║FRAME ║      │      │      │      │ r2 (v: 0.5 - 0.75)
  ├──────┼──────┼──────╚══════╝──────┼──────┼──────┼──────┤
  │      │      │      │      │      │      │      │      │ r3 (v: 0.75 - 1.0)
  └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
(0,1)────────────────────────────────────────(1,1)

                       ↑
             Frame at column 3, row 2
             UV: (0.375, 0.5) to (0.5, 0.75)
```

### Comparison with CSS background-position

If you have done CSS sprite sheets, the UV coordinate calculation is the same operation expressed differently.

```
CSS (web development):
  .sprite {
    background-image: url('spritesheet.png');
    width: 32px;
    height: 48px;
    background-position: -96px -96px;   /* Negative pixel offsets */
  }

  The browser shows a 32x48 window into the sprite sheet,
  shifted so that pixel (96, 96) is at the top-left of the element.

  CSS uses NEGATIVE PIXEL offsets (shift the image left/up).

OpenGL (your engine):
  uvOffset = vec2(0.375, 0.5);   /* Normalized (0-1) offset */
  uvSize   = vec2(0.125, 0.25);  /* Normalized (0-1) size */

  The shader computes: TexCoord = uvOffset + aTexCoord * uvSize
  This maps the quad's 0-1 texture coordinates into the correct
  sub-region of the sprite sheet.

  OpenGL uses POSITIVE NORMALIZED offsets (not pixels, not negative).

Conversion between the two:
  CSS pixel offset  ->  OpenGL UV offset:
    uvOffsetX = abs(cssOffsetX) / textureWidth
    uvOffsetY = abs(cssOffsetY) / textureHeight

  Example:
    CSS: background-position: -96px -96px
    OpenGL: uvOffset = (96/256, 96/192) = (0.375, 0.5)
```

The conceptual model is identical: you have a large image containing many sub-images, and you compute which sub-rectangle to display. CSS does it with pixel offsets and element dimensions. OpenGL does it with normalized UV coordinates and quad geometry. The math is the same, just expressed in different coordinate systems.

---

## 3. Animation Timing -- Deep Dive

### Why Delta Time Matters

Your game loop already uses delta time for movement. Animation timing is the same concept: **decouple the rate of visual change from the game's frame rate.**

```
Without delta time (frame-rate DEPENDENT):

  On a 60fps machine:
    update() is called 60 times per second
    "Advance animation every 10 updates" = 6 animation fps

  On a 30fps machine (slow hardware):
    update() is called 30 times per second
    "Advance animation every 10 updates" = 3 animation fps
    The character walks in SLOW MOTION on slow hardware!

  On a 144fps machine (fast hardware):
    update() is called 144 times per second
    "Advance animation every 10 updates" = 14.4 animation fps
    The character walks FASTER on fast hardware!

With delta time (frame-rate INDEPENDENT):

  On ANY machine:
    "Advance animation when 0.15 seconds have accumulated"
    = always ~6.67 animation fps, regardless of game framerate

  60fps:  0.15s / 0.0167s per frame = advance every ~9 frames
  30fps:  0.15s / 0.0333s per frame = advance every ~4.5 frames
  144fps: 0.15s / 0.0069s per frame = advance every ~21.7 frames

  In all cases, the animation plays at the same wall-clock speed.
```

This is directly analogous to `requestAnimationFrame` in web development. The browser calls your animation callback at whatever rate it can (60fps, 120fps, or lower if the tab is backgrounded). You use the timestamp parameter to compute elapsed time and update your animation accordingly, rather than assuming a fixed frame rate.

### The Accumulator Pattern

The accumulator pattern is the standard way to drive frame-by-frame animation. You accumulate elapsed time and advance the frame when enough time has passed.

```
State:
  float elapsedTime = 0.0;       // Time accumulated since last frame change
  int   currentFrame = 0;        // Which frame of the animation is showing
  float frameDuration = 0.15;    // Seconds per frame
  int   totalFrames = 4;         // Number of frames in this animation
  bool  looping = true;          // Does this animation loop?

Each update(deltaTime):
  elapsedTime += deltaTime;

  if (elapsedTime >= frameDuration) {
      elapsedTime -= frameDuration;    // SUBTRACT, do not reset to zero
      currentFrame++;

      if (currentFrame >= totalFrames) {
          if (looping) {
              currentFrame = 0;
          } else {
              currentFrame = totalFrames - 1;  // Stay on last frame
          }
      }
  }
```

**Why subtract instead of reset?**

This is a crucial detail. Consider what happens when `frameDuration` is 0.15s and a frame takes 0.017s to render (60fps):

```
Subtract approach (correct):
  Frame 1: elapsed = 0.000
  Frame 2: elapsed = 0.017
  Frame 3: elapsed = 0.034
  ...
  Frame 9: elapsed = 0.136
  Frame 10: elapsed = 0.153  --> >= 0.15! Advance animation.
                                  elapsed = 0.153 - 0.15 = 0.003
                                  (0.003s of "credit" carries over)
  Frame 11: elapsed = 0.020
  ...
  Frame 19: elapsed = 0.139
  Frame 20: elapsed = 0.156  --> >= 0.15! Advance animation.
                                  elapsed = 0.156 - 0.15 = 0.006
                                  (more credit carries over)

  Over time, the animation plays at EXACTLY the intended rate
  because remainders accumulate and self-correct.

Reset approach (incorrect):
  Frame 10: elapsed = 0.153  --> >= 0.15! Advance animation.
                                  elapsed = 0.0  (LOST 0.003s!)
  Frame 20: elapsed = 0.170  --> >= 0.15! Advance animation.
                                  elapsed = 0.0  (LOST 0.020s!)

  Over 1000 animation advances, you lose:
    ~0.003s * 1000 = ~3 seconds of drift

  The animation plays ~2% slower than intended. This is visible
  when two characters that should be synchronized gradually drift
  out of sync.
```

This is the same principle as a **fixed timestep** game loop. The subtraction approach is analogous to how physics engines accumulate time and consume it in fixed steps, carrying the remainder forward.

### Fixed vs. Variable Frame Durations

**Fixed duration (uniform timing):** Every frame displays for the same amount of time. This is the simplest approach and works well for smooth, cyclic animations like walk cycles.

```
Fixed duration walk cycle:
  Frame:    0       1       2       3       0       1    ...
  Time:  |--150ms--|--150ms--|--150ms--|--150ms--|--150ms--| ...

  All frames display for exactly 150ms.
  The walk cycle is perfectly regular, like a metronome.
```

**Variable duration (per-frame timing):** Each frame has its own duration. This is essential for animations with **anticipation, action, and follow-through** -- the fundamental principles of animation.

```
Variable duration sword swing:

  Frame:  0 (wind-up)  1 (peak)  2 (swing!)  3 (follow)  4 (recover)
  Time:   |---200ms----|--150ms--|---50ms----|---100ms----|---200ms---|

  ████████████████████                                    ████████████████████
  ████████████████████                                    ████████████████████
               ████████████████                     ██████████████████
                        █████ (50ms -- barely visible, feels FAST)

  The fast frame (50ms) creates the illusion of speed.
  The slow frames (200ms) create weight and anticipation.
  This is the principle of SPACING in traditional animation.

  Without variable timing, the swing feels robotic and lifeless.
```

**Implementation with variable durations:**

```
Instead of a single frameDuration, store an array:

  Animation "sword_swing":
    frames:    [0,    1,    2,    3,    4   ]
    durations: [0.20, 0.15, 0.05, 0.10, 0.20]  // seconds per frame
    loop: false

  The accumulator pattern changes slightly:

  Each update(deltaTime):
    elapsedTime += deltaTime;

    while (elapsedTime >= durations[currentFrame]) {
        elapsedTime -= durations[currentFrame];
        currentFrame++;

        if (currentFrame >= totalFrames) {
            if (looping) {
                currentFrame = 0;
            } else {
                currentFrame = totalFrames - 1;
                break;  // Stop advancing, stay on last frame
            }
        }
    }

  Note the WHILE loop instead of IF: with variable durations,
  a single deltaTime could span multiple frames (if a frame's
  duration is very short and deltaTime is large).
```

### Animation Speed Curves

Speed curves control how the animation advances through its frames over time. This is analogous to CSS `animation-timing-function` (linear, ease-in, ease-out, cubic-bezier).

**Linear (constant speed):** Every frame gets equal time. This is the default and works for most game animations. Walk cycles, idle loops, and most action animations use linear timing.

**Ease-in (slow start, fast end):** Early frames display longer, later frames display shorter. Use this for acceleration effects -- a character starting to run.

**Ease-out (fast start, slow end):** Early frames display shorter, later frames display longer. Use this for deceleration -- a character coming to a stop.

```
Linear:                   Ease-in:                  Ease-out:
Frame: 0 1 2 3 4 5       Frame: 0 1 2 3 4 5       Frame: 0 1 2 3 4 5
       | | | | | |              |   |  | || |              || |  |   |
Time:  ─────────────>     Time:  ─────────────>     Time:  ─────────────>
       Equal spacing            Slow then fast            Fast then slow

Implementation: Multiply each frame's base duration by a curve factor.

// Compute ease-in factor for frame i out of N total frames
float easeInFactor(int i, int N) {
    float t = static_cast<float>(i) / (N - 1);  // 0.0 to 1.0
    return 2.0f - t;  // 2.0 for first frame (slow), 1.0 for last (fast)
}

// Compute ease-out factor
float easeOutFactor(int i, int N) {
    float t = static_cast<float>(i) / (N - 1);
    return 1.0f + t;  // 1.0 for first frame (fast), 2.0 for last (slow)
}

// Apply to base duration
float baseDuration = 0.15;
float actualDuration = baseDuration * easeInFactor(currentFrame, totalFrames);
```

In practice, per-frame variable durations (from the previous section) give you more control than mathematical curves. Curves are useful when you want to apply a timing feel to an animation without manually specifying every frame's duration.

### Slow Motion and Speed-Up Effects

Game-wide or per-entity speed modifiers are trivially implemented by scaling deltaTime.

```
// Normal speed
animation.update(deltaTime);

// Slow motion (half speed)
animation.update(deltaTime * 0.5f);

// Speed up (double speed, e.g., haste buff)
animation.update(deltaTime * 2.0f);

// Freeze (pause animation completely)
animation.update(0.0f);

// Reverse (play backwards -- negative deltaTime)
animation.update(-deltaTime);
// (This requires the animation system to handle negative elapsed time,
//  decrementing currentFrame instead of incrementing.)
```

This is exactly how slow-motion effects work in games like Celeste (the assist mode slow motion) or Superhot (time moves when you move). The game applies a time scale factor to all delta times before passing them to systems.

```
Common pattern: global time scale

float globalTimeScale = 1.0f;  // Set to 0.5 for slow-mo, 0.0 for pause

void Game::update(float rawDeltaTime) {
    float scaledDeltaTime = rawDeltaTime * globalTimeScale;

    player.update(scaledDeltaTime);        // Movement slows
    player.animator.update(scaledDeltaTime); // Animation slows
    enemies.update(scaledDeltaTime);        // Enemies slow

    // UI always runs at full speed (menus should not slow down)
    ui.update(rawDeltaTime);
}
```

### Frame Skipping and Lag Spike Handling

What happens if `deltaTime` is very large? This occurs during lag spikes, when the game hitches due to loading, garbage collection, or another process consuming CPU time.

```
Normal frame: deltaTime = 0.016s (60fps)
Lag spike:    deltaTime = 0.500s (game froze for half a second!)

If frameDuration is 0.15s and deltaTime is 0.500s:
  0.500 / 0.15 = 3.33 frames should have passed!

Without handling, the while loop from the accumulator pattern would
advance through 3 frames in a single update. The player would see
the animation "jump" ahead -- frame 0 to frame 3 instantly.
```

**Strategy 1: Clamp deltaTime (most common)**

```
// Cap deltaTime to prevent large jumps
float maxDeltaTime = 0.1f;  // Never process more than 100ms per update
float clampedDelta = std::min(deltaTime, maxDeltaTime);
animation.update(clampedDelta);

// During a lag spike, the animation temporarily slows down instead
// of jumping. When the spike ends, it resumes normally. This is
// usually invisible to the player because they were lagging anyway.
```

**Strategy 2: Skip intermediate frames**

```
// Allow the while loop to advance multiple frames, but only RENDER
// the final frame. The intermediate frames are "skipped" visually
// but their timing is consumed correctly.

while (elapsedTime >= durations[currentFrame]) {
    elapsedTime -= durations[currentFrame];
    currentFrame = (currentFrame + 1) % totalFrames;
    // No rendering here -- just advance the counter
}
// Render only the final frame
```

**Strategy 3: Reset on spike**

```
// If deltaTime exceeds a threshold, reset the animation state
if (deltaTime > 0.5f) {
    elapsedTime = 0.0f;
    // Optionally reset to frame 0, or just continue from current frame
}
```

**Recommended for your project:** Clamp deltaTime (Strategy 1). It is the simplest and most commonly used approach. Games like Celeste and Hollow Knight use this.

### Web Development Comparisons

The animation timing concepts in this section have direct parallels in web development:

```
Game concept:                       Web equivalent:

deltaTime                           requestAnimationFrame's timestamp parameter
  (time since last update)             (time since page load, diff = delta)

accumulator pattern                 setTimeout/setInterval with drift correction
  (subtract frameDuration,             (adjust next timeout based on how late
   carry remainder)                     the current one fired)

frameDuration                       CSS animation-duration / number of keyframes
  (seconds per frame)                  (each @keyframes step has a percentage)

variable frame durations            CSS @keyframes with uneven percentage stops
  (per-frame timing)                   @keyframes { 0% {} 60% {} 70% {} 100% {} }

speed scale factor                  CSS animation-duration being shorter/longer
  (multiply deltaTime by 0.5)          (duration: 2s -> duration: 4s = half speed)

animation pause                     CSS animation-play-state: paused
  (update with deltaTime = 0)          (or cancelAnimationFrame)

deltaTime clamping                  requestAnimationFrame's built-in behavior
  (cap to prevent huge jumps)          (browsers cap rAF to ~60fps even in
                                        background tabs, preventing huge deltas
                                        when switching back to the tab)
```

---

## 4. Animation State Machines -- Complete Design

### What Is a Finite State Machine?

A finite state machine (FSM) is a system that can be in exactly one **state** at any time, and transitions between states are governed by **conditions**. If you have used Redux in web development, you already understand this concept:

```
Redux (web):
  State: { screen: "login" }
  Action: { type: "LOGIN_SUCCESS" }
  Reducer: if state.screen == "login" && action.type == "LOGIN_SUCCESS"
           -> new state: { screen: "dashboard" }

Animation FSM (game):
  State: IDLE
  Condition: player pressed movement key
  Transition: IDLE -> WALK

Both are the same pattern:
  current_state + condition = next_state
```

The key properties of an FSM:
1. **Finite states:** There is a fixed, enumerable set of states (idle, walk, attack, etc.)
2. **One active state:** The system is in exactly one state at any time
3. **Defined transitions:** Moving between states only happens through explicit transition rules
4. **Deterministic:** Given a state and a condition, the next state is always the same

### States for a Top-Down RPG Character

Here are the typical animation states for a Stardew Valley-style character:

```
┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐
│  IDLE   │  │  WALK   │  │   RUN   │  │  SWIM   │
│(looping)│  │(looping)│  │(looping)│  │(looping)│
└─────────┘  └─────────┘  └─────────┘  └─────────┘

┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐
│ ATTACK  │  │  CAST   │  │   USE   │  │  FISH   │
│(one-shot)│  │(one-shot)│  │(one-shot)│  │(one-shot → hold → one-shot)│
└─────────┘  └─────────┘  └─────────┘  └─────────┘

┌─────────┐  ┌─────────┐  ┌─────────┐
│  HURT   │  │   DIE   │  │  EAT   │
│(one-shot)│  │(one-shot)│  │(one-shot)│
└─────────┘  └─────────┘  └─────────┘

IDLE:    Standing still, subtle breathing animation. Looping.
WALK:    Moving at normal speed. Looping. 4-8 frames.
RUN:     Moving at increased speed. Looping. Similar to walk but faster cycle.
SWIM:    In water. Looping. Different sprite set.
ATTACK:  Swinging a tool/weapon. One-shot, returns to IDLE when done.
CAST:    Using a spell/ability. One-shot.
USE:     Using an item (watering can, hoe). One-shot.
FISH:    Casting line (one-shot), waiting (loop), reeling (one-shot).
HURT:    Taking damage. One-shot, brief flash, returns to previous state.
DIE:     Health reached zero. One-shot, entity deactivated after.
EAT:     Consuming food/item. One-shot.
```

### Transitions, Conditions, and Priorities

Transitions define how the FSM moves between states. Each transition has:

```
Transition = {
    from:       source state (or ANY for universal transitions)
    to:         target state
    condition:  what triggers this transition
    priority:   which transition wins if multiple conditions are true
    canInterrupt: can this transition cancel the current animation mid-frame?
}
```

Example transition table for a Stardew Valley character:

```
FROM        CONDITION                  TO         PRIORITY   INTERRUPT?
--------    -----------------------    --------   --------   ----------
ANY         health <= 0                DIE        100        Yes
ANY         damage taken               HURT       90         Yes
IDLE        movement input             WALK       10         Yes
WALK        no movement input          IDLE       10         Yes
WALK        run button held            RUN        20         Yes
RUN         run button released        WALK       20         Yes
IDLE        attack input               ATTACK     50         Yes
WALK        attack input               ATTACK     50         Yes
ATTACK      animation finished         IDLE       10         No (waits)
HURT        animation finished         IDLE       10         No (waits)
IDLE        use item input             USE        50         Yes
USE         animation finished         IDLE       10         No (waits)
IDLE        enter water                SWIM       30         Yes
SWIM        exit water                 IDLE       30         Yes
```

**Priority system:** When multiple transitions are valid simultaneously (e.g., the player presses attack while also moving), the highest priority transition wins. Damage (priority 90) overrides everything except death (priority 100). Movement (priority 10) is the lowest priority.

**Interrupt rules:** Some states should not be interruptible. When the player is in the ATTACK state, they should not be able to cancel the attack mid-swing by pressing a movement key. The "animation finished" condition is the only way to leave ATTACK (except for HURT and DIE, which have higher priority and override the interrupt lock).

```
Interrupt flow:

Player presses attack:
  IDLE ─── [attack pressed, priority 50] ───> ATTACK
                                                │
Player presses movement during attack:          │
  ATTACK ─── [movement pressed, priority 10] ── X (BLOCKED -- attack
             is not interruptible by lower priority transitions)
                                                │
Enemy hits player during attack:                │
  ATTACK ─── [damage taken, priority 90] ───> HURT
             (ALLOWED -- hurt priority > attack's interrupt lock)
```

### One-Shot vs. Looping Animations

**Looping animations** repeat forever: idle, walk, run. When the last frame is reached, the animation wraps back to frame 0. These are typically "ambient" or "movement" animations that play as long as a condition is true.

**One-shot animations** play once and stop: attack, hurt, die. When the last frame is reached, the animation either holds the last frame (for "die") or triggers a transition back to a default state (for "attack" returning to "idle").

```
Looping:
  Frame: 0 -> 1 -> 2 -> 3 -> 0 -> 1 -> 2 -> 3 -> 0 -> 1 -> ...
  Never ends. Must be explicitly stopped by a state transition.

One-shot:
  Frame: 0 -> 1 -> 2 -> 3 -> (DONE)
  Plays once. Fires an "animation finished" event when done.
  The state machine uses this event to transition to the next state.

One-shot with hold:
  Frame: 0 -> 1 -> 2 -> 3 -> 3 -> 3 -> 3 -> ...
  Plays once, then holds the last frame indefinitely.
  Used for "die" (character stays dead) or "open chest" (chest stays open).
```

### Animation Events and Callbacks

Many animations need to trigger game logic at specific frames, not just at the beginning or end. This is the animation event system.

```
Attack animation timeline:

  Frame 0       Frame 1       Frame 2       Frame 3       Frame 4
  Wind-up       Peak          Strike!       Follow        Recovery
  ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐
  │  ╱      │   │   │     │   │    ╲    │   │      ╲  │   │         │
  │ ╱       │   │   │     │   │     ╲   │   │       │ │   │    │    │
  │╱ ready  │   │   │ up  │   │ ──── ╲  │   │  done │ │   │    │    │
  └─────────┘   └─────────┘   └─────────┘   └─────────┘   └─────────┘
                                  ^               ^
                                  │               │
                           "deal_damage"    "play_swoosh_sound"
                           event fires      event fires

Events:
  Frame 0: "play_windup_sound"     -> plays a subtle whoosh
  Frame 2: "deal_damage"           -> checks hitbox, applies damage to enemies
  Frame 2: "spawn_particle"        -> creates a slash particle effect
  Frame 3: "play_hit_sound"        -> plays impact sound (if hit landed)
  Frame 4: (animation_finished)    -> built-in event, transitions to IDLE
```

Without animation events, you would have to either:
- Apply damage at the START of the attack (feels wrong -- damage before the swing)
- Apply damage at the END of the attack (feels laggy -- visible delay between swing and effect)
- Apply damage in the middle using a separate timer (fragile, breaks if animation timing changes)

Animation events solve this by letting you say: "When the animation reaches frame 2, call this function." The timing is always correct because it is defined relative to the animation itself.

In web development, this is similar to CSS `animationend` / `animationiteration` events, or the `onComplete` callbacks in animation libraries like GSAP or Framer Motion. The game version is more granular -- per-frame callbacks rather than just start/end.

### Queueing Animations

Sometimes you want to chain animations: play attack, then automatically return to idle. This is **animation queueing**.

```
Simple queueing:

  Queue: [ATTACK, IDLE]

  1. Pop ATTACK from the queue. Play it.
  2. ATTACK finishes (one-shot). Pop IDLE from the queue. Play it.
  3. IDLE loops indefinitely until a new transition overrides it.

More complex queueing (combo system):

  Player presses attack three times quickly:
  Queue: [ATTACK_1, ATTACK_2, ATTACK_3, IDLE]

  1. ATTACK_1 plays. Near the end, check if ATTACK_2 is queued.
  2. If yes, transition smoothly to ATTACK_2 (combo continues).
  3. If no (player didn't press attack again), transition to IDLE.
  4. ATTACK_2 plays. Same check for ATTACK_3.
  5. After ATTACK_3, return to IDLE.

  This creates a combo system where button timing affects gameplay.
```

The simplest implementation: the state machine has a `nextState` field. When a one-shot animation finishes, it transitions to `nextState` (which defaults to IDLE). The transition logic can override `nextState` when queueing combos.

### Blending Considerations

In frame-by-frame animation, "blending" between animations is NOT the smooth interpolation you see in 3D games (where skeletal rigs smoothly transition between poses). Instead, it is a **snap transition**: one frame of the old animation, then immediately the first frame of the new animation.

```
3D skeletal blending (NOT what frame-by-frame does):
  Walk frame 3 ~~smoothly interpolates over 0.1s~~> Idle frame 0
  Each joint angle is linearly interpolated between the two poses.

Frame-by-frame "blending" (what actually happens):
  Walk frame 3 | Idle frame 0
               ^
         Instant cut. One frame to the next.
```

This instant cut is fine for pixel art. At low resolutions (16x24, 32x48), there is not enough visual information for a smooth blend to matter. The player's eye fills in the transition naturally.

There are two strategies for making the snap less jarring:

**Strategy 1: Transition frames.** Include a frame in the sprite sheet that is specifically drawn to bridge two animations. For example, a "walk-to-idle" transition frame that shows the character mid-step coming to a stop.

**Strategy 2: Choose the best starting frame.** When transitioning from walk to idle, start the idle animation on the frame that most closely matches the walk animation's current pose. If walk frame 2 has the character's left foot forward, start idle on a frame with a similar stance.

Both of these are artistic solutions, not technical ones. The engine simply needs to support "start animation at frame N" and "play this animation before that animation" (queueing). The artist decides which frames to use.

### Hierarchical State Machines

For complex characters, a flat FSM becomes unwieldy. A hierarchical state machine (HSM) groups related states into layers.

```
Flat FSM (gets complicated fast):

  IDLE_GROUND, WALK_GROUND, RUN_GROUND, ATTACK_GROUND,
  IDLE_WATER, SWIM_WATER, DIVE_WATER,
  IDLE_AIR, FALL_AIR, GLIDE_AIR,
  CLIMB_UP, CLIMB_DOWN, CLIMB_IDLE,
  HURT, DIE

  22 states, and transitions between all of them are possible.
  That's potentially 22 * 21 = 462 transition rules to define!


Hierarchical FSM (organized):

  Top Level: [GROUND] [WATER] [AIR] [CLIMBING] [DEAD]

  GROUND sub-states:                 WATER sub-states:
  ┌──────────────────┐               ┌──────────────────┐
  │ idle             │               │ idle (treading)   │
  │ walk             │               │ swim              │
  │ run              │               │ dive              │
  │ attack           │               └──────────────────┘
  │ use_item         │
  └──────────────────┘

  AIR sub-states:                    CLIMBING sub-states:
  ┌──────────────────┐               ┌──────────────────┐
  │ falling          │               │ climb_up          │
  │ gliding          │               │ climb_down        │
  └──────────────────┘               │ climb_idle        │
                                     └──────────────────┘

  Top-level transitions (few):
    GROUND ──[enter water]──> WATER
    GROUND ──[fall off edge]──> AIR
    GROUND ──[grab ladder]──> CLIMBING
    ANY ──[health <= 0]──> DEAD

  Sub-state transitions (within each group):
    GROUND.idle ──[move]──> GROUND.walk
    GROUND.walk ──[stop]──> GROUND.idle
    etc.

  This is 5 top-level states with ~3-5 sub-states each.
  Transitions within a group are simple (walk <-> idle <-> run).
  Transitions between groups are few (enter water, fall off edge).
  Total rules: ~20-30 instead of ~400.
```

In web development terms, this is similar to nested React Router routes or XState's hierarchical state charts. The top-level state determines which "page" you are on, and sub-states handle the details within that page.

For a Stardew Valley clone, a flat FSM is sufficient. The character does not have complex movement modes (no climbing, no air states). A flat FSM with 8-12 states and ~20 transition rules is very manageable.

---

## 5. Direction Handling -- Top-Down Specifics

### 4-Directional vs. 8-Directional Animation

**4-directional:** The character has distinct sprites for up, down, left, and right. This is the standard for pixel art top-down RPGs. Stardew Valley, classic Zelda, Pokemon, Earthbound, and most RPG Maker games use 4-directional animation.

**8-directional:** The character also has distinct sprites for the four diagonals (up-left, up-right, down-left, down-right). This is less common in pixel art because it doubles the art workload, but games like Diablo II and Ragnarok Online use it.

```
4-directional:                       8-directional:

        UP                                  UP
        │                            UL    │    UR
        │                              ╲   │   ╱
  LEFT──┼──RIGHT                  LEFT──┼──RIGHT
        │                              ╱   │   ╲
        │                            DL    │    DR
       DOWN                                DOWN

Art requirement:                     Art requirement:
  4 directions x N frames             8 directions x N frames
  e.g., 4 x 6 = 24 frames            e.g., 8 x 6 = 48 frames

4-directional is standard for pixel art because:
  1. Fewer frames to draw (half the work)
  2. Diagonal pixel art often looks awkward at low resolutions
  3. The visual style is established and expected by players
  4. Stardew Valley uses it, and that's your reference
```

### Last-Facing Direction for Idle

When the player stops moving, the character should face the direction they were last moving in. This is the "last-facing direction" pattern.

```
Player walks right, then stops:
  WRONG: Character snaps to "idle_down" (default direction)
  RIGHT: Character shows "idle_right" (last movement direction)

Implementation:

  enum Direction { DOWN, LEFT, RIGHT, UP };

  Direction facingDirection = DOWN;   // Default facing direction

  void Player::update(float deltaTime) {
      Vector2 input = getMovementInput();

      if (input.length() > 0) {
          // Player is moving -- update facing direction
          facingDirection = directionFromVector(input);
          animator.play("walk", facingDirection);
      } else {
          // Player stopped -- keep facingDirection unchanged
          animator.play("idle", facingDirection);
      }
  }

  Direction directionFromVector(Vector2 v) {
      // Determine the dominant axis
      if (abs(v.x) > abs(v.y)) {
          return v.x > 0 ? RIGHT : LEFT;
      } else {
          return v.y > 0 ? DOWN : UP;
      }
  }
```

Note: in your engine's coordinate system (Y increases downward, as set by your orthographic projection with top=0, bottom=viewportHeight), positive Y movement means moving **down** the screen.

### Horizontal Sprite Flipping

A common optimization: draw the character facing right, and mirror the sprite horizontally to produce the left-facing version. This halves the art required for left/right directions.

```
Sprite sheet with flipping:

         Frame 0   Frame 1   Frame 2   Frame 3
        ┌─────────┬─────────┬─────────┬─────────┐
Row 0:  │ walk D0 │ walk D1 │ walk D2 │ walk D3 │  <- Down (unique art)
        ├─────────┼─────────┼─────────┼─────────┤
Row 1:  │ walk R0 │ walk R1 │ walk R2 │ walk R3 │  <- Right (unique art)
        ├─────────┼─────────┼─────────┼─────────┤
Row 2:  │ walk U0 │ walk U1 │ walk U2 │ walk U3 │  <- Up (unique art)
        └─────────┴─────────┴─────────┴─────────┘

        "Walk Left" is produced by flipping "Walk Right" at render time.
        Only 3 rows of art needed instead of 4.
```

There are two ways to flip a sprite:

**Method 1: Negate the model's X scale (simplest)**

```cpp
// In your Sprite::draw() or wherever the model matrix is built:

glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, 0.0f));

if (facingLeft) {
    // Flip horizontally: negate X scale and offset to compensate
    // Without the offset, the sprite would flip around its left edge
    // and appear one sprite-width to the left of where it should be.
    model = glm::translate(model, glm::vec3(size.x, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(-size.x, size.y, 1.0f));
} else {
    model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f));
}

// The negative X scale causes the GPU to render the quad mirrored.
// The texture coordinates are unchanged -- the geometry itself is flipped.
```

**Method 2: Swap U coordinates**

```
Normal (facing right):
  Top-left vertex:     UV = (u_left,  v_top)     = (0.375, 0.25)
  Top-right vertex:    UV = (u_right, v_top)     = (0.5,   0.25)
  Bottom-left vertex:  UV = (u_left,  v_bottom)  = (0.375, 0.5)
  Bottom-right vertex: UV = (u_right, v_bottom)  = (0.5,   0.5)

Flipped (facing left):
  Top-left vertex:     UV = (u_right, v_top)     = (0.5,   0.25)   <- SWAPPED
  Top-right vertex:    UV = (u_left,  v_top)     = (0.375, 0.25)   <- SWAPPED
  Bottom-left vertex:  UV = (u_right, v_bottom)  = (0.5,   0.5)    <- SWAPPED
  Bottom-right vertex: UV = (u_left,  v_bottom)  = (0.375, 0.5)    <- SWAPPED

The quad geometry stays the same. The texture is mapped in reverse horizontally.
```

Both methods produce identical results. Method 1 (scale negation) is simpler because you do not need to modify UV coordinates -- you just flip the geometry.

**When flipping does NOT work:** Asymmetric characters. If the character holds a sword in their right hand, flipping makes them hold it in their left hand. If the character has an eye patch on one side, flipping moves it to the other side. For such characters, you need separate left and right sprites. Stardew Valley handles this by having most characters be fairly symmetric, but the player's held tools are drawn asymmetrically and require special handling.

### Diagonal Movement Priority

When the player presses both right and down simultaneously, the character moves diagonally. But which direction animation should play? You only have 4 directional animations (no diagonal sprites).

```
Input: right + down simultaneously
Movement: diagonal (both X and Y change)
Animation: must choose one of 4 directions

Common approaches:

1. Dominant axis (most common, used by Stardew Valley):
   Pick the axis with the larger input magnitude.

   if (abs(velocity.x) > abs(velocity.y)) {
       direction = velocity.x > 0 ? RIGHT : LEFT;
   } else {
       direction = velocity.y > 0 ? DOWN : UP;
   }

   Problem: If velocity.x == velocity.y (perfect diagonal),
   one axis must win. Convention: vertical wins (DOWN/UP preferred).

2. Last-changed axis:
   The direction changes to whichever key was pressed most recently.

   Player holds right, then also presses down:
     -> animation shows DOWN (down was pressed most recently)

   Player holds down, then also presses right:
     -> animation shows RIGHT (right was pressed most recently)

   This feels responsive but can be flickery during true diagonals.

3. Priority order:
   Hard-coded priority: DOWN > UP > LEFT > RIGHT (or any fixed order).
   Simple but feels unresponsive because the animation does not match
   the player's most recent input.

Recommendation for Stardew Valley clone: Use dominant axis (approach 1)
with vertical preference on ties. This matches Stardew's behavior.
```

### Sprite Sheet Layouts for Directional Animation

Different games and tools use different row orders for directional animations. Here are the most common:

```
RPG Maker VX/MV convention:
  Row 0: Down
  Row 1: Left
  Row 2: Right
  Row 3: Up
  (3 frames per direction, middle frame is the "standing" pose)

Stardew Valley convention (approximate):
  Rows grouped by animation, then by direction within each animation.
  Walk Down is followed by Walk Right, then Walk Up.
  Walk Left is produced by flipping Walk Right.

LPC (Liberated Pixel Cup) convention:
  Row 0: Walk Up
  Row 1: Walk Left
  Row 2: Walk Down
  Row 3: Walk Right
  (Used by many open-source sprite packs on opengameart.org)

Your custom convention (pick one and be consistent):
  Row 0: Idle Down    (1-4 frames)
  Row 1: Idle Right   (1-4 frames, flip for Left)
  Row 2: Idle Up      (1-4 frames)
  Row 3: Walk Down    (4-8 frames)
  Row 4: Walk Right   (4-8 frames, flip for Left)
  Row 5: Walk Up      (4-8 frames)
  Row 6: Attack Down  (4-6 frames)
  Row 7: Attack Right (4-6 frames, flip for Left)
  Row 8: Attack Up    (4-6 frames)
  ... and so on

  This groups by (animation, direction) pairs.
  The engine knows that row = animationIndex * 3 + directionIndex.
  Mapping: DOWN=0, RIGHT=1, UP=2. LEFT is RIGHT flipped.
```

The convention you choose is a contract between the artist and the engine. Document it clearly and enforce it consistently. If an artist puts the walk animation on the wrong row, the character will display the wrong animation for a given state.

---

## 6. Implementation Architecture

### The Animation Class

The `Animation` class represents a single named animation clip: a sequence of frames with timing data.

```cpp
// Animation.h -- a single animation clip (e.g., "walk_down")
// This is DATA: it describes what frames to play and how fast.
// It does NOT track playback state (current frame, elapsed time).

struct Animation {
    std::string name;           // "walk_down", "idle_up", "attack_right"
    int row;                    // Which row of the sprite sheet this animation lives on
    int startFrame;             // First frame column (usually 0)
    int frameCount;             // Number of frames in this animation
    std::vector<float> frameDurations;  // Duration of each frame in seconds
    bool loops;                 // Does this animation repeat?

    // Construct an animation with uniform frame timing
    Animation(const std::string& name, int row, int startFrame, int frameCount,
              float frameDuration, bool loops)
        : name(name), row(row), startFrame(startFrame), frameCount(frameCount),
          loops(loops)
    {
        // Fill all frames with the same duration
        frameDurations.resize(frameCount, frameDuration);
    }

    // Construct an animation with per-frame timing
    Animation(const std::string& name, int row, int startFrame,
              const std::vector<float>& durations, bool loops)
        : name(name), row(row), startFrame(startFrame),
          frameCount(static_cast<int>(durations.size())),
          frameDurations(durations), loops(loops)
    {
    }

    // Get the total duration of one full cycle of this animation
    float getTotalDuration() const {
        float total = 0.0f;
        for (float d : frameDurations) {
            total += d;
        }
        return total;
    }
};
```

### The Animator / AnimationController Component

The `Animator` manages playback state and owns a collection of animations. It tracks which animation is currently playing, the current frame, and elapsed time.

```cpp
// Animator.h -- manages animation playback and state transitions

class Animator {
private:
    // All available animations, keyed by name
    std::unordered_map<std::string, Animation> animations;

    // Current playback state
    const Animation* currentAnimation;   // Pointer to the active animation
    int currentFrame;                     // Current frame index (within the animation)
    float elapsedTime;                    // Time accumulated since last frame change
    bool finished;                        // True if a one-shot animation has completed
    float speedMultiplier;                // 1.0 = normal, 0.5 = half speed, etc.

    // Direction state
    Direction facingDirection;            // DOWN, LEFT, RIGHT, UP

    // Event callbacks
    // Maps (animationName, frameIndex) -> callback function
    std::unordered_map<std::string,
        std::unordered_map<int, std::function<void()>>> frameEvents;

    // Callback for when any one-shot animation finishes
    std::function<void(const std::string&)> onAnimationFinished;

public:
    Animator()
        : currentAnimation(nullptr),
          currentFrame(0),
          elapsedTime(0.0f),
          finished(false),
          speedMultiplier(1.0f),
          facingDirection(Direction::DOWN),
          onAnimationFinished(nullptr)
    {
    }

    // Register an animation clip
    void addAnimation(const Animation& anim) {
        animations[anim.name] = anim;
    }

    // Play a named animation (resets playback if different from current)
    void play(const std::string& name) {
        auto it = animations.find(name);
        if (it == animations.end()) {
            return;  // Animation not found
        }

        const Animation* newAnim = &it->second;

        // Only reset playback if switching to a different animation
        if (currentAnimation != newAnim) {
            currentAnimation = newAnim;
            currentFrame = 0;
            elapsedTime = 0.0f;
            finished = false;
        }
    }

    // Update animation timing. Called every frame with delta time.
    void update(float deltaTime) {
        if (currentAnimation == nullptr || finished) {
            return;
        }

        elapsedTime += deltaTime * speedMultiplier;

        // Advance frames while enough time has accumulated
        while (elapsedTime >= currentAnimation->frameDurations[currentFrame]) {
            elapsedTime -= currentAnimation->frameDurations[currentFrame];

            // Check for frame events on the frame we are LEAVING
            fireFrameEvent(currentAnimation->name, currentFrame);

            currentFrame++;

            if (currentFrame >= currentAnimation->frameCount) {
                if (currentAnimation->loops) {
                    currentFrame = 0;
                } else {
                    currentFrame = currentAnimation->frameCount - 1;
                    finished = true;

                    // Notify that the animation finished
                    if (onAnimationFinished) {
                        onAnimationFinished(currentAnimation->name);
                    }
                    break;
                }
            }
        }
    }

    // Get the current frame's column in the sprite sheet
    int getFrameColumn() const {
        if (currentAnimation == nullptr) return 0;
        return currentAnimation->startFrame + currentFrame;
    }

    // Get the current animation's row in the sprite sheet
    int getFrameRow() const {
        if (currentAnimation == nullptr) return 0;
        return currentAnimation->row;
    }

    // Check if the current one-shot animation has finished
    bool isFinished() const {
        return finished;
    }

    // Set the speed multiplier (1.0 = normal, 0.5 = half speed)
    void setSpeedMultiplier(float multiplier) {
        speedMultiplier = multiplier;
    }

    // Register a callback for a specific frame of a specific animation
    void onFrame(const std::string& animName, int frame, std::function<void()> callback) {
        frameEvents[animName][frame] = callback;
    }

    // Register a callback for when any one-shot animation finishes
    void setOnFinished(std::function<void(const std::string&)> callback) {
        onAnimationFinished = callback;
    }

    // Set facing direction
    void setDirection(Direction dir) {
        facingDirection = dir;
    }

    Direction getDirection() const {
        return facingDirection;
    }

private:
    void fireFrameEvent(const std::string& animName, int frame) {
        auto animIt = frameEvents.find(animName);
        if (animIt != frameEvents.end()) {
            auto frameIt = animIt->second.find(frame);
            if (frameIt != animIt->second.end()) {
                frameIt->second();  // Call the registered callback
            }
        }
    }
};
```

### Separation of Concerns

The animation system has three distinct responsibilities that should be kept separate:

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  ANIMATION DATA │     │ ANIMATION LOGIC  │     │   RENDERING     │
│                 │     │                  │     │                 │
│  - Frame list   │     │  - Current frame │     │  - UV offset    │
│  - Durations    │     │  - Elapsed time  │     │  - UV size      │
│  - Loop flag    │     │  - State machine │     │  - Texture bind │
│  - Row in sheet │     │  - Direction     │     │  - Quad draw    │
│                 │     │  - Events        │     │                 │
│  (Animation     │────>│  (Animator       │────>│  (Sprite        │
│   struct)       │     │   class)         │     │   class)        │
└─────────────────┘     └──────────────────┘     └─────────────────┘

Data flows left to right:
  1. Animation data defines WHAT can be played
  2. Animator decides WHICH frame to show right now
  3. Sprite renders THAT frame using UV coordinates

This separation means:
  - You can change animation data (add frames, adjust timing) without touching rendering code
  - You can swap rendering backends (OpenGL -> Vulkan) without touching animation logic
  - You can test animation logic without a GPU (unit testing)
```

This maps to web development's MVC/MVVM patterns:
- **Animation data** = Model (the data)
- **Animator** = Controller/ViewModel (the logic)
- **Sprite rendering** = View (the presentation)

### Integration with Entity and Sprite Systems

Here is how the animation system integrates with your existing engine architecture. Your current flow is:

```
Current flow (no animation):

  Player::update(deltaTime)
    -> updates position
    -> sets sprite position

  Player::render(shader, camera)
    -> sprite->draw(shader, camera)
       -> binds full texture
       -> draws entire texture as UV (0,0) to (1,1)

New flow (with animation):

  Player::update(deltaTime)
    -> updates position
    -> sets sprite position
    -> animator.update(deltaTime)           // NEW: advance animation frame
    -> compute UV offset from animator      // NEW: get current frame's UV

  Player::render(shader, camera)
    -> sprite->draw(shader, camera, uvOffset, uvSize)   // MODIFIED: pass UV region
       -> binds sprite sheet texture
       -> sets uvOffset and uvSize uniforms
       -> draws sub-region of texture
```

The key change is that your Sprite class needs to support rendering a sub-region of its texture (using `uvOffset` and `uvSize`), which is exactly what your tile shader already does. In fact, you could reuse the tile shader for animated sprites with minimal changes.

### Complete Pseudocode for All Classes

Here is the complete interaction between all classes, showing how data flows from input to screen.

```
// ===== SETUP (at character creation time) =====

// 1. Load the sprite sheet texture
Texture* spriteSheet = new Texture("assets/sprites/player.png");
// spriteSheet is 256x192, frames are 32x48, grid is 8 columns x 4 rows

// 2. Create the Animator and register animations
Animator animator;

// Row 0: walk down, 4 frames, 0.15s each, looping
animator.addAnimation(Animation("walk_down", 0, 0, 4, 0.15f, true));

// Row 1: walk right, 4 frames, 0.15s each, looping
animator.addAnimation(Animation("walk_right", 1, 0, 4, 0.15f, true));

// Row 2: walk up, 4 frames, 0.15s each, looping
animator.addAnimation(Animation("walk_up", 2, 0, 4, 0.15f, true));

// Row 3: attack down, 5 frames, variable timing, one-shot
animator.addAnimation(Animation("attack_down", 3, 0,
    {0.20f, 0.15f, 0.05f, 0.10f, 0.20f}, false));

// Register an event: deal damage on frame 2 of the attack animation
animator.onFrame("attack_down", 2, [this]() {
    checkAttackHitbox();
});

// Register animation-finished callback
animator.setOnFinished([this](const std::string& animName) {
    if (animName.find("attack") != std::string::npos) {
        // Attack finished, return to idle
        currentState = IDLE;
    }
});

// 3. Compute UV size (constant for this sprite sheet)
float uvWidth  = 32.0f / 256.0f;   // = 0.125
float uvHeight = 48.0f / 192.0f;   // = 0.25


// ===== GAME LOOP (every frame) =====

// 1. Process input and update state machine
void Player::handleInput() {
    Vector2 input = getMovementInput();

    if (currentState == ATTACK) {
        // Cannot interrupt attack with movement
        return;
    }

    if (input.length() > 0) {
        currentState = WALK;
        Direction dir = directionFromVector(input);
        animator.setDirection(dir);

        // Select the correct directional animation
        std::string animName = "walk_" + directionToString(dir);
        animator.play(animName);
    } else {
        currentState = IDLE;
        // Use current facing direction for idle
        std::string animName = "idle_" + directionToString(animator.getDirection());
        animator.play(animName);
    }

    if (attackButtonPressed() && currentState != ATTACK) {
        currentState = ATTACK;
        std::string animName = "attack_" + directionToString(animator.getDirection());
        animator.play(animName);
    }
}

// 2. Update animation timing
void Player::update(float deltaTime) {
    handleInput();
    animator.update(deltaTime);
    // ... movement code ...
}

// 3. Render with correct UV
void Player::render(Shader& shader, const Camera& camera) {
    int col = animator.getFrameColumn();
    int row = animator.getFrameRow();

    float uvOffsetX = col * uvWidth;    // e.g., 2 * 0.125 = 0.25
    float uvOffsetY = row * uvHeight;   // e.g., 1 * 0.25  = 0.25

    bool flipX = (animator.getDirection() == LEFT);

    // Set shader uniforms
    shader.use();
    shader.setVec2("uvOffset", uvOffsetX, uvOffsetY);
    shader.setVec2("uvSize", flipX ? -uvWidth : uvWidth, uvHeight);
    // Note: negative uvWidth flips the texture horizontally

    // Build model matrix
    glm::mat4 model = glm::translate(glm::mat4(1.0f),
        glm::vec3(position.x, position.y, 0.0f));
    model = glm::scale(model, glm::vec3(32.0f, 48.0f, 1.0f));
    shader.setMat4("model", model);

    // Bind and draw
    spriteSheet->bind(0);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
```

---

## 7. Optimization Techniques

### Texture Atlasing Multiple Characters

Instead of one texture per character (requiring a texture bind per character per frame), pack multiple characters into a single large texture.

```
Individual textures:                     Shared atlas:
┌────────┐ ┌────────┐ ┌────────┐       ┌──────────────────────────┐
│ Player │ │ NPC_A  │ │ NPC_B  │       │ Player  │ NPC_A  │ NPC_B │
│ 256x256│ │ 128x128│ │ 128x128│       │         │        │       │
└────────┘ └────────┘ └────────┘       │         │        │       │
3 texture binds per frame               └──────────────────────────┘
                                        1 texture bind per frame

With a shared atlas, rendering 10 characters is:
  1 glBindTexture call
  10 glDrawArrays calls (or 1 if batch rendering)

Without a shared atlas:
  10 glBindTexture calls (expensive GPU state changes)
  10 glDrawArrays calls
```

The trade-off is complexity. With a shared atlas, UV calculations need an additional offset for each character's region within the atlas. The metadata becomes more complex. For a pixel art game with under 20 on-screen characters, individual textures are fine.

### Instanced Rendering

When many entities use the **same animation at the same frame** (e.g., a field of identical flowers swaying, or a crowd of NPCs idle-ing), instanced rendering draws them all in a single draw call.

```
Without instancing:
  for each flower:
    set model matrix uniform
    glDrawArrays(GL_TRIANGLES, 0, 6)
  // 100 flowers = 100 draw calls

With instancing:
  upload 100 model matrices to a buffer
  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 100)
  // 100 flowers = 1 draw call

  The vertex shader uses gl_InstanceID to look up the correct
  model matrix from the buffer for each instance.
```

This is only beneficial when you have many (50+) identical sprites. For unique characters with different animations, instancing does not help because each needs different UV coordinates.

### Animation LOD

LOD (Level of Detail) reduces animation quality for entities that are far away or very small on screen, saving CPU time for animation updates.

```
Animation LOD levels:

  LOD 0 (close to camera, full detail):
    Update animation every frame
    8 frames per animation
    Smooth timing

  LOD 1 (medium distance):
    Update animation every 2nd frame (halve update frequency)
    8 frames but only checks every other game frame
    Slightly choppier but not noticeable at distance

  LOD 2 (far from camera, minimal detail):
    Update animation every 4th frame
    OR use a simpler animation (2 frames instead of 8)
    At this distance, the character is very small on screen

  LOD 3 (very far / mostly off-screen):
    Stop animating entirely
    Show a static frame
    Saves all animation CPU cost for this entity

Implementation:
  float distanceToCamera = distance(entity.position, camera.position);

  if (distanceToCamera < 200) {
      entity.animator.update(deltaTime);                // Full update
  } else if (distanceToCamera < 500) {
      entity.lodAccumulator += deltaTime;
      if (entity.lodAccumulator >= 0.033f) {            // ~30fps animation
          entity.animator.update(entity.lodAccumulator);
          entity.lodAccumulator = 0.0f;
      }
  } else {
      // Do not update animation at all
  }
```

For a Stardew Valley-style game, you are unlikely to need animation LOD. The number of on-screen characters is small, and the camera zoom level is relatively fixed. But for open-world games with hundreds of visible NPCs, it becomes essential.

### Pre-Computed UV Tables

Instead of recalculating UV coordinates every frame, compute them once and store them in a table.

```
// At initialization, compute UVs for all frames in the sprite sheet
struct FrameUV {
    float offsetX, offsetY;    // UV offset (top-left corner of frame)
};

// For a grid of 8 columns x 6 rows:
std::vector<std::vector<FrameUV>> uvTable;  // uvTable[row][col]

void precomputeUVs(int cols, int rows, float uvW, float uvH) {
    uvTable.resize(rows);
    for (int r = 0; r < rows; r++) {
        uvTable[r].resize(cols);
        for (int c = 0; c < cols; c++) {
            uvTable[r][c].offsetX = c * uvW;
            uvTable[r][c].offsetY = r * uvH;
        }
    }
}

// During rendering, just look up instead of calculating:
const FrameUV& uv = uvTable[animator.getFrameRow()][animator.getFrameColumn()];
shader.setVec2("uvOffset", uv.offsetX, uv.offsetY);

// The multiplication (col * uvWidth) is eliminated. This is a micro-optimization
// that only matters if you have thousands of animated sprites. For a few dozen,
// the two multiplications per frame are negligible.
```

### Off-Screen Culling

Do not update or render animations for entities that are completely outside the camera's view.

```
Camera viewport (what is visible on screen):

┌─────────────────────────────────────┐
│                                     │
│    Visible area                     │
│                                     │
│         ○ NPC_A (render & animate)  │
│                                     │
│    ○ Player (always render)         │
│                                     │
└─────────────────────────────────────┘

    ○ NPC_B (off-screen: DO NOT render, OPTIONALLY stop animating)

    ○ NPC_C (way off-screen: definitely stop animating)

Implementation:
  bool isOnScreen(Vector2 entityPos, Vector2 entitySize,
                  const Camera& camera) {
      float camLeft   = camera.getPosition().x;
      float camTop    = camera.getPosition().y;
      float camRight  = camLeft + camera.getViewportWidth();
      float camBottom = camTop + camera.getViewportHeight();

      float entRight  = entityPos.x + entitySize.x;
      float entBottom = entityPos.y + entitySize.y;

      // AABB overlap test
      return !(entityPos.x > camRight  || entRight  < camLeft ||
               entityPos.y > camBottom || entBottom < camTop);
  }

  // In the game loop:
  for (auto& entity : entities) {
      if (isOnScreen(entity.getPosition(), entity.getSize(), camera)) {
          entity.update(deltaTime);   // animate
          entity.render(shader, camera);  // draw
      }
      // Off-screen entities are completely skipped
  }
```

Note: you may still want to update off-screen entities' **game logic** (movement, AI) even if you skip their animation. An NPC walking toward the player should still move even when off-screen, so they appear at the correct position when the player scrolls to them. But their animation does not need to advance -- nobody can see it.

---

## 8. Real-World Examples

### Celeste -- Tight, Responsive, State-Driven

Celeste (2018) by Maddy Thorson and Noel Berry is a masterclass in responsive game animation. Here is how it handles sprite animation:

```
Celeste's animation design principles:

1. RESPONSIVENESS OVER SMOOTHNESS
   When the player presses jump, the jump animation starts IMMEDIATELY.
   There is zero anticipation -- no wind-up frames. The character is
   in the air on the very first frame. This sacrifices visual realism
   for gameplay feel.

   Compare to a fighting game where attack animations have "startup frames"
   that create a deliberate delay. Celeste prioritizes instant response.

2. ANIMATION FOLLOWS PHYSICS, NOT THE OTHER WAY AROUND
   The character's position is determined by physics (gravity, velocity,
   collisions). The animation REACTS to the physics state:
     - velocity.y < 0 -> play "rising" animation
     - velocity.y > 0 -> play "falling" animation
     - grounded && velocity.x != 0 -> play "running" animation
     - grounded && velocity.x == 0 -> play "idle" animation

   The animation does not drive movement. Movement drives animation.
   This is the opposite of games like Uncharted where the animation
   drives the character's position (root motion).

3. COYOTE TIME AND VISUAL FEEDBACK
   The character can jump for a few frames after walking off a ledge
   ("coyote time"). During these frames, the animation shows the
   character standing on nothing -- which is technically wrong but
   feels right because it matches what the player intended.

4. SQUASH AND STRETCH (procedural)
   When landing, the sprite is squashed (compressed vertically,
   stretched horizontally) for 2-3 frames. When jumping, it is
   stretched (elongated vertically, compressed horizontally).
   This is done procedurally by modifying the model matrix scale,
   NOT by drawing squashed/stretched frames in the sprite sheet.

5. HAIR PHYSICS (procedural)
   Madeline's hair is a chain of 5-6 colored circles that follow
   her head position with spring physics. Each circle follows the
   one before it with a slight delay. The hair color indicates dash
   status (red = dash available, blue = dash used). This is entirely
   procedural -- no pre-drawn hair frames.
```

### Shovel Knight -- Classic Sprite Sheet Organization

Shovel Knight (2014) by Yacht Club Games is a deliberate homage to NES-era design with modern quality-of-life improvements.

```
Shovel Knight's sprite sheet approach:

1. NES-INSPIRED CONSTRAINTS
   The team limited themselves to an NES-like palette and resolution
   (but did not strictly follow NES hardware limits). Characters are
   small (roughly 24x24 to 32x32 pixels) with limited frame counts.

2. EXPRESSIVE WITH FEW FRAMES
   Shovel Knight's walk cycle is 4 frames. His attack is 3-4 frames.
   But each frame is carefully crafted to convey maximum motion:
     - Strong silhouette changes between frames
     - Anticipation and follow-through are crammed into 1-2 frames
     - The "smear" technique: in fast actions, one frame shows the
       character in an impossible pose that reads as motion blur

3. SPRITE SHEET ORGANIZATION
   Each character's animations are on a single sheet. Different
   animation states occupy different regions of the sheet. The
   game uses a data file to define frame rectangles and timing.

4. PALETTE SWAPPING FOR ENEMY VARIANTS
   True to NES tradition, Shovel Knight uses palette swapping for
   enemy color variants. A green knight and a red knight share the
   same sprite data but use different color lookup tables.

5. ANIMATION-DRIVEN HITBOXES
   Attack hitboxes are defined per-frame. Frame 0 of the downward
   shovel pogo has no hitbox. Frame 1 (the active frame) has a
   hitbox at the shovel tip. Frame 2 (recovery) has no hitbox.
   The animation data IS the gameplay data.
```

### Undertale -- Minimal Frames, Maximum Expression

Undertale (2015) by Toby Fox demonstrates that animation quantity is far less important than animation quality and context.

```
Undertale's animation philosophy:

1. EXTREMELY MINIMAL FRAME COUNTS
   Most characters have only 2-4 frames of animation.
   Walk cycles are often just 2 frames (left foot, right foot).
   Idle animations are 1-2 frames (static or very subtle breathing).

   This works because:
     a. The low resolution (small sprites) means subtle motion is invisible
     b. The game's emotional impact comes from context, not fluidity
     c. Players fill in motion with their imagination

2. EXPRESSIVE THROUGH CONTEXT, NOT TECHNIQUE
   Sans (a skeleton character) has almost no animation at all.
   His "animation" is mostly positional -- he teleports around,
   appears at different places on screen, and the text and music
   create the emotional weight. The sprite barely moves.

   This teaches an important lesson: animation serves the game.
   You do not need fluid animation if the game's other systems
   (dialogue, music, pacing) carry the emotional load.

3. BATTLE SPRITES VS. OVERWORLD SPRITES
   Overworld sprites are tiny and simple (2-frame walk cycles).
   Battle sprites are larger and more detailed, but still use
   few frames -- often just a static image with procedural
   effects (shaking, fading, color flashing).

4. SPRITE SHEETS ARE SIMPLE GRIDS
   Each character's sheet is a small, tightly packed grid.
   No complex metadata. No variable frame sizes.
   The engine reads frames left-to-right, top-to-bottom.
```

### Dead Cells -- 3D-to-2D Pipeline

Dead Cells (2018) by Motion Twin uses a unique pipeline that produces exceptionally fluid pixel art animation.

```
Dead Cells' animation pipeline:

1. THE HYBRID APPROACH
   a. Artists create a 3D model of each character
   b. Animators animate the 3D model with skeletal animation
      (smooth, professional-quality motion capture or hand-animated)
   c. The 3D animation is rendered to a 2D sprite sheet
      (orthographic camera, low resolution, limited palette)
   d. Artists hand-paint OVER the rendered frames in pixel art style
      (cleanup artifacts, add pixel-art details, ensure consistency)

   Result: Motion quality of 3D skeletal animation combined with
   the visual style of hand-drawn pixel art.

2. HIGH FRAME COUNTS
   Because the 3D-to-2D pipeline automates much of the frame creation,
   Dead Cells characters have many more animation frames than typical
   pixel art games:
     - Walk cycles: 8-12 frames (vs. 4 in most pixel art games)
     - Attack animations: 10-20 frames (vs. 3-6 in most pixel art games)
     - This creates exceptionally fluid motion

3. TRADE-OFFS
   - Requires 3D modeling and animation skills (not just pixel art)
   - The paintover step is still labor-intensive
   - Results in larger sprite sheets (more frames = more VRAM)
   - But the final quality is widely considered the best in the genre

4. RELEVANCE TO YOUR PROJECT
   You probably will not use this pipeline (it requires 3D tools and
   skills). But it is worth knowing that Dead Cells' fluidity comes
   from having MORE frames, not better technique. If your animations
   feel stiff, adding 2-4 more frames to each cycle (especially
   in-between frames for transitions) will have the largest impact.
```

---

## Summary: Connecting Everything Together

Frame-by-frame sprite animation is built on concepts you already have working in your engine. The core insight is:

```
Tilemap rendering:       tileID (static)     -> UV offset -> draw
Sprite animation:        frameIndex (dynamic) -> UV offset -> draw
```

The rendering is identical -- your tile vertex shader with `uvOffset` and `uvSize` uniforms already does everything needed. The new work is entirely on the CPU side:

1. **Animation data** defines which frames belong to which animation (struct with row, frame count, durations)
2. **Animator** tracks playback state (accumulator pattern with delta time, current frame, events)
3. **State machine** decides which animation to play (FSM with transitions, priorities, interrupt rules)
4. **Direction handling** selects the correct directional variant (last-facing direction, horizontal flip)

Each of these is a self-contained concept. You can implement them incrementally:
- Start with a hardcoded frame cycle (just the accumulator pattern)
- Add multiple named animations (the Animation struct and Animator class)
- Add directional animations (facing direction + animation name lookup)
- Add the state machine (transition rules between idle, walk, attack)
- Add events (damage on specific attack frames)
- Add optimizations (UV tables, culling) only if needed

This progression mirrors how game engines are built in practice: get the simplest version working, then layer on complexity as needed.
