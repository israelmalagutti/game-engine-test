# Layered Composite Sprite Animation -- Deep Dive

This document is an exhaustive exploration of layered composite sprite rendering: the technique that makes character customization possible in 2D games without drowning in art assets. It covers the theory, the math, the GPU pipeline, the shader code, and real-world case studies from Stardew Valley, Terraria, and Octopath Traveler 2. Every concept is explained from scratch with web development analogies where helpful.

## Table of Contents

- [1. The Problem: Combinatorial Explosion](#1-the-problem-combinatorial-explosion)
- [2. Layer Architecture](#2-layer-architecture)
- [3. Registration and Alignment](#3-registration-and-alignment)
- [4. Color Tinting System -- Complete Theory](#4-color-tinting-system----complete-theory)
- [5. Rendering Pipeline for Layered Characters](#5-rendering-pipeline-for-layered-characters)
- [6. Render-to-Texture Optimization (FBO Compositing)](#6-render-to-texture-optimization-fbo-compositing)
- [7. Advanced: Layer Effects](#7-advanced-layer-effects)
- [8. Stardew Valley Deep Analysis](#8-stardew-valley-deep-analysis)
- [9. Terraria's Approach (Comparison)](#9-terrarias-approach-comparison)
- [10. Implementation Patterns](#10-implementation-patterns)
- [Glossary](#glossary)

---

## 1. The Problem: Combinatorial Explosion

### The Core Issue

You want players to customize their character. Hair, shirt, pants, hat, accessories. Each category has multiple options. Each option might come in multiple colors. The naive approach is to pre-render every possible combination as a complete sprite sheet. This sounds reasonable until you do the arithmetic.

### The Math: Multiplication vs. Addition

Suppose your character creator offers these choices:

```
Category          Options
------------------------------
Body type:             3
Skin tone:             6
Hairstyle:            20
Hair color:           12
Shirt:                15
Shirt color:          10
Pants:                12
Pant color:            8
Hat:                  10  (including "no hat")
Accessory:             8  (including "none")
```

With monolithic (pre-baked) sprite sheets, the number of unique sheets you need is the **product** of all options:

```
Monolithic approach (multiplication):

3 x 6 x 20 x 12 x 15 x 10 x 12 x 8 x 10 x 8

=  3 x 6          =       18
x 20              =      360
x 12              =    4,320
x 15              =   64,800
x 10              =  648,000
x 12              = 7,776,000
x  8              = 62,208,000
x 10              = 622,080,000
x  8              = 4,976,640,000

Result: ~5 BILLION unique sprite sheets.
```

Each sprite sheet might have 64 frames (4 directions x 16 animation frames). At 32x48 pixels per frame, that is a 128x768 pixel sheet. Even at 8-bit color, each sheet is roughly 400 KB. Five billion of them would require about 2 exabytes of storage. That is more data than every hard drive on Earth combined. Clearly impossible.

With the layered approach, the number of assets you need is the **sum** of all options:

```
Layered approach (addition):

3 + 6 + 20 + 12 + 15 + 10 + 12 + 8 + 10 + 8 = 104 sprite sheets
```

But wait -- many of those "options" are just color variations. Hair comes in 20 styles but 12 colors. If you draw each style in neutral gray and tint at runtime, you only need 20 hair sheets, not 20 x 12 = 240. The same logic applies to shirts, pants, and so on.

```
With runtime color tinting:

  3 body sheets      (skin tones baked in -- too complex to tint)
  6 skin tone sheets (actually, body type x skin tone = 18 sheets)
 20 hair sheets      (drawn in grayscale, tinted at runtime)
 15 shirt sheets     (drawn in grayscale, tinted at runtime)
 12 pant sheets      (drawn in grayscale, tinted at runtime)
 10 hat sheets       (some tinted, some pre-colored)
  8 accessory sheets (pre-colored, small count)

Total: 18 + 20 + 15 + 12 + 10 + 8 = 83 sprite sheets

Plus a handful of vec3 color values (RGB, essentially zero memory)
```

From **5 billion** to **83**. That is what decomposition buys you.

### The Web Development Analogy

This is the same problem web developers face with CSS utility classes vs. pre-built component variants:

```
Pre-built (monolithic):
  .btn-small-blue-rounded-outlined    { ... }
  .btn-small-blue-rounded-filled      { ... }
  .btn-small-blue-square-outlined     { ... }
  .btn-small-blue-square-filled       { ... }
  .btn-small-red-rounded-outlined     { ... }
  ... (hundreds of classes)

Composable (layered):
  .btn         { base styles }
  .btn-small   { size }
  .btn-blue    { color }
  .btn-rounded { shape }
  .btn-outlined { variant }

  Usage: <button class="btn btn-small btn-blue btn-rounded btn-outlined">
```

Tailwind CSS is popular precisely because it solves the combinatorial explosion of styling. Layered sprites solve the same explosion for art assets.

### Real Game Numbers

**Stardew Valley** (actual data from decompiled assets):

```
Character customization options:
  Skin tones:      6
  Hairstyles:     73   (56 base + community additions)
  Hair colors:     8   (via palette, more via mods)
  Shirts:        112
  Shirt colors:    variable (RGB tint)
  Pants:           variable (base pants, tinted)
  Pant colors:     variable (RGB tint)
  Accessories:    20   (beards, glasses, etc.)
  Hats:          94

Monolithic: 6 x 73 x 8 x 112 x 20 x 94 = ~5.8 billion combinations
Layered:    6 + 73 + 112 + 20 + 94 = 305 sheets (plus color values)
```

Without layering, Stardew Valley would be physically impossible to ship. ConcernedApe (the sole developer) would need to hand-draw billions of sprite sheets. The layered approach made the entire game feasible for a single artist-programmer.

### Why This Matters for Indie Developers

The art budget is almost always the bottleneck for indie games. A solo developer or small team cannot produce thousands of sprite sheets. Layered compositing gives you:

1. **Additive content creation**: Adding a new hat requires drawing ONE new hat sheet, not re-drawing every character combination with that hat
2. **Parallelizable work**: One artist draws hairstyles while another draws shirts -- no coordination needed beyond frame layout
3. **Player ownership**: Players feel attached to a character they designed themselves, increasing engagement
4. **Mod support for free**: Modders can add new layers without touching existing assets

### The "Paper Doll" Metaphor

The technique is sometimes called "paper doll" animation because it works exactly like physical paper dolls:

```
Physical paper doll:

  Base doll (body):     Clothing cutouts:     Assembled:
  ┌──────────┐         ┌──────────┐          ┌──────────┐
  │   O      │         │          │          │   O      │
  │  /|\     │    +    │  [SHIRT] │    =     │  [SHIRT] │
  │  / \     │         │  [PANTS] │          │  [PANTS] │
  └──────────┘         └──────────┘          └──────────┘

  The cutouts (layers) are placed ON TOP of the base.
  Transparent areas let the layer below show through.
  You can swap any cutout independently.
```

In rendering terms, "placing on top" means drawing a textured quad at the same screen position with alpha blending enabled, so transparent pixels in the upper layer reveal the lower layer.

---

## 2. Layer Architecture

### What Constitutes a "Layer"

A layer is a **separate sprite sheet** that contains one isolated visual component of a character, drawn with the exact same frame layout as every other layer. The non-relevant areas of each layer are fully transparent (alpha = 0).

```
Body layer (frame 0):        Hair layer (frame 0):       Shirt layer (frame 0):
┌──────────────────┐        ┌──────────────────┐        ┌──────────────────┐
│                  │        │     ########     │        │                  │
│      ######      │        │   ############   │        │                  │
│     ########     │        │   ############   │        │                  │
│     ########     │        │    ##########    │        │                  │
│      ######      │        │                  │        │     ┌──────┐    │
│     ########     │        │                  │        │     │FABRIC│    │
│    ##########    │        │                  │        │     │FABRIC│    │
│     ##    ##     │        │                  │        │     └──────┘    │
│     ##    ##     │        │                  │        │                  │
│    ####  ####    │        │                  │        │                  │
└──────────────────┘        └──────────────────┘        └──────────────────┘

Key observations:
  - All three sheets have IDENTICAL pixel dimensions per frame
  - Each layer only contains its own pixels; everything else is transparent
  - When overlaid, transparent areas let lower layers show through
  - All three represent the SAME pose (frame 0 = same body position)
```

Each layer has several properties:

```
Layer definition:
  texture:      Texture*     -- which sprite sheet to use
  tintColor:    vec4         -- RGBA tint applied in the shader (white = no tint)
  visible:      bool         -- whether to draw this layer at all
  depthOrder:   int          -- back-to-front draw priority
  category:     enum         -- BASE, EQUIPMENT, ACCESSORY, etc.
```

### Layer Ordering: Which Layers Go On Top

The fundamental rule of 2D rendering is **painter's algorithm**: draw back-to-front. Things drawn later cover things drawn earlier. This means layer order directly determines visual overlap.

```
Typical layer order (bottom to top):

  Draw order    Layer          Notes
  ─────────────────────────────────────────────────
  1 (bottom)    Body           Always present, base figure
  2             Pants          Covers lower body
  3             Shirt          Covers upper body
  4             Back arm       Behind the torso for some poses
  5             Front arm      In front of the torso
  6             Hair           On top of head/body
  7             Accessory      Beard, glasses, etc.
  8 (top)       Hat            Topmost layer

  Visual stack (side view):

       Hat         ──── 8 (drawn last, on top)
       Hair        ──── 7
       Accessory   ──── 6
       Front arm   ──── 5
       Shirt       ──── 4
       Back arm    ──── 3
       Pants       ──── 2
       Body        ──── 1 (drawn first, on bottom)
```

### Dynamic Layer Ordering: The Arm Problem

Here is where things get complicated. Layer order is NOT always fixed. Consider a character facing different directions:

```
Facing DOWN (toward camera):          Facing RIGHT (profile):

       ┌───┐                                ┌───┐
       │ O │  head                           │ O ├──
       ├───┤                                 ├───┤  \  right arm
  left │   │ right                           │   │   | (in FRONT)
  arm ─┤   ├─ arm                            │   │──/
       │   │                            ──── │   │
       ├───┤                           /     ├───┤
       │   │                     left │      │   │
       └───┘                     arm  │      └───┘
                                (BEHIND body)

  Both arms should be in FRONT        Left arm is BEHIND body,
  of the torso.                        right arm is in FRONT.
```

When the character faces down, both arms are drawn after (in front of) the torso. When the character faces right, the left arm needs to be behind the torso and the right arm in front. This means **layer order must change per animation frame or per direction**.

This is exactly the problem Stardew Valley solves by splitting the arms into a separate layer from the body. The arm layer's draw order changes based on the current animation frame.

### Per-Frame Layer Order Metadata

The cleanest solution is to store layer order information as metadata per animation frame:

```
Animation "walk_right":
  Frame 0: { body: 2, back_arm: 1, front_arm: 3, shirt: 2.5 }
  Frame 1: { body: 2, back_arm: 1, front_arm: 3, shirt: 2.5 }
  Frame 2: { body: 2, back_arm: 1, front_arm: 3, shirt: 2.5 }
  Frame 3: { body: 2, back_arm: 1, front_arm: 3, shirt: 2.5 }

Animation "walk_down":
  Frame 0: { body: 1, back_arm: 2, front_arm: 2, shirt: 1.5 }
  Frame 1: { body: 1, back_arm: 2, front_arm: 2, shirt: 1.5 }
  ...

The number is the draw order. Lower = drawn first (behind).
Layers with the same draw order are drawn in their default sequence.
```

An alternative approach is to simply duplicate the body into "body behind arms" and "body in front of arms" sub-layers, switching which is visible per direction. Stardew Valley uses the simpler approach: the arm is just one layer whose draw order relative to the torso flips based on direction.

### Layer Visibility: Toggle On/Off

Not all layers are always active. A player without a hat should not render the hat layer at all. This is a simple boolean toggle:

```
Layer visibility examples:

  Player with full equipment:         Player with no hat or accessory:
  ┌─ Hat ────────── visible ──┐       ┌─ Hat ────────── HIDDEN ──┐
  ├─ Accessory ──── visible ──┤       ├─ Accessory ──── HIDDEN ──┤
  ├─ Hair ───────── visible ──┤       ├─ Hair ───────── visible ──┤
  ├─ Shirt ──────── visible ──┤       ├─ Shirt ──────── visible ──┤
  ├─ Pants ──────── visible ──┤       ├─ Pants ──────── visible ──┤
  └─ Body ───────── visible ──┘       └─ Body ───────── visible ──┘

  Draw calls: 6 layers                Draw calls: 4 layers (2 skipped)
```

In web terms, this is exactly `display: none` vs. `display: block` on stacked `<div>` elements. Hidden layers produce zero draw calls.

### Layer Categories

Layers fall into two categories that determine their behavior:

```
BASE LAYERS (always present):
  - Body        -- the character's physical form
  - Skin        -- may be baked into body or separate for tinting

  These layers are NEVER hidden. Without them, there is no character.
  Think of them as the <html> and <body> tags -- always present.

EQUIPMENT LAYERS (conditionally present):
  - Hair        -- always visible but swappable
  - Shirt       -- may or may not be equipped
  - Pants       -- may or may not be equipped
  - Hat         -- optional
  - Accessory   -- optional
  - Shoes       -- optional
  - Cape/Cloak  -- optional
  - Weapon      -- optional (may be a separate entity, not a layer)

  These layers appear/disappear based on game state (inventory, equipment).
  Think of them as conditionally rendered React components.
```

The distinction matters for your data model. Base layers are guaranteed non-null, so you never need null checks. Equipment layers might be absent, requiring an `if (layer.visible)` guard before drawing.

---

## 3. Registration and Alignment

### Why All Layers MUST Have Identical Frame Dimensions

This is the single most important constraint in layered sprite systems. Every layer must use the same pixel dimensions per frame and the same grid layout. If the body sheet uses 32x48 pixel frames arranged in 4 columns and 8 rows, then the hair sheet, shirt sheet, pants sheet, and every other layer must also use 32x48 pixel frames in 4 columns and 8 rows.

```
CORRECT: All layers share the same frame grid

Body sheet (32x48 per frame, 4 cols x 4 rows):
┌────┬────┬────┬────┐
│ B0 │ B1 │ B2 │ B3 │  Walk Down
├────┼────┼────┼────┤
│ B4 │ B5 │ B6 │ B7 │  Walk Left
├────┼────┼────┼────┤
│ B8 │ B9 │B10 │B11 │  Walk Right
├────┼────┼────┼────┤
│B12 │B13 │B14 │B15 │  Walk Up
└────┴────┴────┴────┘

Hair sheet (32x48 per frame, 4 cols x 4 rows):
┌────┬────┬────┬────┐
│ H0 │ H1 │ H2 │ H3 │  Walk Down   <-- same animation in same position
├────┼────┼────┼────┤
│ H4 │ H5 │ H6 │ H7 │  Walk Left
├────┼────┼────┼────┤
│ H8 │ H9 │H10 │H11 │  Walk Right
├────┼────┼────┼────┤
│H12 │H13 │H14 │H15 │  Walk Up
└────┴────┴────┴────┘

Frame B5 (body, walk left, frame 1) and frame H5 (hair, walk left, frame 1)
represent the SAME pose. When both are drawn at the same screen position with
the same UV offset, the hair aligns perfectly on top of the body.


INCORRECT: Layers with different frame dimensions

Body sheet (32x48):    Hair sheet (24x32):     <-- WRONG DIMENSIONS
┌────┬────┬────┐       ┌───┬───┬───┐
│    │    │    │       │   │   │   │          Hair frames are smaller.
│    │    │    │       └───┴───┴───┘          Even if you scale them up,
│    │    │    │                               the alignment will be off.
└────┴────┴────┘                               Nothing will line up.
```

The reason is mathematical: the UV calculation for selecting a frame depends on frame dimensions and grid layout. If all layers share the same layout, they all share the same UV calculation. Frame N in the body sheet occupies the same UV rectangle as frame N in the hair sheet. You compute the UV offset once and apply it to every layer.

### Anchor Points and Pivot Points

"Alignment" does not always mean "top-left corner." Different layers may need different reference points for positioning. An anchor point (or pivot point) defines where on the sprite the position coordinate refers to.

```
Top-left anchor (default):           Bottom-center anchor (common for characters):

(x,y) ──> ┌──────┐                            ┌──────┐
           │      │                            │      │
           │      │                            │      │
           │      │                            │  ◎   │  <-- visual center
           └──────┘                            └──┬───┘
                                                  │
                                               (x,y) is HERE (bottom-center)
```

For layered sprites, the anchor point must be consistent across all layers. If the body uses bottom-center as its anchor and the hair uses top-left, they will not align even with identical frame dimensions. In practice, since all layers have identical frame dimensions and are rendered at the same position with the same UV offset, the anchor point is implicitly "the entire frame rectangle." Alignment comes from the art, not from code-level anchor offsets.

However, anchor points become relevant when:
- You want the character's "foot position" to be the world coordinate (useful for Y-sorting)
- Different character sizes need to align to the same ground plane
- A weapon or held item has a different pivot than the character body

### The Artist Workflow

The practical workflow for creating aligned layers uses a single master file with separate art layers:

```
Aseprite / Photoshop workflow:

1. Create a single file at the full sheet dimensions (e.g., 128 x 192 px)

2. Set up the frame grid (4 columns x 4 rows of 32x48 frames)

3. Create art layers within this file:
   ┌──────────────────────────────────────────┐
   │  Layer panel:                            │
   │                                          │
   │  [eye] Hat layer         (art layer)     │
   │  [eye] Hair layer        (art layer)     │
   │  [eye] Accessory layer   (art layer)     │
   │  [eye] Shirt layer       (art layer)     │
   │  [eye] Pants layer       (art layer)     │
   │  [eye] Body layer        (art layer)     │
   │  [ ] Grid guide          (reference)     │
   └──────────────────────────────────────────┘

4. Draw all layers in the same file. The artist can see how
   layers overlap and adjust each frame accordingly.

5. Export each art layer as a separate PNG:
   - body.png   (only body layer visible)
   - pants.png  (only pants layer visible)
   - shirt.png  (only shirt layer visible)
   - hair_01.png (only hair layer visible, style 1)
   - etc.

Because all layers share the same canvas, pixel alignment is guaranteed.
```

This is analogous to how web designers work in Figma: all elements on the same frame, then export individual assets. The shared coordinate system ensures consistency.

**Aseprite-specific tip:** Aseprite supports "layers as separate files" export. You animate in a single file with all layers visible (so you can see alignment), then use File > Export Sprite Sheet with the "Split Layers" option to generate one sheet per layer automatically.

### Common Alignment Bugs and Debugging

Even with the correct workflow, alignment issues are the most common bugs in layered sprite systems. Here are the usual culprits:

```
Bug 1: Off-by-one pixel offset

  Symptom: Hair "jitters" relative to body during animation.

  Body frame 2:        Hair frame 2:
  ┌──────────┐        ┌──────────┐
  │          │        │          │
  │   ████   │        │  ▓▓▓▓   │  <-- hair is 1 pixel to the LEFT
  │  ██████  │        │ ▓▓▓▓▓▓  │      of where it should be
  │  ██████  │        │          │
  └──────────┘        └──────────┘

  Cause: The artist nudged the hair layer by 1 pixel when editing frame 2
         but not the other frames. Or the export cropped whitespace.

  Fix: Re-export from the master file. Ensure "trim" or "auto-crop" is OFF
       during export. Every frame must include the full canvas.


Bug 2: Frame dimensions mismatch

  Symptom: Layers slide apart as animation progresses.

  Cause: One layer was exported at 30x46 per frame instead of 32x48.
         The UV math produces different coordinates, causing progressive
         misalignment across frames.

  Fix: Verify all exported sheets have identical pixel dimensions.
       A script that checks image dimensions is valuable here.


Bug 3: Different frame counts

  Symptom: Hair "loops" at wrong time, showing frame 0 hair over
           frame 4 body.

  Cause: The body has 6 frames per animation row, the hair has 4.
         When the body reaches frame 5, the hair wraps to frame 1.

  Fix: All layers MUST have the same number of frames per animation.
       Pad shorter layers with duplicate frames if needed.


Bug 4: Export with auto-trim

  Symptom: Layers are misaligned in inconsistent, seemingly random ways.

  Cause: The export tool trimmed transparent pixels around each frame,
         changing the effective canvas offset.

  Fix: Always export with fixed frame size, no trimming. In Aseprite,
       ensure "Trim" is unchecked in the export dialog.
```

### Handling Different Body Types

A hard question: if you have 3 body types (small, medium, large), do equipment layers need to be drawn 3 times?

```
Option A: One equipment set per body type

  Small body + small shirt sheet
  Medium body + medium shirt sheet
  Large body + large shirt sheet

  Art cost: 3x shirt sheets (one per body type)
  Quality: Perfect fit for every body type
  Used by: Terraria (different armor renders per body type gender)


Option B: One equipment set stretched to fit

  Small body + shared shirt sheet (scaled down)
  Medium body + shared shirt sheet (normal size)
  Large body + shared shirt sheet (scaled up)

  Art cost: 1x shirt sheets
  Quality: Stretching distorts pixel art badly
  Used by: Almost nobody for pixel art (acceptable for high-res art)


Option C: One equipment set designed for the "middle" body type

  Small body + medium shirt sheet (some clipping)
  Medium body + medium shirt sheet (perfect fit)
  Large body + medium shirt sheet (some gaps)

  Art cost: 1x shirt sheets
  Quality: Acceptable if body type differences are small
  Used by: Many indie games where body variation is subtle


Option D: Equipment layers with transparency-aware design

  Design equipment layers with extra margin so they cover
  all body types. The equipment is slightly oversized for
  small bodies and slightly undersized for large bodies,
  but transparent edges absorb the difference.

  Art cost: 1x shirt sheets (designed carefully)
  Quality: Good compromise
  Used by: Stardew Valley (body type variation is minimal)
```

For your HD-2D Stardew Valley clone, Option C or D is the pragmatic choice. Stardew Valley itself uses essentially one body type with minor skin tone variation, avoiding this problem entirely.

---

## 4. Color Tinting System -- Complete Theory

Color tinting is how you get infinite color variation from a single grayscale sprite sheet. This section covers every tinting method, from the simplest to the most sophisticated, with complete shader code for each.

### Multiplicative Tinting: The Foundation

Multiplicative tinting multiplies each pixel's color by a uniform tint color. This is the simplest, most common, and most efficient approach.

**The math:**

```
Given:
  texColor  = the pixel's color sampled from the texture  (vec4)
  tintColor = the desired tint passed as a uniform        (vec4)

Result = texColor * tintColor  (component-wise multiplication)

Example with a grayscale texture and blue tint:

  texColor = (0.8, 0.8, 0.8, 1.0)   -- light gray pixel
  tintColor = (0.3, 0.5, 1.0, 1.0)  -- blue tint

  result.r = 0.8 * 0.3 = 0.24
  result.g = 0.8 * 0.5 = 0.40
  result.b = 0.8 * 1.0 = 0.80
  result.a = 1.0 * 1.0 = 1.0

  Result = (0.24, 0.40, 0.80, 1.0) -- a blue pixel!
```

**Why it preserves shading:**

```
Grayscale texture with shading:

  Highlight:  0.95  (nearly white)
  Midtone:    0.60  (medium gray)
  Shadow:     0.25  (dark gray)

After multiplying by red tint (1.0, 0.2, 0.2):

  Highlight:  (0.95, 0.19, 0.19)  -- bright red
  Midtone:    (0.60, 0.12, 0.12)  -- medium red
  Shadow:     (0.25, 0.05, 0.05)  -- dark red

The RATIO between highlight, midtone, and shadow is preserved:
  0.95 : 0.60 : 0.25  (original grayscale ratios)
  0.95 : 0.60 : 0.25  (red channel ratios after tinting)

This is why the shading "survives" tinting -- multiplication
preserves relative brightness relationships.
```

Visualized:

```
Before tint (grayscale):           After tint (red):

  ░░████████░░                      ░░▓▓▓▓▓▓▓▓░░
  ░████████████░                    ░▓▓▓▓▓▓▓▓▓▓▓▓░
  ██████████████                    ▓▓▓▓▓▓▓▓▓▓▓▓▓▓
  ██████████████                    ▓▓▓▓▓▓▓▓▓▓▓▓▓▓
  ░████████████░                    ░▓▓▓▓▓▓▓▓▓▓▓▓░
  ░░████████░░                      ░░▓▓▓▓▓▓▓▓░░

  The shape and shading are identical.
  Only the hue changed.
```

**Complete fragment shader for multiplicative tinting:**

```glsl
#version 330 core

// Input from vertex shader
in vec2 TexCoord;

// Output color
out vec4 FragColor;

// The sprite sheet texture for this layer
uniform sampler2D spriteTexture;

// Tint color: set to (1.0, 1.0, 1.0, 1.0) for no tint
uniform vec4 u_tintColor;

// UV offset to select the current animation frame
uniform vec2 u_uvOffset;

// UV size of one frame (1.0/columns, 1.0/rows)
uniform vec2 u_uvSize;

void main() {
    // Calculate the UV coordinates for the current frame
    // TexCoord is 0-1 across the full quad; we remap it to the frame's sub-region
    vec2 frameUV = u_uvOffset + TexCoord * u_uvSize;

    // Sample the texture at the frame's UV
    vec4 texel = texture(spriteTexture, frameUV);

    // Discard fully transparent pixels so they don't write to the framebuffer
    // This prevents transparent areas from covering layers below
    if (texel.a < 0.01) {
        discard;
    }

    // Apply multiplicative tint: multiply RGB channels, preserve alpha
    FragColor = vec4(texel.rgb * u_tintColor.rgb, texel.a * u_tintColor.a);
}
```

### Why Tintable Layers Should Be Grayscale

Multiplicative tinting can only **darken**. It can never make a pixel brighter than its original value, because multiplying by any value between 0.0 and 1.0 reduces the result.

```
Multiplication can only darken:

  0.5 * 1.0 = 0.5   (tint is pure white, no change)
  0.5 * 0.8 = 0.4   (darker)
  0.5 * 0.5 = 0.25  (much darker)
  0.5 * 0.0 = 0.0   (black)

  There is no tint value that makes 0.5 become 0.7.
  This means dark textures cannot become bright after tinting.
```

The consequence: if your shirt texture is drawn in medium gray (0.5), the brightest possible tinted result is 0.5 (with a white tint). You lose half your dynamic range. To maximize tinting range, draw tintable layers in the **lightest possible grayscale** -- near white for highlights, medium gray for midtones, dark gray for deep shadows.

```
BAD: Shirt drawn in mid-range grays          GOOD: Shirt drawn in near-white grays

  Highlight: 0.6                               Highlight: 0.95
  Midtone:   0.4                               Midtone:   0.65
  Shadow:    0.2                               Shadow:    0.30

  Tinted blue (0.3, 0.5, 1.0):                Tinted blue (0.3, 0.5, 1.0):
  Highlight: (0.18, 0.30, 0.60)               Highlight: (0.285, 0.475, 0.95)
  Midtone:   (0.12, 0.20, 0.40)               Midtone:   (0.195, 0.325, 0.65)
  Shadow:    (0.06, 0.10, 0.20)               Shadow:    (0.09, 0.15, 0.30)

  Left looks muddy and dark.                   Right looks vibrant.
  The lighter the source, the more
  range tinting has to work with.
```

### The Limitation: Multiplication Can Only Darken

This is worth emphasizing because it determines when you should NOT use multiplicative tinting:

```
Cases where multiplicative tinting fails:

1. You want a bright yellow glow on dark armor
   Dark armor texture: (0.2, 0.2, 0.2)
   Yellow tint: (1.0, 1.0, 0.0)
   Result: (0.2, 0.2, 0.0) -- dark brownish, NOT a bright yellow glow

2. You want to recolor a pre-drawn colored sprite
   Red sprite pixel: (0.8, 0.1, 0.1)
   Blue tint: (0.1, 0.1, 0.8)
   Result: (0.08, 0.01, 0.08) -- nearly black, NOT blue
   The red and blue channels fight each other.

3. You want to add a white highlight to a dark texture
   Impossible with multiplication alone.
```

For these cases, you need additive tinting, HSV shifting, or full palette swapping.

### Additive Tinting: For Glow Effects

Additive tinting **adds** a color to the texture instead of multiplying. This brightens pixels, which is useful for glow, hit flash, and magic effects.

```
Additive math:
  result = texColor + tintColor

  texColor = (0.2, 0.2, 0.2)    -- dark pixel
  tintColor = (0.5, 0.3, 0.0)   -- warm glow

  result = (0.7, 0.5, 0.2)      -- brightened with warm tint!

  Unlike multiplication, this CAN make pixels brighter.
  But it can also overflow past 1.0 (clamped by the GPU).
```

```glsl
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform vec4 u_tintColor;
uniform vec2 u_uvOffset;
uniform vec2 u_uvSize;

// 0.0 = pure multiplicative, 1.0 = pure additive
uniform float u_additiveBlend;

void main() {
    vec2 frameUV = u_uvOffset + TexCoord * u_uvSize;
    vec4 texel = texture(spriteTexture, frameUV);

    if (texel.a < 0.01) {
        discard;
    }

    // Blend between multiplicative and additive tinting
    vec3 multiplicative = texel.rgb * u_tintColor.rgb;
    vec3 additive = texel.rgb + u_tintColor.rgb;

    vec3 tinted = mix(multiplicative, additive, u_additiveBlend);

    FragColor = vec4(tinted, texel.a);
}
```

The `u_additiveBlend` uniform lets you smoothly transition between multiplicative (normal tinting) and additive (glow) at runtime. Set it to 0.0 for normal rendering, pulse it toward 1.0 when the character takes damage for a "hit flash" effect.

### HSV/HSL Color Shifting in Shaders

For maximum control over color, you can convert the texture color to HSV (Hue, Saturation, Value) space, shift the hue, and convert back. This lets you change a red shirt to a blue shirt without affecting brightness or saturation -- something multiplicative tinting cannot do cleanly with pre-colored art.

```
HSV color model:

  H (Hue):        0-360 degrees around the color wheel
                   0=red, 60=yellow, 120=green, 180=cyan, 240=blue, 300=magenta

  S (Saturation):  0.0=gray, 1.0=full color

  V (Value):       0.0=black, 1.0=full brightness

  Shifting hue by +120 degrees:
    Red (0) --> Green (120)
    Blue (240) --> Red (360 = 0)

  This preserves shading (Value) and intensity (Saturation).
```

```glsl
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform vec2 u_uvOffset;
uniform vec2 u_uvSize;

// HSV adjustments
uniform float u_hueShift;          // degrees to rotate hue (0-360)
uniform float u_saturationScale;   // multiply saturation (1.0 = no change)
uniform float u_valueScale;        // multiply value/brightness (1.0 = no change)

// Convert RGB to HSV
vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;  // tiny epsilon to avoid division by zero
    return vec3(
        abs(q.z + (q.w - q.y) / (6.0 * d + e)),   // H (0-1)
        d / (q.x + e),                               // S (0-1)
        q.x                                          // V (0-1)
    );
}

// Convert HSV back to RGB
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec2 frameUV = u_uvOffset + TexCoord * u_uvSize;
    vec4 texel = texture(spriteTexture, frameUV);

    if (texel.a < 0.01) {
        discard;
    }

    // Convert to HSV
    vec3 hsv = rgb2hsv(texel.rgb);

    // Apply hue shift (u_hueShift is in degrees, convert to 0-1 range)
    hsv.x = fract(hsv.x + u_hueShift / 360.0);

    // Apply saturation and value scaling
    hsv.y = clamp(hsv.y * u_saturationScale, 0.0, 1.0);
    hsv.z = clamp(hsv.z * u_valueScale, 0.0, 1.0);

    // Convert back to RGB
    vec3 shifted = hsv2rgb(hsv);

    FragColor = vec4(shifted, texel.a);
}
```

HSV shifting is more expensive than multiplicative tinting (more ALU operations in the shader), but it gives you the ability to recolor pre-drawn art without the "darkening" limitation of multiplication. It is particularly useful when your artist draws equipment in a reference color (say, red) and you want to offer other colors at runtime.

### How Stardew Valley Applies Tinting

Stardew Valley uses a hybrid approach:

```
Stardew Valley tinting by layer:

  Body:       No tint. Multiple body sheets per skin tone.
              Skin has complex shading (blush, undertones) that
              multiplicative tinting would destroy.

  Hair:       Multiplicative tint. Hair is drawn in grayscale.
              Player picks a hair color. Darker hair pixels
              (shadows) stay dark relative to highlights.

  Shirt:      Multiplicative tint on the "sleeves" area.
              The shirt texture has two regions:
              - A tintable fabric area (drawn in light gray)
              - A non-tintable overlay area (pattern, buttons)
              Stardew uses a mask or separate texture regions
              to control which pixels receive tinting.

  Pants:      Multiplicative tint. Pants are drawn in light
              gray, tinted to the player's chosen color.

  Hat:        No tint. Each hat is pre-colored.
              (Hats have unique visual designs that tinting
              would ruin.)

  Accessory:  No tint. Beards and glasses are pre-colored.
```

The key insight: Stardew uses tinting selectively. Layers that benefit from simple color shifts (hair, clothing) get tinted. Layers that require artistic detail (skin, hats, accessories) are pre-drawn in their final colors.

### Tint vs. Replace (Palette Swap): When to Use Each

```
MULTIPLICATIVE TINT:
  - Best for: Single-hue recoloring (hair, simple clothing)
  - How:      Multiply RGB by uniform color
  - Cost:     1 multiply in shader (nearly free)
  - Limit:    Can only darken, one-hue shift
  - Example:  Gray shirt --> blue shirt

PALETTE SWAP (CLUT):
  - Best for: Multi-hue recoloring (skin with undertones, patterned armor)
  - How:      Look up each pixel's color in a palette texture
  - Cost:     1 extra texture sample in shader (slightly more expensive)
  - Limit:    Requires indexed texture and palette texture
  - Example:  Fire armor (red+orange+yellow) --> ice armor (blue+cyan+white)

HSV SHIFT:
  - Best for: Recoloring pre-drawn colored art
  - How:      Convert to HSV, rotate hue, convert back
  - Cost:     ~15 ALU operations in shader (moderate)
  - Limit:    Shifts ALL hues by the same amount (cannot target specific colors)
  - Example:  Red dress with gold trim --> blue dress with gold trim
              (only if gold and red are distinguishable in HSV space)
```

For your HD-2D Stardew Valley clone, multiplicative tinting covers 90% of your needs. Use palette swapping for skin tones or complex recoloring, and HSV shifting only if you need to recolor pre-colored art from external sources (mod support, user-submitted content).

### Gamma-Correct Tinting vs. Naive Tinting

Most 2D games ignore gamma correction entirely. But understanding it helps you diagnose color issues that look "wrong" but are hard to pinpoint.

```
The problem:

  Monitors do not display brightness linearly. A pixel value of 0.5
  does NOT produce half the brightness of 1.0. Monitors apply a gamma
  curve (typically gamma 2.2):

  displayed_brightness = pixel_value ^ 2.2

  So:
    pixel_value = 0.5  -->  0.5^2.2 = 0.217 (only 21.7% brightness!)
    pixel_value = 1.0  -->  1.0^2.2 = 1.0   (100% brightness)

  Textures stored on disk are in "sRGB" space (gamma-encoded).
  Math operations (like tinting) should happen in LINEAR space.
```

```
Naive tinting (what most 2D games do):

  1. Sample texture in sRGB space   --> (0.5, 0.5, 0.5)
  2. Multiply by tint               --> (0.5, 0.5, 0.5) * (0.3, 0.5, 1.0)
  3. Result in sRGB space           --> (0.15, 0.25, 0.50)
  4. Output to screen               --> monitor applies gamma

  This is "wrong" because the multiplication happened in a non-linear
  space. The result is slightly different from what it would be if you
  had converted to linear first.


Gamma-correct tinting:

  1. Sample texture in sRGB space   --> (0.5, 0.5, 0.5)
  2. Convert to linear space        --> (0.5^2.2) = (0.217, 0.217, 0.217)
  3. Convert tint to linear space   --> (0.3^2.2, 0.5^2.2, 1.0^2.2)
                                        = (0.073, 0.217, 1.0)
  4. Multiply in linear space       --> (0.016, 0.047, 0.217)
  5. Convert result back to sRGB    --> (0.016^(1/2.2), 0.047^(1/2.2), 0.217^(1/2.2))
                                        = (0.148, 0.242, 0.497)
  6. Output to screen

  The difference is subtle but visible, especially in dark tones.
```

In OpenGL, you can enable automatic sRGB conversion:

```cpp
// Tell OpenGL the texture is in sRGB space
// OpenGL will convert to linear automatically when sampling
glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0,
             GL_RGBA, GL_UNSIGNED_BYTE, data);

// Tell OpenGL to convert back to sRGB when writing to the framebuffer
glEnable(GL_FRAMEBUFFER_SRGB);
```

**Practical advice:** For a pixel art game, naive tinting is fine. Gamma issues are most noticeable in smooth gradients and photorealistic rendering. Pixel art's limited palette and sharp edges hide the difference. Many professional 2D games ship without gamma correction and look great. But now you know why two identically-valued colors sometimes look "off" when blended.

---

## 5. Rendering Pipeline for Layered Characters

### Draw Call Order: Back-to-Front

Each character is rendered as a sequence of layered draw calls, from the furthest-back layer to the frontmost:

```
For ONE character at position (x, y), showing frame N:

  Step 1: Compute frame N's UV offset (shared by all layers)
          uvOffset = computeFrameUV(animState.currentFrame)

  Step 2: For each layer in back-to-front order:
          if (layer.visible):
              a. Bind layer's texture
              b. Set u_uvOffset = uvOffset
              c. Set u_uvSize = (1.0 / cols, 1.0 / rows)
              d. Set u_tintColor = layer.tintColor
              e. Set model matrix (position, size -- same for all layers)
              f. glDrawArrays(GL_TRIANGLES, 0, 6)

Pseudocode:
  for layer in character.layers.sortedByDepth():
      if not layer.visible:
          continue
      layer.texture.bind(0)
      shader.setVec2("u_uvOffset", uvOffset)
      shader.setVec2("u_uvSize", uvSize)
      shader.setVec4("u_tintColor", layer.tintColor)
      shader.setMat4("model", modelMatrix)
      glDrawArrays(GL_TRIANGLES, 0, 6)
```

### Alpha Blending Setup

Alpha blending is what makes transparent pixels in upper layers reveal lower layers. Without it, every pixel of every layer would be opaque, and only the topmost layer would be visible.

```
Alpha blending equation:

  finalColor = srcColor * srcFactor + dstColor * dstFactor

  Where:
    srcColor  = the pixel being drawn (from the current layer)
    dstColor  = the pixel already in the framebuffer (from previous layers)
    srcFactor = how much of the new pixel to use
    dstFactor = how much of the existing pixel to keep

  For standard transparency:
    srcFactor = srcAlpha         (GL_SRC_ALPHA)
    dstFactor = 1 - srcAlpha     (GL_ONE_MINUS_SRC_ALPHA)

  So:
    If srcAlpha = 1.0 (opaque pixel):
      finalColor = srcColor * 1.0 + dstColor * 0.0 = srcColor
      (Fully replaces what was there)

    If srcAlpha = 0.0 (fully transparent pixel):
      finalColor = srcColor * 0.0 + dstColor * 1.0 = dstColor
      (Keeps what was there, ignores new pixel)

    If srcAlpha = 0.5 (semi-transparent):
      finalColor = srcColor * 0.5 + dstColor * 0.5
      (50/50 blend)
```

OpenGL setup for alpha blending:

```cpp
// Enable alpha blending -- this must be done once during initialization
glEnable(GL_BLEND);

// Set the standard transparency blending function
// This is the most common blending mode for 2D sprite rendering
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

**Why order matters with alpha blending:**

```
CORRECT order (back to front):

  1. Draw body (opaque)     --> framebuffer shows body
  2. Draw shirt (opaque)    --> shirt pixels replace body pixels where shirt exists
  3. Draw cape (50% alpha)  --> cape blends with body+shirt underneath
  4. Draw hair (opaque)     --> hair covers everything below

  Result: Cape correctly shows body/shirt through it.


WRONG order (front to back):

  1. Draw hair (opaque)     --> framebuffer shows hair
  2. Draw cape (50% alpha)  --> cape blends with hair AND with the black background
                                 where body should be (but body isn't drawn yet!)
  3. Draw shirt (opaque)    --> shirt appears ON TOP of cape (wrong!)
  4. Draw body (opaque)     --> body appears ON TOP of everything (wrong!)

  Result: Complete visual mess.
```

This is the same concept as CSS `z-index` stacking: elements must be layered in the correct order for transparency to work correctly.

### Depth Testing Considerations

In 3D rendering, depth testing (z-buffer) automatically handles which objects are in front. In 2D layered sprites, depth testing is usually **disabled**:

```
Why depth testing is typically disabled for 2D sprite layers:

  1. All layers are at the same Z depth (0.0). They're the same character
     at the same world position. Depth testing would cause Z-fighting
     (flickering as the GPU can't decide which pixel is "in front").

  2. We WANT painter's algorithm (draw order determines overlap).
     Depth testing would interfere by rejecting pixels that "should"
     be covered but have the same Z value.

  3. Semi-transparent layers would not blend correctly with depth
     testing, because the depth buffer doesn't account for transparency.
```

```cpp
// Typically, for 2D sprite rendering:
glDisable(GL_DEPTH_TEST);

// If you DO use depth testing for Y-sorting between different characters,
// disable depth WRITING while drawing layers within a single character:
glEnable(GL_DEPTH_TEST);
glDepthMask(GL_FALSE);    // Don't write to depth buffer
// ... draw all layers for one character ...
glDepthMask(GL_TRUE);     // Re-enable for next character
```

### How to Handle Semi-Transparent Layers

Some equipment should be partially see-through: a gossamer cape, a ghost effect, a magic aura. This uses the alpha channel of the tint color:

```
Normal layer (opaque tint):
  u_tintColor = (1.0, 1.0, 1.0, 1.0)
  Every opaque pixel in the texture is drawn at full opacity.

Semi-transparent layer (cape at 60% opacity):
  u_tintColor = (1.0, 1.0, 1.0, 0.6)
  Every opaque pixel in the texture is drawn at 60% opacity.
  The body and shirt underneath are partially visible through the cape.

Fading out (death animation):
  u_tintColor.a = lerp(1.0, 0.0, deathProgress)
  The entire character fades from solid to invisible over time.
```

The fragment shader already handles this because `FragColor.a = texel.a * u_tintColor.a` reduces opacity when the tint alpha is less than 1.0.

### Outline Rendering: Around the Entire Character

A common visual effect is drawing an outline around the character -- a 1-2 pixel colored border. The tricky part with layered sprites: the outline should go around the **composited silhouette**, not around each individual layer.

```
WRONG (outline per layer):              RIGHT (outline around composite):

  ┌─ outline ────────────┐               ┌─ outline ──────────┐
  │ ┌─ hair outline ───┐ │               │ ┌────────────────┐ │
  │ │     ######       │ │               │ │     ######     │ │
  │ └──────────────────┘ │               │ │    ########    │ │
  │ ┌─ shirt outline ──┐ │               │ │    ##SHIRT#    │ │
  │ │    ##SHIRT##     │ │               │ │    ##SHIRT#    │ │
  │ └──────────────────┘ │               │ │    ########    │ │
  │ ┌─ body outline ───┐ │               │ │     ## ##      │ │
  │ │    ########      │ │               │ └────────────────┘ │
  │ └──────────────────┘ │               └────────────────────┘
  └──────────────────────┘
                                          ONE outline around the entire
  Internal outlines visible               character silhouette. Clean.
  between layers. Messy.
```

The solution is to use the FBO compositing technique (covered in Section 6): render all layers to an off-screen texture first, then apply an outline shader to the composited result.

```glsl
// Outline shader -- applied to the composited character texture
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D compositeTexture;
uniform vec2 u_texelSize;       // (1.0 / textureWidth, 1.0 / textureHeight)
uniform vec4 u_outlineColor;    // e.g., (0.0, 0.0, 0.0, 1.0) for black outline
uniform float u_outlineWidth;   // in texels, typically 1.0 or 2.0

void main() {
    vec4 center = texture(compositeTexture, TexCoord);

    // If this pixel is already opaque, draw it normally
    if (center.a > 0.5) {
        FragColor = center;
        return;
    }

    // This pixel is transparent. Check if any neighbor is opaque.
    // If so, this pixel is on the EDGE of the silhouette -- draw outline.
    float maxNeighborAlpha = 0.0;

    // Sample in a cross pattern (up, down, left, right)
    // For thicker outlines, expand the sampling area
    for (float x = -u_outlineWidth; x <= u_outlineWidth; x += 1.0) {
        for (float y = -u_outlineWidth; y <= u_outlineWidth; y += 1.0) {
            vec2 offset = vec2(x, y) * u_texelSize;
            float neighborAlpha = texture(compositeTexture, TexCoord + offset).a;
            maxNeighborAlpha = max(maxNeighborAlpha, neighborAlpha);
        }
    }

    // If a neighbor is opaque, draw the outline color here
    if (maxNeighborAlpha > 0.5) {
        FragColor = u_outlineColor;
    } else {
        // No opaque neighbor, this pixel stays transparent
        FragColor = vec4(0.0);
    }
}
```

---

## 6. Render-to-Texture Optimization (FBO Compositing)

### The Problem: Too Many Draw Calls

Each visible layer is one draw call. A character with 6 visible layers produces 6 draw calls. With 30 characters on screen, that is 180 draw calls just for character rendering.

```
Draw call cost estimate:

  6 layers/character x 30 characters = 180 draw calls

  Each draw call involves:
    - glBindTexture          (change texture)
    - shader uniform updates (tint, UV offset)
    - glDrawArrays           (GPU command)

  On modern hardware (2020+), 180 draw calls is negligible.
  On older hardware or mobile, it may matter.
  At 100+ characters (crowds), it definitely matters.
```

For comparison, your tilemap likely renders in 1-2 draw calls (one per tilemap layer, using batch rendering). Characters using 180 draw calls for 30 sprites looks disproportionate.

### The Solution: Framebuffer Object (FBO) Compositing

The idea: instead of drawing 6 layers to the screen each frame, **pre-compose** all layers into a single texture. Then draw that one texture to the screen each frame -- just 1 draw call per character.

```
Without FBO compositing:                With FBO compositing:

  EVERY FRAME:                           ON EQUIPMENT CHANGE (rare):
  1. Bind body texture                   1. Bind FBO (off-screen render target)
  2. Draw body quad                      2. Clear FBO to transparent black
  3. Bind pants texture                  3. Draw body layer to FBO
  4. Draw pants quad                     4. Draw pants layer to FBO
  5. Bind shirt texture                  5. Draw shirt layer to FBO
  6. Draw shirt quad                     6. Draw hair layer to FBO
  7. Bind hair texture                   7. Unbind FBO
  8. Draw hair quad
  = 4 draw calls per character           EVERY FRAME:
    per frame                            1. Bind pre-composited texture
                                         2. Draw single quad
                                         = 1 draw call per character per frame
```

The re-bake only happens when the character's appearance changes (equipping a new item, changing hair color). During normal gameplay (walking, fighting, talking), the composited texture is reused. Since equipment changes are infrequent (maybe once every few minutes), you amortize the compositing cost over thousands of frames.

### Complete FBO Setup Walkthrough

Here is a complete, step-by-step OpenGL FBO setup for compositing character layers. Every call is explained:

```cpp
// Step 1: Create the FBO
GLuint compositeFBO;
glGenFramebuffers(1, &compositeFBO);
// This generates a framebuffer object "name" (an integer ID).
// It does not allocate any memory yet -- it is just an empty container.

// Step 2: Create a texture to render into
GLuint compositeTexture;
glGenTextures(1, &compositeTexture);
glBindTexture(GL_TEXTURE_2D, compositeTexture);

// Allocate texture memory sized for the full sprite sheet
// If the character sprite sheet is 128x768 pixels, use those dimensions
int compositeWidth = 128;   // width of the full sprite sheet
int compositeHeight = 768;  // height of the full sprite sheet

glTexImage2D(
    GL_TEXTURE_2D,       // target
    0,                   // mipmap level (0 = base)
    GL_RGBA8,            // internal format (8 bits per channel, RGBA)
    compositeWidth,      // texture width
    compositeHeight,     // texture height
    0,                   // border (must be 0)
    GL_RGBA,             // pixel data format
    GL_UNSIGNED_BYTE,    // pixel data type
    nullptr              // no initial data -- we will render into this
);

// For pixel art: use NEAREST filtering (no blur)
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

// Step 3: Attach the texture to the FBO
glBindFramebuffer(GL_FRAMEBUFFER, compositeFBO);
glFramebufferTexture2D(
    GL_FRAMEBUFFER,           // target
    GL_COLOR_ATTACHMENT0,     // attachment point (color output 0)
    GL_TEXTURE_2D,            // texture target
    compositeTexture,         // texture to attach
    0                         // mipmap level
);

// Step 4: Verify the FBO is complete (all attachments are valid)
GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
if (status != GL_FRAMEBUFFER_COMPLETE) {
    // Error! The FBO is not set up correctly.
    // Common causes: wrong texture format, missing attachment, size mismatch
    printf("FBO incomplete! Status: 0x%X\n", status);
}

// Step 5: Unbind the FBO (go back to rendering to the screen)
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

### Compositing: Rendering Layers Into the FBO

When the character's appearance changes, you "bake" all layers into the composited texture:

```cpp
void CharacterComposite::bakeComposite(Shader& layerShader) {
    // Save the current viewport so we can restore it after
    GLint previousViewport[4];
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    // Bind the FBO -- all subsequent draws go to the composite texture
    glBindFramebuffer(GL_FRAMEBUFFER, compositeFBO);

    // Set the viewport to match the composite texture dimensions
    // This is critical: the viewport tells OpenGL the render target size
    glViewport(0, 0, compositeWidth, compositeHeight);

    // Clear to transparent black (R=0, G=0, B=0, A=0)
    // This ensures areas with no layers are fully transparent
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set up orthographic projection matching the sprite sheet pixel dimensions
    // This lets us draw layers using pixel coordinates
    glm::mat4 projection = glm::ortho(
        0.0f, static_cast<float>(compositeWidth),
        static_cast<float>(compositeHeight), 0.0f
    );

    // Identity view matrix (no camera offset -- we are rendering to the texture)
    glm::mat4 view = glm::mat4(1.0f);

    // Model matrix covers the entire composite texture
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(compositeWidth, compositeHeight, 1.0f));

    layerShader.use();
    layerShader.setMat4("projection", projection);
    layerShader.setMat4("view", view);
    layerShader.setMat4("model", model);

    // Draw each layer in order (back to front)
    // UV offset/size are (0,0) and (1,1) because we are rendering
    // the ENTIRE sprite sheet, not a single frame
    layerShader.setVec2("u_uvOffset", glm::vec2(0.0f, 0.0f));
    layerShader.setVec2("u_uvSize", glm::vec2(1.0f, 1.0f));

    for (const auto& layer : getSortedLayers()) {
        if (!layer.visible) {
            continue;
        }

        // Bind this layer's sprite sheet texture
        layer.texture->bind(0);
        layerShader.setInt("spriteTexture", 0);

        // Set this layer's tint color
        layerShader.setVec4("u_tintColor", layer.tintColor);

        // Draw the layer (a full-screen quad that maps the entire texture)
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    // Unbind the FBO -- go back to rendering to the screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Restore the previous viewport
    glViewport(previousViewport[0], previousViewport[1],
               previousViewport[2], previousViewport[3]);

    // Mark the composite as up-to-date
    isDirty = false;
}
```

After baking, the `compositeTexture` contains the fully composited sprite sheet. You can now treat it like any other sprite sheet -- index frames by UV offset and draw single quads.

### Memory Trade-offs

```
Memory cost analysis:

  Without FBO compositing:
    - N shared layer textures in VRAM (e.g., 87 textures)
    - Each texture is ~400 KB (128x768 px, RGBA8)
    - Total: 87 x 400 KB = ~35 MB
    - All characters share these textures

  With FBO compositing:
    - Same N shared layer textures PLUS
    - One composite texture per UNIQUE character appearance
    - If 30 characters on screen, each with unique appearance:
      30 x 400 KB = 12 MB extra
    - If many characters share appearances (NPCs):
      Fewer unique composites needed

  Trade-off:
    Memory: +12 MB
    Draw calls: -150 per frame (from 180 to 30)
    CPU time: Negligible (uniform updates are cheap, but still saved)
```

### When This Optimization Is Worth It vs. Premature

```
WORTH IT when:
  - You have 50+ characters on screen simultaneously (crowd scenes)
  - Your target is mobile hardware (limited draw call budget)
  - You need per-character outline rendering (requires composite)
  - You want to apply post-processing effects per character

NOT WORTH IT when:
  - Fewer than 20 characters on screen at once
  - You are targeting desktop hardware exclusively
  - Character appearances change very frequently (re-baking is expensive)
  - You are still prototyping (add complexity later)

For your HD-2D Stardew Valley clone:
  - Stardew typically has 5-15 characters on screen
  - Start WITHOUT FBO compositing (simpler code)
  - Add it later IF you need outlines or IF performance is a problem
  - The optimization is always available as a fallback
```

This follows the standard engineering advice: measure first, optimize second. In web terms, this is the difference between setting up server-side rendering for a blog vs. a high-traffic e-commerce site. The technique exists, but you should not reach for it until the simpler approach hits its limits.

---

## 7. Advanced: Layer Effects

### Per-Layer Shaders

Sometimes different layers need different visual effects. A shirt might have a metallic shimmer, hair might sway in the wind, or a magic cloak might glow. This requires binding a different shader program per layer.

```
Example: Per-layer shader assignment

  Layer      Shader           Effect
  ─────────────────────────────────────────────
  Body       standard.frag    Simple multiplicative tint
  Pants      standard.frag    Simple multiplicative tint
  Shirt      shimmer.frag     Metallic shimmer overlay (time-based sine wave)
  Hair       wind_sway.frag   Vertex displacement (wind effect)
  Hat        standard.frag    No tint
  Cape       dissolve.frag    Dissolve/transparency pattern
```

The rendering loop changes slightly:

```cpp
for (const auto& layer : getSortedLayers()) {
    if (!layer.visible) {
        continue;
    }

    // Each layer can have its own shader
    Shader& shader = layer.customShader ? *layer.customShader : defaultShader;
    shader.use();

    // Set common uniforms
    shader.setMat4("projection", projection);
    shader.setMat4("view", view);
    shader.setMat4("model", model);
    shader.setVec2("u_uvOffset", uvOffset);
    shader.setVec2("u_uvSize", uvSize);
    shader.setVec4("u_tintColor", layer.tintColor);

    // Set shader-specific uniforms
    if (layer.customShader) {
        shader.setFloat("u_time", totalTime);       // for animations
        shader.setFloat("u_intensity", layer.effectIntensity);
    }

    layer.texture->bind(0);
    shader.setInt("spriteTexture", 0);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
```

**Performance note:** Switching shader programs (`glUseProgram`) is one of the more expensive GPU state changes. If you have many characters with per-layer shaders, sort layers by shader program to minimize switches. In practice, most layers share the same shader, so the cost is usually negligible.

### Masks: Using One Layer to Clip Another

A common need: when a hat is equipped, the top part of the hair should be hidden (it is "under" the hat). You can achieve this with a mask texture that defines which pixels of the hair layer should be visible.

```
Without mask:                     With mask:

  ┌──────────────┐                ┌──────────────┐
  │  HAT HAT HAT │                │  HAT HAT HAT │
  │  HAIR HAIR   │                │              │   <-- hair hidden under hat!
  │  FACE FACE   │                │  FACE FACE   │
  │  BODY BODY   │                │  BODY BODY   │
  └──────────────┘                └──────────────┘

  Hair clips through the hat.      Mask covers the hair-under-hat area.
```

The mask is a grayscale texture where white = visible and black = hidden:

```glsl
// Fragment shader with mask support
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform sampler2D u_maskTexture;      // mask texture (white = show, black = hide)
uniform bool u_useMask;               // whether masking is active
uniform vec4 u_tintColor;
uniform vec2 u_uvOffset;
uniform vec2 u_uvSize;

void main() {
    vec2 frameUV = u_uvOffset + TexCoord * u_uvSize;
    vec4 texel = texture(spriteTexture, frameUV);

    if (texel.a < 0.01) {
        discard;
    }

    // Apply mask if active
    if (u_useMask) {
        float maskValue = texture(u_maskTexture, frameUV).r;  // sample mask at same UV
        if (maskValue < 0.5) {
            discard;  // this pixel is masked out (under the hat)
        }
    }

    FragColor = vec4(texel.rgb * u_tintColor.rgb, texel.a * u_tintColor.a);
}
```

Each hat style would have a corresponding mask texture that defines which area of the head it covers. The hair shader samples this mask and discards pixels in the masked region.

### Normal Maps Per Layer (For HD-2D Lighting)

This is where your project's HD-2D ambitions intersect with layered sprites. In Octopath Traveler 2's visual style, 2D sprites are lit by dynamic lights. This requires **normal maps** -- textures that encode surface direction (which way each pixel "faces") so the lighting shader knows how light interacts with the surface.

```
What a normal map encodes:

  A normal map stores a 3D direction vector at each pixel:
    R channel = X direction  (left/right surface tilt)
    G channel = Y direction  (up/down surface tilt)
    B channel = Z direction  (facing toward/away from camera)

  A flat, forward-facing surface has normal = (0.5, 0.5, 1.0) in texture space
  (which maps to the direction (0, 0, 1) in world space -- pointing toward the camera).


Normal map for a character body:

  Color texture:              Normal map:
  ┌──────────────┐            ┌──────────────┐
  │     @@@@     │            │   ┌──────┐   │
  │    @@@@@@    │            │   │ Blue │   │  <-- mostly blue (facing camera)
  │   @@@@@@@@   │            │  ┌┤      ├┐  │
  │    @@@@@@    │            │  │└──────┘│  │  <-- edges tilt left/right (R varies)
  │   @@@  @@@   │            │  │  ││    │  │
  │   @@@  @@@   │            │  └──┘└──┘ │  │
  └──────────────┘            └──────────────┘
```

For layered sprites, each layer has its own normal map. The lighting shader samples both the color texture and the normal map:

```glsl
// Fragment shader with per-layer normal map and point light
#version 330 core

in vec2 TexCoord;
in vec2 WorldPos;
out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform sampler2D u_normalMap;
uniform vec4 u_tintColor;
uniform vec2 u_uvOffset;
uniform vec2 u_uvSize;

// Lighting uniforms
uniform vec3 u_ambientLight;        // base illumination
uniform vec2 u_lightPos;            // world position of light
uniform vec3 u_lightColor;          // light RGB
uniform float u_lightRadius;        // falloff distance
uniform float u_lightIntensity;     // brightness multiplier

void main() {
    vec2 frameUV = u_uvOffset + TexCoord * u_uvSize;
    vec4 texel = texture(spriteTexture, frameUV);

    if (texel.a < 0.01) {
        discard;
    }

    // Sample normal map and convert from [0,1] to [-1,1] range
    vec3 normal = texture(u_normalMap, frameUV).rgb * 2.0 - 1.0;
    normal = normalize(normal);

    // Calculate light direction in 2D (with assumed Z component)
    vec3 lightDir = vec3(u_lightPos - WorldPos, 50.0);  // Z=50 assumed height
    lightDir = normalize(lightDir);

    // Diffuse lighting: how aligned is the surface with the light direction?
    float diffuse = max(dot(normal, lightDir), 0.0);

    // Distance attenuation
    float dist = distance(WorldPos, u_lightPos);
    float attenuation = 1.0 - smoothstep(0.0, u_lightRadius, dist);

    // Combine lighting
    vec3 lighting = u_ambientLight + u_lightColor * u_lightIntensity * diffuse * attenuation;

    // Apply tint and lighting
    vec3 tinted = texel.rgb * u_tintColor.rgb;
    FragColor = vec4(tinted * lighting, texel.a);
}
```

### Emission Maps Per Layer

An emission map defines which pixels glow (emit their own light, unaffected by scene lighting). This is perfect for glowing runes on armor, magical staffs, or bioluminescent creatures.

```
Emission map concept:

  Color texture:     Normal map:         Emission map:
  ┌──────────────┐  ┌──────────────┐    ┌──────────────┐
  │   ARMOR      │  │ surface dirs │    │              │
  │   ▓▓▓▓▓▓    │  │              │    │   ▓▓▓▓▓▓    │  <-- glowing runes!
  │   RUNES      │  │              │    │   BRIGHT!    │
  │   ▓▓▓▓▓▓    │  │              │    │   ▓▓▓▓▓▓    │
  └──────────────┘  └──────────────┘    └──────────────┘

  In the emission map:
    Black pixels (0,0,0) = no emission (affected by lighting normally)
    Colored pixels = emit that color regardless of scene lighting
```

The shader adds emission to the final color:

```glsl
uniform sampler2D u_emissionMap;

void main() {
    // ... (normal lighting calculation as above) ...

    vec3 emission = texture(u_emissionMap, frameUV).rgb;

    // Emission is ADDED to the lit color, not affected by scene lighting
    FragColor = vec4(tinted * lighting + emission, texel.a);
}
```

### How Octopath Traveler 2 Likely Handles Sprite Layering

Octopath Traveler 2 uses a 3D rendering engine with 2D sprites placed on 3D planes. Based on visual analysis and technical breakdowns:

```
Octopath Traveler 2 technical approach (inferred):

  1. Sprites are rendered as textured quads in a 3D scene
  2. Each sprite has a normal map for dynamic lighting
  3. The 3D scene provides real depth (not just layer ordering)
  4. Post-processing effects (bloom, DOF) work naturally because
     the sprites exist in 3D space with real Z values

  Key differences from a pure 2D approach:
  - Depth of field works because sprites have real Z positions
  - Lighting is calculated per-pixel using normal maps
  - Shadows are cast by 3D geometry (the sprite quad blocks light)
  - Particle effects interact with scene depth naturally

  For character sprites specifically:
  - Characters are likely single composited sprites (not layered)
    because NPCs don't have character customization
  - The protagonist may use minimal layering (weapon, status effects)
  - The HD-2D effect comes from the ENVIRONMENT, not from character layers
```

For your engine, the takeaway is: **HD-2D lighting and layered character sprites are orthogonal concerns.** You can implement both, but they solve different problems. HD-2D is about environmental atmosphere (lighting, DOF, bloom). Layered sprites are about character customization. They combine well -- each character layer can have normal/emission maps -- but one does not require the other.

---

## 8. Stardew Valley Deep Analysis

### Complete Breakdown of Player Character Rendering

Stardew Valley's player character is assembled from approximately 8 distinct layers, rendered in a specific order that changes per animation frame and direction. This information comes from decompiled game code and community modding documentation.

```
Stardew Valley farmer layer stack:

  Layer               Source sheet                  Tinted?    Notes
  ──────────────────────────────────────────────────────────────────────
  1. Body             farmer_base.xnb              No         Includes skin tone variants.
                      (or farmer_base_bald.xnb)               Largest sheet: ~96 frames.
                                                              Contains body, face, idle,
                                                              walking, tool use, etc.

  2. Back Arm         farmer_base.xnb (subregion)  No         Same texture as body, but
                                                              drawn from a different sprite
                                                              region. Rendered BEHIND the
                                                              torso for side-facing poses.

  3. Pants            pants.xnb                     Yes        Drawn in light gray.
                                                              Tinted to player's chosen
                                                              pant color (RGB uniform).

  4. Shirt            shirts.xnb                    Partial    The shirt sheet contains BOTH
                                                              a tintable region (fabric color)
                                                              and a non-tintable region
                                                              (pattern overlay, buttons).
                                                              Two draw calls per shirt frame.

  5. Front Arm        farmer_base.xnb (subregion)  No         Same texture as body.
                                                              Rendered IN FRONT of the shirt.
                                                              Arm position changes per frame.

  6. Hair             hairstyles.xnb               Yes        Multiple styles in one large
                                                              sheet (8 cols x many rows).
                                                              Tinted to player's hair color.

  7. Accessory        accessories.xnb              No         Beards, glasses, etc.
                                                              Pre-colored, no tinting.
                                                              Positioned to align with face.

  8. Hat              hats.xnb                     No         When equipped. Some hats
                                                              modify hair rendering (hide
                                                              top of hair, as described in
                                                              the masks section).
```

### The Arm Depth Problem in Detail

The most clever part of Stardew's system is the arm handling:

```
Facing DOWN (toward camera):

  Draw order: body -> pants -> shirt -> BOTH arms (in front) -> hair -> hat

  Why: When facing the camera, both arms are visible in front of the torso.
  The arms overlap the shirt sleeves.

  Visual:
     ┌─────┐
     │  :) │   head
     ├─────┤
   ──┤shirt├──   arms in FRONT of shirt
     │pants│
     ├─┤ ├─┤
     │ │ │ │   legs
     └─┘ └─┘


Facing RIGHT (profile):

  Draw order: BACK arm -> body -> pants -> shirt -> FRONT arm -> hair -> hat

  Why: The left arm (away from camera) must be BEHIND the body.
  The right arm (toward camera) must be IN FRONT of the shirt.

  Visual:
         ┌─────┐
     ──  │  :) │   head
    back ├─────┤──   right arm in FRONT
    arm  │shirt│
  (behind│pants│
   body) ├─┤ ├─┤
         │ │ │ │
         └─┘ └─┘


Facing UP (away from camera):

  Draw order: body -> pants -> shirt -> hair (large, covers back of head) ->
              BOTH arms (in front of body, behind hair for some frames) -> hat

  Why: When facing away, hair covers most of the head. Arms may go
  behind or in front depending on the animation frame (tool swing vs. idle).
```

Stardew handles this by splitting the body sprite into regions. The farmer's arm sprites are stored in the same texture as the body but at different sprite coordinates. The code explicitly draws the "back arm" and "front arm" at different points in the layer stack, changing which is which based on the facing direction.

### How Stardew Handles Farmer vs. NPC Rendering

```
Farmer (player character):
  - Fully layered: body + pants + shirt + hair + accessories + hat
  - Tintable layers for customization
  - Complex layer ordering per direction
  - ~96 animation frames (idle, walk, tool use, fishing, eating, etc.)
  - Equipment system swaps layer textures dynamically

NPCs:
  - MONOLITHIC sprites: each NPC is a single pre-drawn sprite sheet
  - No layering, no tinting, no customization
  - Much simpler rendering (1 draw call per NPC)
  - Each NPC has their own unique sheet (abigail.xnb, alex.xnb, etc.)
  - Fixed appearance (modders must replace the entire sheet to change looks)

  Why the difference?
  - Only the player character needs customization
  - NPCs have unique, hand-drawn appearances that define their personality
  - Layering NPCs would be wasted complexity (nobody changes Abigail's shirt)
  - Simpler NPC rendering is better for performance (more NPCs on screen)
```

This is a pragmatic engineering decision. The layered system is complex and only justified when customization is needed. When it is not needed, the simpler monolithic approach is strictly better.

### Character Portraits

```
Stardew Valley portrait system:

  Portraits are COMPLETELY SEPARATE from sprites.
  They are high-resolution (close-up face) images used in dialogue.

  Sprite:                Portrait:
  ┌────┐                ┌──────────────┐
  │ 16 │                │              │
  │ x  │                │   Detailed   │
  │ 32 │                │    face      │
  │ px │                │   drawing    │
  └────┘                │              │
                        └──────────────┘
  Tiny, pixelated        128x128 or larger

  Portraits have their OWN sprite sheet with emotional expressions:
  ┌──────┬──────┬──────┬──────┐
  │happy │ sad  │angry │ shy  │
  ├──────┼──────┼──────┼──────┤
  │laugh │blush │shock │ love │
  └──────┴──────┴──────┴──────┘

  The portrait system has NO connection to the sprite layering system.
  It does not use layers, tinting, or animation state.
  It is a simple "select a sub-image from a grid" system.
```

For the player character's portrait, Stardew dynamically generates it by compositing the player's chosen features (skin tone, hair style, hair color, accessories) at a higher resolution. This is a separate compositing system from the in-game sprite compositing.

### Limitations and Modder Frustrations

```
Known limitations of Stardew's approach:

1. Adding new body types is extremely difficult
   The arm positions are hardcoded pixel offsets relative to the body.
   A new body shape requires recalculating all arm offsets for all frames.
   Modders who attempt "taller farmer" or "wider farmer" mods must
   rewrite significant rendering code.

2. Shirt/pants alignment is fragile
   The shirt and pants regions are defined by pixel rectangles in the code.
   Modders adding new shirts must match the exact pixel boundaries.
   Off-by-one errors are common and hard to debug.

3. Hair under hats is imprecise
   Stardew uses a simple "hide Y pixels from the top of hair" approach
   rather than a per-hat mask. This means some hair/hat combinations
   look wrong (hair clips through the brim or disappears too aggressively).

4. No per-layer visual effects
   All layers use the same rendering -- no per-layer shaders.
   Modders cannot add glow to armor or shimmer to hair without
   modifying the game's rendering code.

5. Portrait generation is slow
   The portrait compositing happens on the CPU (not GPU) and is
   noticeable during character creation (slight lag when changing options).

These limitations are NOT design flaws -- they are reasonable trade-offs
for a solo developer shipping a game. But knowing them helps you design
a better system from the start.
```

---

## 9. Terraria's Approach (Comparison)

### How Terraria Handles Character Customization

Terraria takes a fundamentally different approach from Stardew Valley. While Stardew uses a fixed set of equipment types with color tinting, Terraria has hundreds of armor sets that each provide a completely different visual appearance.

```
Stardew Valley approach:                 Terraria approach:
─────────────────────────                ──────────────────
Base character + equipment layers        Base character + visual overrides
Equipment changes COLOR                  Equipment changes SHAPE + COLOR
Same silhouette, different hue           Completely different silhouette

  Player + blue shirt:                    Player + Meteor Armor:
  ┌──────────┐                           ┌──────────┐
  │   (O)    │                           │   {O}    │  <-- different helmet
  │  /▓▓\   │  (blue shirt,             │  /████\  │     (full armor,
  │ / ▓▓ \  │   same body shape)         │ / ████ \ │      different shape)
  │  /  \   │                           │  /====\  │
  └──────────┘                           └──────────┘
```

### Terraria's Armor Rendering System

```
Terraria layer structure:

  Layer          Description                    Override behavior
  ──────────────────────────────────────────────────────────────────
  1. Skin        Base character body            Always visible under armor gaps
  2. Undershirt  Default clothing               Hidden when chest armor equipped
  3. Pants       Default lower clothing         Hidden when leg armor equipped
  4. Shoes       Default footwear               Hidden when leg armor equipped (some)
  5. Head armor  Helmet/hat                     REPLACES head appearance entirely
  6. Body armor  Chest plate                    REPLACES torso appearance entirely
  7. Leg armor   Greaves/boots                  REPLACES leg appearance entirely
  8. Accessory   Wings, shields, etc.           ADDED on top (doesn't replace)

  When no armor is equipped:
    Player shows skin + undershirt + pants + shoes

  When full armor set is equipped:
    Player shows skin (barely visible) + head armor + body armor + leg armor
    The armor COMPLETELY replaces the clothing layers.
```

Terraria has separate sprite sheets for every armor piece in every animation frame. This is feasible because:

1. Terraria's character is simpler (fewer frames per animation, simpler poses)
2. Terraria has a huge modding community that contributes art
3. Armor pieces in Terraria are the game's primary progression system, so the investment in art is justified

### Terraria's Dye System

Terraria's dye system is a form of runtime color manipulation applied to specific equipment layers:

```
Dye system concept:

  Armor piece:  Iron Helmet (gray, pre-colored)
  Dye applied:  Red Dye

  Without dye:          With red dye:
  ┌──────────┐          ┌──────────┐
  │  ▓▓▓▓▓▓  │          │  ▒▒▒▒▒▒  │
  │ ▓▓▓▓▓▓▓▓ │   -->    │ ▒▒▒▒▒▒▒▒ │
  │  ▓▓  ▓▓  │          │  ▒▒  ▒▒  │
  └──────────┘          └──────────┘
   Gray metal            Red metal

How dyes work in Terraria:

  Basic dyes:    Simple hue shift (similar to HSV rotation)

  Gradient dyes: Apply a color gradient based on Y position
                 (top of armor is one color, bottom is another)

  Special dyes:  Complex shader effects:
                 - Flame dye: animated fire pattern overlay
                 - Rainbow dye: cycling hue over time
                 - Negative dye: invert colors
                 - Shadow dye: silhouette effect (all black with colored edge)
                 - Phase dye: semi-transparent with shimmer
```

Terraria's dye system is essentially a **per-layer shader swap** where each dye type corresponds to a different fragment shader (or a different branch within a single shader). This is the same concept as the per-layer shader system described in Section 7.

### Comparing the Two Approaches

```
                        Stardew Valley         Terraria
                        ──────────────         ────────
Customization focus:    Colors/styles          Equipment shapes
Layer count:            6-8 per character      5-8 per character
Tinting method:         Multiplicative RGB     HSV shift + special shaders
Equipment impact:       Adds layers on top     Replaces layers entirely
Art cost per item:      Low (tintable gray)    High (full colored sprite)
Visual variety:         Moderate (same shape)  High (different shapes)
Player identity:        "My character's look"  "My character's gear"
Mod complexity:         Low (add a PNG)        Medium (add a PNG + metadata)

Both approaches are valid. Choose based on your game's design:
  - If customization is about personal identity: Stardew approach
  - If customization is about progression/power: Terraria approach
  - If both: Combine them (tintable layers + full visual overrides for special items)
```

---

## 10. Implementation Patterns

### CharacterComposite Class Structure

Here is the complete conceptual architecture for a layered character composite system. This shows how the pieces fit together without being tied to a specific game's entity system.

```cpp
// ------- Layer.h -------
// Represents one visual layer of a composite character

#pragma once
#include "Texture.h"
#include <glm/vec4.hpp>
#include <string>

// Layer categories determine behavior and default draw order
enum class LayerCategory {
    BASE,           // Body, skin -- always present
    CLOTHING,       // Shirt, pants -- always present, tintable
    EQUIPMENT,      // Hat, armor -- conditionally present
    ACCESSORY,      // Beard, glasses -- conditionally present
    EFFECT          // Glow, aura -- special rendering
};

struct CharacterLayer {
    std::string name;               // "body", "shirt", "hair", etc.
    Texture* texture;               // Non-owning pointer to the sprite sheet
    glm::vec4 tintColor;            // RGBA tint (white = no tint)
    LayerCategory category;         // What type of layer this is
    int depthOrder;                 // Base draw order (lower = drawn first)
    bool visible;                   // Whether to render this layer

    CharacterLayer()
        : name(""),
          texture(nullptr),
          tintColor(1.0f, 1.0f, 1.0f, 1.0f),
          category(LayerCategory::BASE),
          depthOrder(0),
          visible(true)
    {}

    CharacterLayer(const std::string& name, Texture* tex, LayerCategory cat, int depth)
        : name(name),
          texture(tex),
          tintColor(1.0f, 1.0f, 1.0f, 1.0f),
          category(cat),
          depthOrder(depth),
          visible(true)
    {}
};
```

```cpp
// ------- CharacterComposite.h -------
// Manages a set of layers and renders them as a single composite character

#pragma once
#include "CharacterLayer.h"
#include "Shader.h"
#include "Camera.h"
#include "Vector2.h"
#include <vector>
#include <algorithm>

class CharacterComposite {
private:
    std::vector<CharacterLayer> layers;

    // Animation state (shared across ALL layers)
    int currentFrame;
    int currentRow;         // which animation row (direction/action)
    float elapsedTime;
    float frameDuration;
    int framesPerRow;       // how many columns in the sprite sheet
    int totalRows;          // how many rows in the sprite sheet

    // Frame dimensions in the sprite sheet
    int frameWidth;         // pixel width of one frame
    int frameHeight;        // pixel height of one frame
    int sheetWidth;         // pixel width of the full sheet
    int sheetHeight;        // pixel height of the full sheet

    // World position and rendering size
    Vector2 position;
    Vector2 renderSize;

    // Cached sorted layer list (re-sorted when depth order changes)
    std::vector<CharacterLayer*> sortedLayers;
    bool layerOrderDirty;

    void rebuildSortedLayers() {
        sortedLayers.clear();
        for (auto& layer : layers) {
            sortedLayers.push_back(&layer);
        }
        // Sort by depthOrder ascending (lowest = drawn first = behind)
        std::sort(sortedLayers.begin(), sortedLayers.end(),
            [](const CharacterLayer* a, const CharacterLayer* b) {
                return a->depthOrder < b->depthOrder;
            }
        );
        layerOrderDirty = false;
    }

public:
    CharacterComposite(int frameW, int frameH, int sheetW, int sheetH,
                       int framesPerRow, int totalRows)
        : currentFrame(0),
          currentRow(0),
          elapsedTime(0.0f),
          frameDuration(0.15f),
          framesPerRow(framesPerRow),
          totalRows(totalRows),
          frameWidth(frameW),
          frameHeight(frameH),
          sheetWidth(sheetW),
          sheetHeight(sheetH),
          position(0, 0),
          renderSize(static_cast<float>(frameW), static_cast<float>(frameH)),
          layerOrderDirty(true)
    {}

    // --- Layer management ---

    void addLayer(const CharacterLayer& layer) {
        layers.push_back(layer);
        layerOrderDirty = true;
    }

    // Find a layer by name and return a pointer (or nullptr)
    CharacterLayer* getLayer(const std::string& name) {
        for (auto& layer : layers) {
            if (layer.name == name) {
                return &layer;
            }
        }
        return nullptr;
    }

    // Change a layer's texture (e.g., equip new shirt)
    void setLayerTexture(const std::string& name, Texture* tex) {
        CharacterLayer* layer = getLayer(name);
        if (layer) {
            layer->texture = tex;
        }
    }

    // Change a layer's tint color
    void setLayerTint(const std::string& name, const glm::vec4& color) {
        CharacterLayer* layer = getLayer(name);
        if (layer) {
            layer->tintColor = color;
        }
    }

    // Show or hide a layer (e.g., equip/unequip hat)
    void setLayerVisible(const std::string& name, bool vis) {
        CharacterLayer* layer = getLayer(name);
        if (layer) {
            layer->visible = vis;
        }
    }

    // Change a layer's draw order (for direction-dependent ordering)
    void setLayerDepth(const std::string& name, int depth) {
        CharacterLayer* layer = getLayer(name);
        if (layer) {
            layer->depthOrder = depth;
            layerOrderDirty = true;
        }
    }

    // --- Animation ---

    void setAnimation(int row, int numFrames, float duration) {
        currentRow = row;
        framesPerRow = numFrames;
        frameDuration = duration;
        currentFrame = 0;
        elapsedTime = 0.0f;
    }

    void update(float deltaTime) {
        elapsedTime += deltaTime;

        if (elapsedTime >= frameDuration) {
            // Subtract (not reset) to preserve timing accuracy
            elapsedTime -= frameDuration;
            currentFrame = (currentFrame + 1) % framesPerRow;
        }
    }

    // --- Rendering ---

    void render(Shader& shader, const Camera& camera) {
        if (layerOrderDirty) {
            rebuildSortedLayers();
        }

        shader.use();

        // Build the projection matrix (orthographic, matching viewport)
        int viewportWidth = camera.getViewportWidth();
        int viewportHeight = camera.getViewportHeight();
        glm::mat4 projection = glm::ortho(
            0.0f, static_cast<float>(viewportWidth),
            static_cast<float>(viewportHeight), 0.0f
        );

        // Get the camera's view matrix
        glm::mat4 view = camera.getViewMatrix();

        // Build model matrix (position and scale for the character)
        glm::mat4 model = glm::translate(
            glm::mat4(1.0f),
            glm::vec3(position.x, position.y, 0.0f)
        );
        model = glm::scale(model, glm::vec3(renderSize.x, renderSize.y, 1.0f));

        // Set shared matrices
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        shader.setMat4("model", model);

        // Calculate UV offset and size for the current frame
        // This is the same UV math used in the tilemap system
        float uvW = static_cast<float>(frameWidth) / static_cast<float>(sheetWidth);
        float uvH = static_cast<float>(frameHeight) / static_cast<float>(sheetHeight);
        float uvX = static_cast<float>(currentFrame * frameWidth) / static_cast<float>(sheetWidth);
        float uvY = static_cast<float>(currentRow * frameHeight) / static_cast<float>(sheetHeight);

        shader.setVec2("u_uvOffset", glm::vec2(uvX, uvY));
        shader.setVec2("u_uvSize", glm::vec2(uvW, uvH));

        // Draw each visible layer in depth order
        for (const CharacterLayer* layer : sortedLayers) {
            if (!layer->visible || layer->texture == nullptr) {
                continue;
            }

            // Bind this layer's texture
            layer->texture->bind(0);
            shader.setInt("spriteTexture", 0);

            // Set this layer's tint color
            shader.setVec4("u_tintColor", layer->tintColor);

            // Draw the quad (6 vertices = 2 triangles = 1 quad)
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
        }
    }

    // --- Position ---

    void setPosition(const Vector2& pos) { position = pos; }
    void setRenderSize(const Vector2& sz) { renderSize = sz; }
    Vector2 getPosition() const { return position; }
};
```

### Integration with Animation State Machine

All layers must share the same animation state. When the character transitions from IDLE to WALK, every layer switches to the walk animation row simultaneously. The composite owns the animation state; layers do not have their own timers.

```
Animation state machine drives the composite:

  ┌───────────┐                          ┌───────────────────────┐
  │ Animation │ ─── currentRow ────────> │  CharacterComposite   │
  │   State   │ ─── framesPerRow ──────> │                       │
  │  Machine  │ ─── frameDuration ─────> │  All layers use the   │
  │           │                          │  same frame index.    │
  │ (IDLE,    │                          │                       │
  │  WALK,    │ ─── direction ─────────> │  Layer depth order    │
  │  ATTACK)  │     (changes depth       │  may change based     │
  │           │      order of arms)      │  on direction.        │
  └───────────┘                          └───────────────────────┘

  The state machine says: "play walk_right, row 2, 4 frames at 0.15s each"
  The composite says: "every layer now shows row 2, cycling frames 0-3"
```

```cpp
// Integration example: how the animation state machine updates the composite

void Player::update(float deltaTime) {
    // Update animation state machine based on input
    AnimationState newState = determineState(input);

    if (newState != currentState) {
        currentState = newState;

        // Tell the composite which animation to play
        switch (currentState) {
            case AnimationState::IDLE_DOWN:
                composite.setAnimation(0, 1, 0.5f);   // row 0, 1 frame, 0.5s
                composite.setLayerDepth("back_arm", 3);
                composite.setLayerDepth("front_arm", 3);
                break;

            case AnimationState::WALK_DOWN:
                composite.setAnimation(1, 4, 0.15f);   // row 1, 4 frames, 0.15s
                composite.setLayerDepth("back_arm", 3);
                composite.setLayerDepth("front_arm", 3);
                break;

            case AnimationState::WALK_RIGHT:
                composite.setAnimation(2, 4, 0.15f);   // row 2, 4 frames, 0.15s
                composite.setLayerDepth("back_arm", 0); // behind body
                composite.setLayerDepth("front_arm", 5); // in front of shirt
                break;

            case AnimationState::WALK_LEFT:
                composite.setAnimation(3, 4, 0.15f);   // row 3, 4 frames, 0.15s
                composite.setLayerDepth("back_arm", 0); // behind body
                composite.setLayerDepth("front_arm", 5); // in front of shirt
                break;

            case AnimationState::WALK_UP:
                composite.setAnimation(4, 4, 0.15f);   // row 4, 4 frames, 0.15s
                composite.setLayerDepth("back_arm", 3);
                composite.setLayerDepth("front_arm", 3);
                break;
        }
    }

    // Advance the animation timer
    composite.update(deltaTime);

    // Move the character position
    composite.setPosition(Vector2(worldX, worldY));
}
```

### Equipment System Interaction

When the player equips or unequips an item, the composite's layers are updated:

```cpp
// Equipment system modifies the composite's layers

void Equipment::equipItem(const Item& item, CharacterComposite& composite) {
    switch (item.slot) {
        case EquipSlot::HAT:
            composite.setLayerTexture("hat", item.spriteSheet);
            composite.setLayerVisible("hat", true);
            break;

        case EquipSlot::SHIRT:
            composite.setLayerTexture("shirt", item.spriteSheet);
            composite.setLayerTint("shirt", item.defaultColor);
            break;

        case EquipSlot::PANTS:
            composite.setLayerTexture("pants", item.spriteSheet);
            composite.setLayerTint("pants", item.defaultColor);
            break;

        case EquipSlot::ACCESSORY:
            composite.setLayerTexture("accessory", item.spriteSheet);
            composite.setLayerVisible("accessory", true);
            break;
    }
}

void Equipment::unequipItem(EquipSlot slot, CharacterComposite& composite) {
    switch (slot) {
        case EquipSlot::HAT:
            composite.setLayerVisible("hat", false);
            break;

        case EquipSlot::SHIRT:
            // Shirt is always visible -- revert to default
            composite.setLayerTexture("shirt", defaultShirtTexture);
            composite.setLayerTint("shirt", glm::vec4(1.0f));
            break;

        case EquipSlot::PANTS:
            composite.setLayerTexture("pants", defaultPantsTexture);
            composite.setLayerTint("pants", glm::vec4(1.0f));
            break;

        case EquipSlot::ACCESSORY:
            composite.setLayerVisible("accessory", false);
            break;
    }
}
```

### Complete Rendering Pipeline Pseudocode

Here is the full rendering pipeline from game loop to pixel output, showing how everything connects:

```
GAME LOOP (each frame):

1. INPUT
   │
   ├─ Read keyboard/gamepad state
   ├─ Determine movement direction and speed
   └─ Detect action inputs (equip, interact)

2. UPDATE
   │
   ├─ Update player position (velocity * deltaTime)
   │
   ├─ Update animation state machine:
   │   ├─ Determine new state (IDLE/WALK/ATTACK based on input)
   │   ├─ If state changed:
   │   │   ├─ Set composite animation row, frame count, duration
   │   │   └─ Update layer depth orders for new direction
   │   └─ Advance animation timer (composite.update(deltaTime))
   │
   ├─ Handle equipment changes:
   │   ├─ If item equipped: update layer texture/tint/visibility
   │   └─ If item unequipped: revert layer or hide
   │
   └─ Update camera (follow player)

3. RENDER
   │
   ├─ Clear screen (glClear)
   │
   ├─ Render world (tilemap layers, background objects)
   │
   ├─ For each character (sorted by Y position for depth):
   │   │
   │   ├─ Compute UV offset for current animation frame:
   │   │   uvX = (currentFrame * frameWidth) / sheetWidth
   │   │   uvY = (currentRow * frameHeight) / sheetHeight
   │   │   uvW = frameWidth / sheetWidth
   │   │   uvH = frameHeight / sheetHeight
   │   │
   │   ├─ Build model matrix from world position
   │   │
   │   ├─ For each visible layer (sorted by depthOrder):
   │   │   ├─ Bind layer.texture to texture unit 0
   │   │   ├─ Set uniform u_uvOffset = (uvX, uvY)
   │   │   ├─ Set uniform u_uvSize = (uvW, uvH)
   │   │   ├─ Set uniform u_tintColor = layer.tintColor
   │   │   ├─ Set uniform model = character's model matrix
   │   │   └─ glDrawArrays(GL_TRIANGLES, 0, 6)
   │   │
   │   └─ (All layers drawn for this character)
   │
   ├─ Render foreground objects, particles, UI
   │
   └─ Swap buffers (window.swapBuffers())
```

### Vertex Shader for Layered Sprites

The vertex shader is nearly identical to your existing `sprite.vert`. The key difference is that UV coordinates are remapped from full-quad (0-1) to the sub-region of the current animation frame:

```glsl
#version 330 core

// Vertex attributes: position and texture coordinates
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

// Interpolated outputs to fragment shader
out vec2 TexCoord;

// Transform matrices
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // Transform vertex position through MVP pipeline
    gl_Position = projection * view * model * vec4(aPos, 0.0, 1.0);

    // Pass texture coordinates to fragment shader
    // The fragment shader will apply u_uvOffset and u_uvSize
    // to remap these to the correct frame sub-region
    TexCoord = aTexCoord;
}
```

### Fragment Shader for Layered Sprites (Complete)

```glsl
#version 330 core

// Interpolated texture coordinates from vertex shader (0-1 range across quad)
in vec2 TexCoord;

// Final pixel color output
out vec4 FragColor;

// The sprite sheet texture bound to texture unit 0
uniform sampler2D spriteTexture;

// UV offset to the current frame's top-left corner (normalized)
// Computed on CPU: (currentFrame * frameWidth) / sheetWidth, etc.
uniform vec2 u_uvOffset;

// UV size of one frame (normalized)
// Computed on CPU: frameWidth / sheetWidth, frameHeight / sheetHeight
uniform vec2 u_uvSize;

// Tint color for this layer
// Set to (1, 1, 1, 1) for no tinting (white = identity for multiplication)
uniform vec4 u_tintColor;

void main() {
    // Remap the quad's 0-1 texture coordinates to the frame's sub-region
    // TexCoord * u_uvSize shrinks the UV range to one frame's width/height
    // + u_uvOffset shifts it to the correct frame position in the sheet
    vec2 frameUV = u_uvOffset + TexCoord * u_uvSize;

    // Sample the texture at the frame's UV coordinates
    vec4 texel = texture(spriteTexture, frameUV);

    // Discard fully transparent pixels
    // This prevents transparent areas from:
    //   a) Writing to the depth buffer (if depth testing is on)
    //   b) Affecting blending calculations
    //   c) Covering layers drawn below
    if (texel.a < 0.01) {
        discard;
    }

    // Apply multiplicative tinting
    // RGB channels are multiplied (color shift), alpha is multiplied (opacity control)
    FragColor = vec4(texel.rgb * u_tintColor.rgb, texel.a * u_tintColor.a);
}
```

---

## Glossary

| Term | Definition |
|------|-----------|
| **Layered composite** | Rendering a character by drawing multiple overlapping textured quads (layers) at the same position |
| **Combinatorial explosion** | The problem where N x M x O options require N*M*O assets unless decomposed into layers |
| **Paper doll** | Informal name for the layered composite technique, from physical paper dolls with removable clothing |
| **Registration** | The alignment of layers so that corresponding pixels overlay correctly across all frames |
| **Anchor point / Pivot point** | The reference point on a sprite that determines where position coordinates place it |
| **Multiplicative tinting** | Multiplying texture RGB by a uniform color (preserves shading, can only darken) |
| **Additive tinting** | Adding a color to texture RGB (brightens, used for glow effects) |
| **HSV shifting** | Converting RGB to Hue-Saturation-Value space, rotating hue, and converting back |
| **Palette swap / CLUT** | Replacing colors by looking up indices in a Color Look-Up Table |
| **FBO (Framebuffer Object)** | An OpenGL object that redirects rendering to a texture instead of the screen |
| **Compositing** | Combining multiple layers into a single image |
| **Baking** | Pre-rendering a composite to a texture so it can be reused without re-compositing |
| **Alpha blending** | GPU operation that mixes a new pixel with the existing framebuffer pixel based on alpha |
| **Painter's algorithm** | Drawing objects back-to-front so nearer objects naturally cover farther ones |
| **Depth order** | An integer or float that determines the drawing priority of a layer (lower = behind) |
| **Normal map** | A texture encoding surface direction vectors for lighting calculations |
| **Emission map** | A texture encoding which pixels glow (emit light independent of scene lighting) |
| **Mask texture** | A grayscale texture used to show/hide parts of another layer (white = visible, black = hidden) |
| **sRGB** | The standard color space for monitor display, where brightness is gamma-encoded (non-linear) |
| **Linear space** | A color space where numerical values are proportional to physical brightness |
| **Gamma correction** | The process of converting between sRGB and linear space for mathematically correct operations |
| **Texel** | A single pixel within a texture (distinguished from screen pixels) |
| **Draw call** | A single command to the GPU to render geometry (e.g., `glDrawArrays`) |
| **Base layer** | A layer that is always visible and never toggled (body, skin) |
| **Equipment layer** | A layer that appears/disappears based on game state (hat, weapon) |
