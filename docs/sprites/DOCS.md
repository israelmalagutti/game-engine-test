# Sprite Animation — Techniques, Theory, and Architecture

This document covers everything you need to understand about 2D character animation: from the simplest frame-by-frame approach to procedural generation. It explains the "why" behind each technique, connects them to what you already know, and gives you the mental model to choose the right approach for any situation.

## Table of Contents
- [Foundational Concept: What Animation Actually Is](#foundational-concept-what-animation-actually-is)
- [Technique 1: Frame-by-Frame Sprite Animation](#technique-1-frame-by-frame-sprite-animation)
  - [Sprite Sheets and Frame Indexing](#sprite-sheets-and-frame-indexing)
  - [Animation Timing and Delta Time](#animation-timing-and-delta-time)
  - [Animation State Machines](#animation-state-machines)
  - [Direction Handling in Top-Down Games](#direction-handling-in-top-down-games)
  - [The UV Math (You Already Know This)](#the-uv-math-you-already-know-this)
  - [Trade-offs Summary](#frame-by-frame-trade-offs)
- [Technique 2: Layered Composite Sprites](#technique-2-layered-composite-sprites)
  - [The Combinatorial Explosion Problem](#the-combinatorial-explosion-problem)
  - [How Layers Work at the Rendering Level](#how-layers-work-at-the-rendering-level)
  - [Layer Registration and Alignment](#layer-registration-and-alignment)
  - [Color Tinting for Customization](#color-tinting-for-customization)
  - [Render-to-Texture Optimization](#render-to-texture-optimization)
  - [How Stardew Valley Structures Its Layers](#how-stardew-valley-structures-its-layers)
  - [Trade-offs Summary](#layered-composite-trade-offs)
- [Technique 3: Palette Swapping / CLUT](#technique-3-palette-swapping--clut)
  - [Historical Context: How Hardware Palettes Worked](#historical-context-how-hardware-palettes-worked)
  - [Modern GPU Palette Swapping](#modern-gpu-palette-swapping)
  - [Indexed Color Textures](#indexed-color-textures)
  - [Palette Textures](#palette-textures)
  - [The Fragment Shader Lookup](#the-fragment-shader-lookup)
  - [Advanced: Palette Animation (Color Cycling)](#advanced-palette-animation-color-cycling)
  - [Combining CLUT with Layered Sprites](#combining-clut-with-layered-sprites)
  - [Trade-offs Summary](#clut-trade-offs)
- [Technique 4: Procedural Animation Basics](#technique-4-procedural-animation-basics)
  - [What "Procedural" Means](#what-procedural-means)
  - [Sine Waves: The Building Block](#sine-waves-the-building-block)
  - [Inverse Kinematics (IK)](#inverse-kinematics-ik)
  - [Spring Physics](#spring-physics)
  - [Hybrid: Procedural on Top of Frame-by-Frame](#hybrid-procedural-on-top-of-frame-by-frame)
  - [Case Study: Rain World](#case-study-rain-world)
  - [Trade-offs Summary](#procedural-trade-offs)
- [Skeletal / Bone Animation (Reference)](#skeletal--bone-animation-reference)
- [Choosing Your Approach](#choosing-your-approach)
- [Glossary](#glossary)

---

## Foundational Concept: What Animation Actually Is

Before diving into techniques, understand what animation fundamentally is: **displaying a sequence of different images fast enough that the human eye perceives continuous motion.**

This is identical to how video works. A 60fps game shows 60 still images per second. The only question is: **where do those images come from?**

```
Source of frames:

Pre-made (artist draws them)          Generated (code creates them)
├── Frame-by-Frame                    ├── Procedural math
├── Layered Composite                 ├── Skeletal/Bone rigs
└── Palette-swapped variants          ├── Physics simulation
                                      └── Mesh deformation
```

Every technique on the left side means **more artist work, more artistic control**. Every technique on the right means **more programmer work, more dynamic behavior**. Most real games use a hybrid — pre-made art enhanced with procedural touches.

### The Web Development Analogy

Think of it like CSS animations vs. JavaScript animations:

- **CSS keyframe animations** = Frame-by-frame. You define exact states at exact times. Total control, but you must specify everything.
- **CSS transitions** = Tweening/interpolation. You define start and end states, the browser fills in between. Less control, less work.
- **JavaScript requestAnimationFrame** = Procedural. You compute positions mathematically each frame. Maximum flexibility, maximum complexity.
- **React Spring / Framer Motion** = Physics-based. You define forces and constraints, the library simulates motion. Organic-feeling, harder to predict exact results.

The same spectrum exists in game animation.

---

## Technique 1: Frame-by-Frame Sprite Animation

This is the foundation. Every other technique builds on or reacts against this one. Even if you choose a different approach later, you must understand frame-by-frame first.

### Sprite Sheets and Frame Indexing

A sprite sheet is a texture atlas where each cell is one frame of animation. You already built tilemap rendering using exactly this concept — indexing into a grid of sub-images within a larger texture.

```
A character sprite sheet (4 columns × 4 rows = 16 frames):

Column:     0        1        2        3
         ┌────────┬────────┬────────┬────────┐
Row 0:   │Walk D 0│Walk D 1│Walk D 2│Walk D 3│  ← Walking Down
         ├────────┼────────┼────────┼────────┤
Row 1:   │Walk L 0│Walk L 1│Walk L 2│Walk L 3│  ← Walking Left
         ├────────┼────────┼────────┼────────┤
Row 2:   │Walk R 0│Walk R 1│Walk R 2│Walk R 3│  ← Walking Right
         ├────────┼────────┼────────┼────────┤
Row 3:   │Walk U 0│Walk U 1│Walk U 2│Walk U 3│  ← Walking Up
         └────────┴────────┴────────┴────────┘

Each cell is, say, 32×48 pixels (width × height).
The full texture is 128×192 pixels (4×32 wide, 4×48 tall).
```

To display frame (column=2, row=1) — which is "Walking Left, frame 2" — you calculate UV coordinates exactly like you do for tilemap tiles:

```
frameWidth  = 32px     (width of one frame)
frameHeight = 48px     (height of one frame)
textureW    = 128px    (total texture width)
textureH    = 192px    (total texture height)

// Normalized UV coordinates (0.0 to 1.0)
u_left   = column * frameWidth  / textureW  = 2 * 32 / 128 = 0.5
u_right  = (column + 1) * frameWidth / textureW = 3 * 32 / 128 = 0.75
v_top    = row * frameHeight / textureH = 1 * 48 / 192 = 0.25
v_bottom = (row + 1) * frameHeight / textureH = 2 * 48 / 192 = 0.5
```

This is identical to the UV calculation in your `Tilemap::draw()` — same atlas math, different purpose.

### Animation Timing and Delta Time

The core loop of animation is: "advance to the next frame after enough time has passed."

```
Animation state:
  currentFrame = 0          (which frame in the sequence we're showing)
  elapsedTime  = 0.0        (time accumulated since last frame change)
  frameDuration = 0.15      (seconds per frame — ~6.67 fps animation)

Each game update:
  elapsedTime += deltaTime

  if (elapsedTime >= frameDuration):
      elapsedTime -= frameDuration    // subtract, don't reset to 0!
      currentFrame = (currentFrame + 1) % totalFrames
```

**Why subtract instead of reset?** If `frameDuration` is 0.15s and `elapsedTime` reaches 0.17s, there's 0.02s of "leftover" time. Resetting to 0 loses that 0.02s, causing animation to slowly drift slower than intended. Subtracting preserves the remainder, keeping the animation rate accurate over time. This is the same principle as your fixed-timestep game loop.

**Why not tie animation to game FPS?** If your game runs at 60fps and you advance the animation every 10 game frames, you get 6fps animation — but only if the game actually runs at 60fps. On a slow machine running at 30fps, the animation would be 3fps. On a 144Hz monitor, it'd be 14.4fps. Delta time decouples animation speed from frame rate, exactly like you already do for movement.

### Variable Frame Durations

Not all frames should display for the same duration. In a sword swing:

```
Frame 0: Wind-up     (0.2s)  — slow anticipation
Frame 1: Wind-up 2   (0.15s) — building tension
Frame 2: Swing        (0.05s) — fast! The actual strike
Frame 3: Follow-through (0.1s) — deceleration
Frame 4: Recovery     (0.2s)  — returning to idle
```

This is the animation principle of **spacing** — frames closer together in time appear faster. Disney's 12 principles of animation apply directly to game sprites, even at pixel-art scale.

The simplest way to handle this is storing durations per-frame:

```
Animation "sword_swing":
  frames:    [0,    1,    2,    3,    4   ]
  durations: [0.2,  0.15, 0.05, 0.1,  0.2 ]
  loop: false
```

### Animation State Machines

A character has multiple animations: idle, walk, run, attack, hurt, die. You need a system to manage which animation is active and when to switch. This is a **finite state machine (FSM)** — a concept you might know from web dev as the state pattern in Redux or XState.

```
                    ┌──────────┐
        ┌──────────→│   IDLE   │←──────────┐
        │           └────┬─────┘           │
        │                │                 │
    no input         movement          attack ends
        │            input             (timer)
        │                │                 │
        │           ┌────▼─────┐    ┌──────┴─────┐
        │           │   WALK   │───→│   ATTACK   │
        │           └────┬─────┘    └──────┬─────┘
        │                │                 │
        │            damage            damage
        │            taken             taken
        │                │                 │
        │           ┌────▼─────┐           │
        └───────────│   HURT   │←──────────┘
                    └────┬─────┘
                         │
                     health <= 0
                         │
                    ┌────▼─────┐
                    │   DIE    │
                    └──────────┘
```

Each state maps to an animation:
- `IDLE` → plays "idle" animation (looping)
- `WALK` → plays "walk_down" / "walk_up" / etc. based on direction (looping)
- `ATTACK` → plays "attack" animation (one-shot, returns to IDLE when done)
- `HURT` → plays "hurt" animation (one-shot with brief invulnerability)
- `DIE` → plays "death" animation (one-shot, then entity is deactivated)

**Transitions** define when to switch states. A transition has:
- **Source state**: Where you're coming from
- **Condition**: What triggers the switch (input, timer, game event)
- **Target state**: Where you're going
- **Interruption rules**: Can this animation be canceled? An attack might be non-interruptible, but walking can be interrupted by anything.

### Direction Handling in Top-Down Games

In a top-down game, direction adds another axis to animation. The character has a **facing direction** that's separate from the animation state:

```
Total animation set for one character:

               Down      Left      Right      Up
             ┌────────┬─────────┬─────────┬────────┐
Idle:        │ 1 frame│ 1 frame │ 1 frame │ 1 frame│
Walk:        │ 4 frame│ 4 frame │ 4 frame │ 4 frame│
Attack:      │ 5 frame│ 5 frame │ 5 frame │ 5 frame│
Hurt:        │ 2 frame│ 2 frame │ 2 frame │ 2 frame│
Die:         │ 4 frame│ 4 frame │ 4 frame │ 4 frame│
             └────────┴─────────┴─────────┴────────┘

Total: 4 directions × (1+4+5+2+4) = 4 × 16 = 64 frames minimum
```

**Common optimization: horizontal mirroring.** If your art style allows it (symmetric characters), you can draw only "walk left" and flip the UV coordinates horizontally to get "walk right" for free. This halves the art workload for left/right directions.

In the shader or when setting up the quad, you swap `u_left` and `u_right`:

```
Normal (facing right):    UV: left=0.0, right=0.25
Flipped (facing left):    UV: left=0.25, right=0.0   ← swapped!
```

The GPU renders the texture mirrored. No extra texture data needed.

### The UV Math (You Already Know This)

This is worth emphasizing: **you already implemented this exact UV math in your tilemap system.** The tilemap picks a tile ID, converts it to a row and column in the tileset atlas, and computes UV coordinates. Sprite animation is the same operation, except:

- Instead of a tile ID from a map array, you have a **frame index from an animation state**
- Instead of static (never changes), the index **advances over time**
- Instead of one texture for all tiles, you have **one texture per character** (or a shared atlas)

```
Tilemap rendering (what you have):
  tileID → (column, row) in atlas → UV offset → draw

Sprite animation (what you need):
  animationState + elapsedTime → frameIndex → (column, row) in sheet → UV offset → draw
```

The vertex shader and fragment shader can be almost identical. The only new logic is on the CPU side: tracking animation state and computing which frame to show.

### Frame-by-Frame Trade-offs

| Strength | Weakness |
|----------|----------|
| Total artistic control over every frame | Every frame must be hand-drawn |
| Simplest to implement (UV math you already have) | Combinatorial explosion with customization |
| Pixel-perfect at any resolution | Memory grows linearly with animation count |
| No runtime artifacts (no interpolation, no mesh tearing) | Adding a new animation means drawing all frames × all directions |
| Easy to debug (you can see every frame in the sheet) | No dynamic variation — every walk cycle is identical every time |

---

## Technique 2: Layered Composite Sprites

### The Combinatorial Explosion Problem

Imagine your game has character creation. The player can choose from:

```
Body types:    4
Skin tones:   10
Hairstyles:   30
Hair colors:  15
Shirts:       25
Shirt colors: 20
Pants:        20
Pant colors:  15
Hats:          8
```

With monolithic sprite sheets (one unique sheet per combination):

```
4 × 10 × 30 × 15 × 25 × 20 × 20 × 15 × 8 = 21,600,000,000 sheets
```

21.6 billion. Even if each sheet is tiny, this is impossible.

With layers:

```
4 + 10 + 30 + 15 + 25 + 20 + 20 + 15 + 8 = 147 sprite sheets
```

147 sheets, each with the same frame layout. This is the power of decomposition.

But colors add another dimension. Those 15 hair colors × 30 hairstyles = 450 hair sheets if you pre-draw every color. Instead, you draw 30 hairstyles in a neutral/white color and **tint** at runtime. Now you need 30 hair sheets and a color value.

```
Final asset count:
  4 body sheets  (can't tint — skin shading is complex)
  30 hair sheets (tinted to any color)
  25 shirt sheets (tinted to any color)
  20 pant sheets (tinted to any color)
  8 hat sheets   (may or may not be tinted)
  = 87 sprite sheets total

+ a handful of color values (stored as vec3, essentially free)
```

From 21.6 billion to 87. That's what layers + tinting gives you.

### How Layers Work at the Rendering Level

Each frame of the character is drawn as **multiple quads stacked on top of each other** at the same position:

```
Frame rendering order (back to front):

  1. Draw body quad    at position (x, y) with body sprite sheet, frame N
  2. Draw pants quad   at position (x, y) with pants sprite sheet, frame N, tinted
  3. Draw shirt quad   at position (x, y) with shirt sprite sheet, frame N, tinted
  4. Draw hair quad    at position (x, y) with hair sprite sheet, frame N, tinted
  5. Draw hat quad     at position (x, y) with hat sprite sheet, frame N (if equipped)
```

Each layer uses the **same frame index N** and the **same screen position**. They differ only in which texture is bound and what tint color is applied.

In OpenGL terms, each layer is:

```
1. glBindTexture(GL_TEXTURE_2D, layerTexture)
2. Set uniform: u_uvOffset = computed UV for frame N
3. Set uniform: u_tintColor = layer's color (or white for no tint)
4. Set uniform: u_model = same model matrix (same position/size)
5. glDrawArrays(GL_TRIANGLES, 0, 6)
```

That's 5 draw calls per character (one per layer). For 20 characters on screen, that's 100 draw calls — easily within budget for a 2D game.

### Layer Registration and Alignment

For layers to stack correctly, **every sprite sheet must use identical frame layouts**:

```
Body sheet:                    Hair sheet:                   Shirt sheet:
┌────┬────┬────┬────┐         ┌────┬────┬────┬────┐        ┌────┬────┬────┬────┐
│    │    │    │    │         │    │    │    │    │        │    │    │    │    │
│ ██ │ ██ │ ██ │ ██ │         │ ▓▓ │ ▓▓ │ ▓▓ │ ▓▓ │        │ ░░ │ ░░ │ ░░ │ ░░ │
│████│████│████│████│         │    │    │    │    │        │████│████│████│████│
│ ██ │ ██ │ ██ │ ██ │         │    │    │    │    │        │    │    │    │    │
└────┴────┴────┴────┘         └────┴────┴────┴────┘        └────┴────┴────┴────┘

When overlaid (composited):
┌────┬────┬────┬────┐
│ ▓▓ │ ▓▓ │ ▓▓ │ ▓▓ │  ← hair on top
│░██░│░██░│░██░│░██░│  ← shirt over body
│████│████│████│████│  ← body visible where others are transparent
│ ██ │ ██ │ ██ │ ██ │  ← legs (body layer)
└────┴────┴────┴────┘
```

**Every frame must be the same pixel dimensions.** Every layer's frame 0 must represent the same pose. If the body's frame 3 is "left foot forward," then every layer's frame 3 must also be drawn for the "left foot forward" pose.

This is the main constraint of the layered approach: **artists must maintain perfect alignment across all layers.** A 1-pixel misalignment between the hair and body layers would be visible and distracting.

In practice, artists work in a single PSD/Aseprite file with each layer on a separate canvas layer, then export each layer as its own sprite sheet. This guarantees alignment because all layers share the same canvas.

### Color Tinting for Customization

The simplest color customization: **multiply the texture color by a tint color in the fragment shader.**

```
How multiplication tinting works:

Texture pixel (white shirt, has shading):
  Light area:  (0.9, 0.9, 0.9, 1.0)  — nearly white
  Medium area: (0.6, 0.6, 0.6, 1.0)  — medium gray
  Dark area:   (0.3, 0.3, 0.3, 1.0)  — dark gray (shadow)

Tint color (player chose blue): (0.2, 0.4, 1.0, 1.0)

Result = texture × tint:
  Light area:  (0.9×0.2, 0.9×0.4, 0.9×1.0) = (0.18, 0.36, 0.90)  — bright blue
  Medium area: (0.6×0.2, 0.6×0.4, 0.6×1.0) = (0.12, 0.24, 0.60)  — medium blue
  Dark area:   (0.3×0.2, 0.3×0.4, 0.3×1.0) = (0.06, 0.12, 0.30)  — dark blue

The shading (light/medium/dark relationship) is preserved!
The hue is shifted to blue!
```

Fragment shader for tinting:

```glsl
#version 330 core

in vec2 TexCoord;

uniform sampler2D spriteTexture;
uniform vec4 tintColor;   // e.g., vec4(0.2, 0.4, 1.0, 1.0) for blue

out vec4 FragColor;

void main() {
    vec4 texel = texture(spriteTexture, TexCoord);

    // Discard fully transparent pixels (no blending needed)
    if (texel.a < 0.01)
        discard;

    // Multiply RGB by tint, preserve original alpha
    FragColor = vec4(texel.rgb * tintColor.rgb, texel.a);
}
```

**Limitation:** Multiplication can only **darken**. A medium-gray texture tinted with any color will always be darker than the tint. You can't make a dark texture brighter. For more control, you need palette swapping (Technique 3).

**Workaround:** Draw tintable layers in very light grayscale (near-white). This gives the most dynamic range for tinting. Stardew Valley does this — the customizable clothing layers are drawn in light, almost-white tones.

### Render-to-Texture Optimization

Drawing 5 layers per character per frame means 5 draw calls per character. With 30 characters on screen, that's 150 draw calls just for characters. This is fine for most 2D games, but you can optimize with **Framebuffer Objects (FBOs)**:

```
Optimization: Render-to-Texture (FBO compositing)

Instead of drawing 5 layers to the screen each frame:

1. On character creation (or equipment change):
   a. Create an FBO with a texture attachment (the "baked" sprite sheet)
   b. Bind the FBO (render to it instead of the screen)
   c. Draw all 5 layers composited onto this texture
   d. Unbind the FBO

2. Every frame:
   a. Just draw 1 quad with the pre-composited texture
   b. Use the same frame indexing as frame-by-frame animation

Now each character is 1 draw call again!
Re-bake only when equipment changes (infrequent).
```

This is a **pre-computation** technique. You trade memory (one extra texture per unique character configuration) for draw call reduction. It's exactly the same concept as server-side rendering in web dev — do the expensive work once, serve the cached result.

In practice, most 2D pixel art games don't need this optimization. 100-200 draw calls is nothing for a modern GPU. But it's worth knowing the technique exists.

### How Stardew Valley Structures Its Layers

Stardew Valley's character rendering (decompiled from the game code) uses roughly this structure:

```
Player rendering order:
  1. Body (includes skin color, base animations — this is the largest sheet)
  2. Arm (back arm, drawn behind body in some poses)
  3. Pants (colored via tint)
  4. Shirt (colored via tint)
  5. Arm (front arm, drawn in front of shirt)
  6. Hair (style selected, colored via tint)
  7. Accessory (beard, glasses, etc.)
  8. Hat (if equipped)

Note: Arms are split from the body because they need to appear
both behind AND in front of the shirt depending on the animation frame.
This is a common challenge in layered sprite systems.
```

The arm split is a key insight: **layer order isn't always fixed.** During a "facing down" pose, both arms are in front of the torso. During a "facing right" pose, the left arm is behind the torso and the right arm is in front. This requires either:

- Splitting body parts into more layers with dynamic ordering
- Having multiple sprite sheets for the same body part at different depths
- Using per-frame metadata that specifies draw order

This is the main source of complexity in layered sprite systems. The more layers you add, the more edge cases arise with overlapping.

### Layered Composite Trade-offs

| Strength | Weakness |
|----------|----------|
| Turns multiplicative customization into additive | All layers must perfectly align (artist discipline) |
| Artists can work on layers independently | Layer ordering gets complex (arms behind/in front of torso) |
| Color tinting is trivial in a shader | Still frame-by-frame art per layer |
| Players feel ownership over their character | More draw calls per entity (5-8 vs. 1) |
| New content (hats, shirts) is additive, not multiplicative | Some visual combinations may clip or look wrong |

---

## Technique 3: Palette Swapping / CLUT

### Historical Context: How Hardware Palettes Worked

On the NES (1983), the hardware literally could not store full RGB colors per pixel. The entire system had 64 possible colors. Each sprite could use only 3 colors plus transparent (a "palette" of 4 entries). The sprite data stored 2-bit indices (values 0-3) that the hardware mapped to actual colors via a **palette table** (Color Look-Up Table — CLUT).

```
NES hardware palette system:

Sprite pixel data (2 bits per pixel):
┌─┬─┬─┬─┬─┬─┬─┬─┐
│0│1│2│2│2│1│0│0│
│1│2│3│3│3│2│1│0│
│0│1│2│2│2│1│0│0│
└─┴─┴─┴─┴─┴─┴─┴─┘

Palette table (4 entries):
  Index 0 → Transparent
  Index 1 → Color #0F (black)
  Index 2 → Color #16 (red)
  Index 3 → Color #30 (white)

To create a "blue" variant of the same enemy:
  DON'T redraw the sprite
  DO change the palette:
    Index 0 → Transparent
    Index 1 → Color #0F (black)    ← same
    Index 2 → Color #12 (blue)     ← changed!
    Index 3 → Color #30 (white)    ← same
```

This is why classic games had palette-swapped enemies (red Koopa, green Koopa, blue Koopa). It was a **hardware feature** that cost zero extra memory for the sprite data.

### Modern GPU Palette Swapping

Modern GPUs don't have hardware palettes. Every pixel in a texture is full RGBA. But you can **recreate** the palette system in a fragment shader, which gives you even more flexibility than the original hardware.

The approach:

```
Classic hardware:
  [Sprite memory] → [Palette hardware] → [Screen color]
       2-bit indices    color table          final pixel

Modern shader:
  [Index texture] → [Fragment shader] → [Screen color]
       R channel      palette lookup       final pixel
   (stores index)   (samples palette)
```

### Indexed Color Textures

Instead of storing actual colors in your sprite texture, you store **indices** — numbers that reference a color in a separate palette. Since you need to store these in a standard RGBA texture (that's what OpenGL expects), you encode the index in one color channel:

```
Standard texture (what you currently use):
  Pixel at (5, 3) = RGBA(180, 50, 50, 255)   ← actual red color

Indexed texture (for palette swapping):
  Pixel at (5, 3) = RGBA(4, 0, 0, 255)        ← index 4 into palette
                          ↑
                     This is the palette index, not a color!
                     The R channel stores the index.
                     G and B are unused (set to 0).
                     A still means transparency.
```

**How to create indexed textures:** You draw your sprite art with a specific, limited palette (say, 16 colors). Then you convert the image so that each pixel stores the index of its color in the palette, rather than the color itself.

In Aseprite (a popular pixel art tool), sprites are naturally indexed-color when using the "Indexed" color mode. Each pixel IS a palette index internally. You export the palette separately and the sprite as an indexed image.

For use in OpenGL, you'd convert the indexed image so the palette index is stored in the red channel:

```
Original indexed pixel:    palette index = 7
Stored in texture as:      R=7, G=0, B=0, A=255    (if opaque)
                           R=0, G=0, B=0, A=0      (if transparent)
```

### Palette Textures

The palette itself is stored as a **1D texture** (or a row of a 2D texture). Each pixel in the palette texture IS a color.

```
Palette as a 1D texture (16 colors wide, 1 pixel tall):

Position: 0      1      2      3      4      5     ...   15
Color:   black  d.gray  gray  l.gray  red   d.red  ...  white

So palette[0] = (0, 0, 0)       — black
   palette[4] = (255, 50, 50)   — red
   palette[7] = (50, 50, 255)   — blue
```

To create a "blue variant" of a "red" enemy, you create a second palette where the red entries are replaced with blue entries:

```
Palette A (fire enemy):    Palette B (ice enemy):
  0: black                   0: black           ← same
  1: dark gray               1: dark gray       ← same
  2: dark red                2: dark blue        ← swapped!
  3: red                     3: blue             ← swapped!
  4: bright red              4: bright blue      ← swapped!
  5: orange                  5: cyan             ← swapped!
  ...                        ...
```

**Multiple palettes in one texture:** Store palettes as rows of a 2D texture:

```
2D Palette Texture (16 colors wide × 4 palettes tall):

Row 0: Normal palette     [black, gray, red, orange, ...]
Row 1: Ice palette        [black, gray, blue, cyan, ...]
Row 2: Poison palette     [black, gray, green, lime, ...]
Row 3: Shadow palette     [black, d.gray, purple, violet, ...]
```

To select a palette, you choose which row to sample. This is extremely efficient — one small texture holds all color variants.

### The Fragment Shader Lookup

Here's how the shader brings it all together:

```glsl
#version 330 core

in vec2 TexCoord;

uniform sampler2D indexTexture;    // The sprite (stores indices in R channel)
uniform sampler2D paletteTexture;  // The palette (colors in a row)
uniform float paletteRow;          // Which palette to use (0.0, 1.0, 2.0...)
uniform float paletteSize;         // How many colors in the palette (e.g., 16.0)
uniform float paletteCount;        // How many palettes total (e.g., 4.0)

out vec4 FragColor;

void main() {
    // 1. Sample the index texture
    vec4 indexSample = texture(indexTexture, TexCoord);

    // Transparent pixels remain transparent
    if (indexSample.a < 0.01)
        discard;

    // 2. Extract the palette index from the red channel
    //    The R value is 0-255, normalized to 0.0-1.0 by OpenGL
    //    Multiply by 255 to get the original index, then normalize to palette UV
    float index = indexSample.r * 255.0;

    // 3. Calculate UV into the palette texture
    //    X = position along the palette row (which color)
    //    Y = which palette row to use
    float u = (index + 0.5) / paletteSize;           // +0.5 for texel center
    float v = (paletteRow + 0.5) / paletteCount;     // +0.5 for texel center

    // 4. Look up the actual color from the palette
    vec4 color = texture(paletteTexture, vec2(u, v));

    FragColor = color;
}
```

The `+ 0.5` offsets are critical. Without them, you'd sample at the edge between two palette entries, and texture filtering might blend them. Adding 0.5 moves the sample point to the **center** of each texel. This is the same texture bleeding concern described in your `10-texture-atlas.md` doc.

Also, the palette texture must use `GL_NEAREST` filtering (no interpolation), or colors will blend at boundaries:

```
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
```

### Advanced: Palette Animation (Color Cycling)

A powerful trick from the CLUT era: **animate the palette itself** instead of the sprite. By shifting colors through palette entries over time, you can create the illusion of movement without changing the sprite at all.

```
Classic example: Flowing water

Frame 1 palette:     Frame 2 palette:     Frame 3 palette:
  0: dark blue         0: medium blue       0: light blue
  1: medium blue       1: light blue        1: white
  2: light blue        2: white             2: dark blue
  3: white             3: dark blue         3: medium blue

The sprite data never changes. But the colors cycle through the palette
entries, creating the illusion of flowing water.
```

This technique was used extensively in:
- **LucasArts adventure games** (Monkey Island's water, lava, and fire effects)
- **Shovel Knight** (waterfall animations, glowing effects)
- **Classic JRPGs** (spell effects, environmental animations)

In your engine, this would be trivial: just update the palette texture's pixel data each frame (or swap which palette row the shader reads from on a timer).

### Combining CLUT with Layered Sprites

CLUT and layered sprites aren't mutually exclusive. You can use both:

```
Hybrid approach:
  Body layer    → palette-swapped (skin tones = different palettes)
  Hair layer    → palette-swapped (hair colors = different palettes)
  Shirt layer   → multiply-tinted (simple enough for single-hue clothing)
  Pants layer   → multiply-tinted
  Accessory     → no modification (pre-drawn in fixed colors)
```

Use palette swapping where you need **precise, multi-color remapping** (skin has highlights, midtones, and shadows that should map to different hues). Use simple tinting where you just need a **hue shift** (a solid-color shirt).

### CLUT Trade-offs

| Strength | Weakness |
|----------|----------|
| Nearly zero memory cost for color variants | Requires special indexed textures (can't use normal art directly) |
| Can remap ANY color to ANY other color | More complex shader and asset pipeline |
| Palette animation is uniquely powerful | Limited to color changes — can't change shape |
| Historically authentic for retro aesthetics | Artists must work within palette constraints |
| GPU-efficient (one texture lookup) | Debugging is harder (sprites look wrong in normal image viewers) |

---

## Technique 4: Procedural Animation Basics

### What "Procedural" Means

"Procedural" means **generated by an algorithm at runtime** rather than pre-made by an artist. In procedural animation, the position, rotation, and scale of visual elements are computed mathematically each frame.

This doesn't mean "no art." It usually means "static art, dynamic positioning." You might still draw a character's arm, but instead of pre-making frames of the arm swinging, you rotate the arm sprite with code.

### Sine Waves: The Building Block

Almost all procedural animation starts with trigonometric functions, especially sine and cosine. Here's why:

```
sin(t) over time produces smooth oscillation:

Value
 1.0 ─────╲──────────────╱──────────────╲──────
           ╲            ╱                ╲
 0.0 ───────╲──────────╱──────────────────╲────
              ╲      ╱                      ╲
-1.0 ──────────╲────╱────────────────────────╲─
                ╲╱                            ╲╱
      Time → →

This naturally models:
  - Breathing (chest expanding and contracting)
  - Walking (legs swinging back and forth)
  - Bobbing (idle hover, underwater floating)
  - Swaying (trees in wind, grass, banners)
```

Combine parameters to control the motion:

```
offset = sin(time * frequency) * amplitude

frequency: How fast the oscillation (higher = faster bobbing)
amplitude: How far the oscillation (higher = larger movement)
phase:     Offset the wave in time: sin(time * freq + phase)
           Used to make multiple things oscillate out of sync
```

Example — idle breathing animation:

```
// Character bobs up and down gently while idle
float breathOffset = sin(totalTime * 2.0) * 1.5;
// 2.0 = ~1 breath per second (2π radians/s ≈ 1 cycle/s at freq 2.0... actually
//        frequency of 2.0 means 2/(2π) ≈ 0.32 cycles per second, or one breath
//        every ~3 seconds — slow, relaxed breathing)
// 1.5 = 1.5 pixels of vertical movement

characterY = baseY + breathOffset;
```

Example — walking leg swing:

```
float leftLegAngle  = sin(totalTime * 8.0) * 30.0;  // degrees
float rightLegAngle = sin(totalTime * 8.0 + PI) * 30.0;  // opposite phase!

// +PI means the right leg is exactly half a cycle behind the left leg
// When left leg is forward (+30°), right leg is backward (-30°)
```

### Inverse Kinematics (IK)

IK answers the question: **"Given where I want my hand/foot to be, what angles should all the joints be?"**

Regular animation (Forward Kinematics / FK) works top-down:
```
FK: Shoulder rotates 30° → Elbow position is determined → Hand position follows
    You control the joints, the end position is a consequence.

IK: Hand needs to reach position (150, 200) → What should elbow and shoulder angles be?
    You control the end position, the joint angles are calculated.
```

For a 2-bone chain (like upper arm + forearm), IK has an analytical solution using the **law of cosines**:

```
Two-bone IK:

   Shoulder (fixed)
       ●
      ╱
     ╱ bone1 (upper arm, length L1)
    ╱
   ●  Elbow (calculated)
    ╲
     ╲ bone2 (forearm, length L2)
      ╲
       ◎  Target (where we want the hand)

Given: shoulder position, target position, L1, L2
Find: elbow position

Step 1: Distance from shoulder to target
  d = distance(shoulder, target)

Step 2: Check reachability
  if d > L1 + L2: target is too far (arm fully extended toward target)
  if d < |L1 - L2|: target is too close (impossible)

Step 3: Law of cosines to find the elbow angle
  cos(angle_at_elbow) = (L1² + L2² - d²) / (2 × L1 × L2)

Step 4: Compute elbow position using the angle
  (This involves some trigonometry to place the elbow in 2D space)
```

**Why IK matters for games:**
- Feet can adapt to terrain (walk up slopes, stand on uneven ground)
- Hands can reach toward objects (grab items, climb ledges)
- Creatures with many legs can walk realistically on any surface (Rain World)

**Why IK is hard:** For chains longer than 2 bones, there's no clean analytical solution. You need iterative algorithms like **FABRIK** (Forward And Backward Reaching Inverse Kinematics) or **CCD** (Cyclic Coordinate Descent) that converge toward a solution over multiple iterations.

### Spring Physics

Springs create organic, reactive motion. Instead of placing something at a position instantly, you define a "rest position" and the object bounces toward it:

```
Spring simulation:

State: position, velocity
Constants: stiffness (k), damping (d)

Each frame:
  force = -k * (position - restPosition)    // Pull toward rest
  force += -d * velocity                     // Friction/damping
  velocity += force * deltaTime
  position += velocity * deltaTime
```

```
Visualization of spring motion over time:

Position
  ↑
  │    ╱╲
  │   ╱  ╲     ╱╲
  │──╱────╲───╱──╲────╱─── rest position
  │ ╱      ╲ ╱    ╲  ╱
  │╱        ╲╱      ╲╱
  │
  └─────────────────────→ Time

Oscillates around rest position, amplitude decreasing due to damping.
Eventually settles at rest.
```

Springs are used for:
- **Hair/cloth following character movement** — rest position is the character's head, hair springs behind
- **Camera smoothing** — camera springs toward player position instead of snapping
- **Squash and stretch** — on landing, the character sprite compresses (squash) then bounces back (stretch)
- **UI juice** — menu elements spring into position instead of appearing instantly

In web dev terms, this is the same principle behind `react-spring` and physics-based CSS animations.

### Hybrid: Procedural on Top of Frame-by-Frame

The most practical approach for your project: **use frame-by-frame for the main animation, add procedural touches on top.**

```
Frame-by-frame base:
  Walk cycle: 4 pre-drawn frames (0, 1, 2, 3)
  These provide the main body motion.

Procedural additions:
  - Subtle vertical bob: sin(time * walkSpeed) * 1px during walk
  - Hair/cape follows with spring delay
  - Squash on landing after a jump (scale Y *= 0.85 for 3 frames)
  - Weapon sway using sine wave when idle
  - Shadow scales inversely during jump (smaller shadow = higher up)
  - Head turns slightly toward interaction target (lerp rotation)
```

This gives you the artistic control of hand-drawn animation WITH the organic reactivity of procedural motion. The pre-drawn frames handle the complex stuff (body poses, limb positions, silhouette changes) while the procedural layer adds polish that would be impossible to pre-draw (different every time, reactive to game state).

**Celeste** does this brilliantly — Madeline's hair is a chain of procedurally positioned sprites that follow her movement with spring physics, while her body is traditional frame-by-frame animation.

### Case Study: Rain World

Rain World (2017) is the most extreme example of procedural animation in 2D games. Worth studying even if you don't replicate it:

**How it works:**
- Every creature is a collection of **body points** connected by **springs and constraints**
- Body points have physics (gravity, friction, collision with terrain)
- The visual appearance is procedurally generated from the body point positions:
  - Limbs are drawn as curves between body points
  - Textures are mapped onto the curves
  - The appearance emerges from the physics state

**Why it looks so alive:**
- No two walk cycles are identical — the physics creates micro-variations
- Creatures respond to terrain naturally — climbing, slipping, squeezing through gaps
- Movement is reactive — bump into a wall and the body deforms realistically
- Personality emerges from physics parameters (heavy vs. light, stiff vs. floppy)

**Why Stardew Valley didn't do this:**
- Rain World's aesthetic IS the procedural movement. The whole game is about creatures that feel alive
- Stardew's aesthetic is SNES nostalgia. Frame-by-frame IS the look they're going for
- Rain World took 6+ years of development, much of it tuning procedural animation
- Procedural animation can't guarantee pixel-perfect poses. In pixel art, a 1-pixel difference is 3-5% of the entire character height. It matters aesthetically

### Procedural Trade-offs

| Strength | Weakness |
|----------|----------|
| Infinite variation — no two cycles identical | Very hard to art-direct precisely |
| Reactive to game state (terrain, impacts) | Debugging is difficult (emergent behavior) |
| Zero pre-made animation frames needed | Can look "floaty" or "mechanical" if poorly tuned |
| Can simulate complex physics (cloth, hair, chains) | Performance cost for many simulated entities |
| Unique feel that frame-by-frame can't replicate | Years of tuning for quality results |
| Minimal art assets | Requires strong math/physics foundation |

---

## Skeletal / Bone Animation (Reference)

Since you didn't select this for deep-dive but it's important to know, here's the executive summary:

**Tools:** Spine (industry standard, paid), DragonBones (free), Godot's built-in 2D skeleton

**How it works:** Cut a character image into parts (head, torso, limbs). Attach each part to a "bone" in a hierarchy. Animate by rotating/moving bones. The runtime recombines parts each frame.

**Best for:** Side-view games with medium-to-high resolution art (Hollow Knight, Darkest Dungeon). Allows animation blending (smoothly transition from walk to run) which frame-by-frame can't do.

**Worst for:** Pixel art. Rotating pixel art by non-90° angles destroys the crispness. Top-down games (need separate rigs per direction).

**The Dead Cells hybrid:** Animate a 3D model → render to 2D → artist paints over every frame in pixel art. Gets skeletal motion quality with pixel art final output. Extremely labor-intensive but produces the best results in the industry for animated pixel art.

---

## Choosing Your Approach

Given your project goals (HD-2D Stardew Valley clone with Octopath-style shaders), here's how these techniques map:

```
Your game needs:

┌─────────────────────────────┬──────────────────────────────┐
│ Feature                     │ Best technique               │
├─────────────────────────────┼──────────────────────────────┤
│ Player character animation  │ Frame-by-frame               │
│ Player customization        │ Layered composite + tinting  │
│ NPC animations              │ Frame-by-frame               │
│ Enemy color variants        │ Palette swapping (CLUT)      │
│ Hair/cape physics           │ Procedural (springs)         │
│ Idle breathing/bobbing      │ Procedural (sine wave)       │
│ Environmental animation     │ Palette cycling (water, lava)│
│ HD-2D visual style          │ Post-processing shaders      │
│ Attack/damage feedback      │ Procedural (squash/stretch)  │
└─────────────────────────────┴──────────────────────────────┘
```

The answer is almost always **"use multiple techniques together."** Frame-by-frame for the base, layers for customization, CLUT for enemy variants, procedural for polish and reactivity.

---

## Glossary

| Term | Definition |
|------|-----------|
| **Sprite sheet** | A single texture containing multiple animation frames in a grid |
| **Frame index** | Which frame of an animation is currently displayed |
| **Animation state machine** | System that tracks current animation and rules for transitioning |
| **Palette / CLUT** | A table mapping index numbers to actual colors |
| **Tinting** | Multiplying texture color by a uniform color to shift hue |
| **IK (Inverse Kinematics)** | Calculating joint angles to reach a target position |
| **FK (Forward Kinematics)** | Rotating joints, letting end positions follow |
| **Spring physics** | Simulating elastic force to create organic, bouncy motion |
| **FBO (Framebuffer Object)** | OpenGL object that lets you render to a texture instead of the screen |
| **Texel** | A single pixel within a texture (to distinguish from screen pixels) |
| **UV coordinates** | Normalized (0.0-1.0) coordinates that map texture pixels to geometry |
| **FABRIK** | Iterative IK algorithm that works for chains of any length |
| **Squash and stretch** | Animation principle: compress on impact, elongate during fast motion |
| **Phase offset** | Shifting a sine wave in time so multiple objects oscillate out of sync |
| **Frame duration** | How long (in seconds) a single animation frame is displayed |
