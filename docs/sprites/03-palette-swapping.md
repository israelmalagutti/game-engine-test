# Palette Swapping (CLUT) -- A Complete Guide

This document is a deep dive into palette swapping, also known as Color Look-Up Table (CLUT) rendering. It covers the historical hardware origins, the color theory behind good palettes, how to implement the technique on modern GPUs with OpenGL 3.3 and GLSL, advanced effects like palette animation, and how this fits into your HD-2D engine project.

If you have ever wondered why classic NES enemies came in red, blue, and green variants that were obviously the same sprite in different colors, this document explains the entire system behind that technique, from the original hardware constraints that made it necessary to the modern shader implementations that make it powerful by choice rather than by limitation.

## Table of Contents
- [1. Historical Foundation -- Hardware Palettes](#1-historical-foundation----hardware-palettes)
  - [The NES PPU](#the-nes-ppu)
  - [The Super NES](#the-super-nes)
  - [Sega Genesis / Mega Drive](#sega-genesis--mega-drive)
  - [The Game Boy](#the-game-boy)
  - [Why Hardware Did This](#why-hardware-did-this)
  - [Creative Constraints and the Aesthetic of an Era](#creative-constraints-and-the-aesthetic-of-an-era)
- [2. Color Theory for Palette Design](#2-color-theory-for-palette-design)
  - [What Makes a Good Palette](#what-makes-a-good-palette)
  - [Shadows Are Not Just Darker](#shadows-are-not-just-darker)
  - [Palette Ramps](#palette-ramps)
  - [How Many Colors a Character Needs](#how-many-colors-a-character-needs)
  - [Standard Palette Databases](#standard-palette-databases)
  - [Designing Palettes That Swap Well](#designing-palettes-that-swap-well)
- [3. Modern GPU Implementation -- Complete Walkthrough](#3-modern-gpu-implementation----complete-walkthrough)
  - [The Core Concept](#the-core-concept)
  - [Step 1: Creating an Indexed-Color Texture](#step-1-creating-an-indexed-color-texture)
  - [Step 2: Creating a Palette Texture](#step-2-creating-a-palette-texture)
  - [Step 3: The Fragment Shader](#step-3-the-fragment-shader)
  - [Step 4: Passing Palette Selection as a Uniform](#step-4-passing-palette-selection-as-a-uniform)
- [4. Palette Animation / Color Cycling](#4-palette-animation--color-cycling)
  - [The Concept](#the-concept)
  - [Classic Uses and Famous Examples](#classic-uses-and-famous-examples)
  - [Mathematical Cycling](#mathematical-cycling)
  - [Ping-Pong Cycling](#ping-pong-cycling)
  - [Per-Range Cycling](#per-range-cycling)
  - [Complete Implementation Walkthrough](#complete-implementation-walkthrough)
  - [Modern Shader Approach](#modern-shader-approach)
- [5. Advanced Techniques](#5-advanced-techniques)
  - [Gradient Mapping](#gradient-mapping)
  - [Palette Lerping](#palette-lerping)
  - [Per-Pixel Palette Selection](#per-pixel-palette-selection)
  - [Palette-Based Effects](#palette-based-effects)
  - [Combining Palette Swap with Multiply Tinting](#combining-palette-swap-with-multiply-tinting)
  - [Dynamic Palette Generation](#dynamic-palette-generation)
- [6. Indexed Sprites vs. Full-Color Sprites -- When to Use Which](#6-indexed-sprites-vs-full-color-sprites----when-to-use-which)
- [7. Real-World Case Studies](#7-real-world-case-studies)
  - [Shovel Knight](#shovel-knight)
  - [Dead Cells](#dead-cells)
  - [Celeste](#celeste)
  - [Hyper Light Drifter](#hyper-light-drifter)
  - [Castlevania (NES/SNES)](#castlevania-nessnes)
- [8. Implementation Architecture](#8-implementation-architecture)
  - [PaletteManager Class](#palettemanager-class)
  - [Integration with the Sprite Rendering Pipeline](#integration-with-the-sprite-rendering-pipeline)
  - [Efficient Batching](#efficient-batching)
  - [Pseudocode for the Complete System](#pseudocode-for-the-complete-system)
- [9. Palette Swapping for Your HD-2D Project](#9-palette-swapping-for-your-hd-2d-project)

---

## 1. Historical Foundation -- Hardware Palettes

To understand palette swapping, you need to understand why it existed in the first place. This was not a stylistic choice. It was an absolute hardware necessity driven by the cost and size of memory in the 1980s and early 1990s.

### The NES PPU

The Nintendo Entertainment System (1983) had a custom chip called the **PPU** (Picture Processing Unit). The PPU handled all graphics rendering independently from the CPU. Here is how its palette system worked.

**The Master Palette.** The NES hardware contained a fixed set of 64 colors (technically 54 unique colors plus duplicates and an unusable black). These were hardwired into the PPU. You could not define custom RGB values. You picked from these 64 entries and that was it.

```
NES Master Palette (simplified representation):

Row 0:  [#7C7C7C] [#0000FC] [#0000BC] [#4428BC] [#940084] [#A80020] [#A81000] [#881400]
Row 1:  [#503000] [#007800] [#006800] [#005800] [#004058] [#000000] [#000000] [#000000]
Row 2:  [#BCBCBC] [#0078F8] [#0058F8] [#6844FC] [#D800CC] [#E40058] [#F83800] [#E45C10]
Row 3:  [#AC7C00] [#00B800] [#00A800] [#00A844] [#008888] [#000000] [#000000] [#000000]
Row 4:  [#F8F8F8] [#3CBCFC] [#6888FC] [#9878F8] [#F878F8] [#F85898] [#F87858] [#FCA044]
Row 5:  [#F8B800] [#B8F818] [#58D854] [#58F898] [#00E8D8] [#787878] [#000000] [#000000]
Row 6:  [#FCFCFC] [#A4E4FC] [#B8B8F8] [#D8B8F8] [#F8B8F8] [#F8A4C0] [#F0D0B0] [#FCE0A8]
Row 7:  [#F8D878] [#D8F878] [#B8F8B8] [#B8F8D8] [#00FCFC] [#F8D8F8] [#000000] [#000000]

64 entries total. The artist picks from ONLY these colors.
No custom RGB. No gradients. These 64 are all you get.
```

**Sprites: 2-bit indices, 4 colors.** Each sprite on the NES was 8x8 pixels. Each pixel was encoded as a **2-bit value** (0, 1, 2, or 3). Two bits can represent four possible values, so each sprite could reference exactly 4 colors from a palette.

```
An 8x8 NES sprite stored in memory:

Each pixel = 2 bits.  Values: 0, 1, 2, 3

  Col: 0  1  2  3  4  5  6  7
      +--+--+--+--+--+--+--+--+
Row 0 | 0| 0| 0| 1| 1| 0| 0| 0|
      +--+--+--+--+--+--+--+--+
Row 1 | 0| 0| 1| 2| 2| 1| 0| 0|
      +--+--+--+--+--+--+--+--+
Row 2 | 0| 1| 2| 3| 3| 2| 1| 0|
      +--+--+--+--+--+--+--+--+
Row 3 | 0| 1| 2| 3| 3| 2| 1| 0|
      +--+--+--+--+--+--+--+--+
Row 4 | 0| 1| 2| 2| 2| 2| 1| 0|
      +--+--+--+--+--+--+--+--+
Row 5 | 0| 0| 1| 2| 2| 1| 0| 0|
      +--+--+--+--+--+--+--+--+
Row 6 | 0| 1| 1| 0| 0| 1| 1| 0|
      +--+--+--+--+--+--+--+--+
Row 7 | 0| 1| 0| 0| 0| 0| 1| 0|
      +--+--+--+--+--+--+--+--+

Total memory for this sprite:
  8 x 8 pixels x 2 bits/pixel = 128 bits = 16 bytes
```

**4 sprite palettes, 4 background palettes.** The NES maintained 8 palettes simultaneously: 4 for sprites and 4 for backgrounds. Each palette contained 4 entries (indices 0-3). Index 0 was always treated as transparent for sprites.

```
NES Sprite Palette Table:

Palette 0:  [transparent] [#0000FC] [#F83800] [#F8F8F8]
Palette 1:  [transparent] [#0000FC] [#00B800] [#F8F8F8]
Palette 2:  [transparent] [#0000FC] [#0078F8] [#F8F8F8]
Palette 3:  [transparent] [#881400] [#FCA044] [#FCFCFC]

Same sprite data + different palette = different colored character:

Palette 0 (red):         Palette 1 (green):       Palette 2 (blue):
   . . . B B . . .          . . . B B . . .          . . . B B . . .
   . . B R R B . .          . . B G G B . .          . . B L L B . .
   . B R W W R B .          . B G W W G B .          . B L W W L B .
   . B R W W R B .          . B G W W G B .          . B L W W L B .
   . B R R R R B .          . B G G G G B .          . B L L L L B .
   . . B R R B . .          . . B G G B . .          . . B L L B . .
   . B B . . B B .          . B B . . B B .          . B B . . B B .
   . B . . . . B .          . B . . . . B .          . B . . . . B .

   (B=blue, R=red,          (B=blue, G=green,        (B=blue, L=light blue,
    W=white)                  W=white)                 W=white)

The sprite pixel data is IDENTICAL. Only the palette table changed.
Zero additional memory cost for three color variants.
```

This is the fundamental insight: the sprite data stores indices, not colors. Changing which colors those indices point to changes the appearance without touching the sprite data at all.

**How palette assignment worked.** Each sprite on screen had a few attribute bits that specified which of the 4 sprite palettes to use. The PPU hardware performed the lookup automatically. When rendering pixel (x, y) of a sprite, the PPU would:

1. Read the 2-bit index from the sprite's tile data
2. Check which palette (0-3) this sprite is assigned to
3. Look up the actual color from the palette table
4. Output that color to the screen

This entire pipeline happened in hardware, in real time, for every visible pixel on every scanline.

### The Super NES

The Super Nintendo (1990) expanded the palette system significantly.

**256-color master palette.** The SNES could display up to 256 simultaneous colors on screen, selected from a total gamut of 32,768 possible colors (15-bit RGB: 5 bits per channel, giving 32 shades per channel).

**8 palettes of 16 colors.** Instead of the NES's 4 colors per palette, the SNES gave each palette 16 entries. With 4-bit indices (values 0-15), each pixel could reference one of 16 colors within its assigned palette.

```
SNES Palette Layout:

Palette 0: [16 colors] -----> Indices 0-15
Palette 1: [16 colors] -----> Indices 0-15
Palette 2: [16 colors] -----> Indices 0-15
Palette 3: [16 colors] -----> Indices 0-15
Palette 4: [16 colors] -----> Indices 0-15
Palette 5: [16 colors] -----> Indices 0-15
Palette 6: [16 colors] -----> Indices 0-15
Palette 7: [16 colors] -----> Indices 0-15

Total: 8 x 16 = 128 palette entries for sprites
       (backgrounds had additional palettes)

Memory per pixel: 4 bits (instead of NES's 2 bits)
Colors per sprite: 16 (instead of NES's 4)

This is why SNES sprites look dramatically more detailed
than NES sprites -- 4x more colors per sprite.
```

The jump from 4 to 16 colors per sprite was enormous for art quality. Suddenly sprites could have smooth shading, detailed faces, and subtle color gradients. But palette swapping worked exactly the same way: reassign a sprite to a different palette row and it changes color instantly, for free.

### Sega Genesis / Mega Drive

The Sega Genesis (1988) had its own approach to palettes.

**512-color gamut.** The Genesis used 9-bit color (3 bits per RGB channel), giving 512 possible colors. This was less than the SNES's 32,768 but still far more than could be displayed simultaneously.

**4 palettes of 16 colors = 64 simultaneous colors.** The Genesis could show 64 colors on screen at once, organized as 4 palettes of 16 entries each. Sprites and backgrounds shared these palettes.

**Palette tricks.** Genesis developers became experts at "palette tricks" -- techniques to squeeze more visual variety from limited palettes:

- **Per-scanline palette changes.** By changing palette entries between scanlines (horizontal lines of the display), developers could effectively use different palettes for the top and bottom of the screen. This was used for split-screen effects and richer backgrounds.
- **Shadow and highlight mode.** The Genesis had a special mode where sprites could darken or brighten the pixels behind them, effectively simulating lighting without using extra palette entries.
- **Water reflection effects.** By modifying palettes mid-frame, underwater sections of the screen could use blue-shifted versions of the same palette.

```
Genesis palette trick -- water reflection:

Top of screen (normal palette):
  Palette entry 5 = (34, 139, 34)   -- forest green
  Palette entry 6 = (139, 69, 19)   -- saddle brown
  Palette entry 7 = (135, 206, 235) -- sky blue

------- water line (mid-screen scanline interrupt) -------

Bottom of screen (modified palette -- same indices, shifted colors):
  Palette entry 5 = (20, 80, 100)   -- blue-green (reflected forest)
  Palette entry 6 = (70, 50, 80)    -- blue-brown (reflected ground)
  Palette entry 7 = (40, 60, 120)   -- deep blue (reflected sky)

Same tile data above and below the water line.
Only the palette changed mid-frame. The "reflection" is free.
```

### The Game Boy

The original Game Boy (1989) is perhaps the most extreme example of palette constraints.

**4 shades.** That is it. The entire display could show exactly 4 shades of greenish-gray (the green tint came from the LCD screen itself, not the hardware -- the values were just luminance):

```
Game Boy "Palette":

Index 0: Lightest (near white)    ████████  #9BBC0F (on actual screen)
Index 1: Light (light gray)       ▓▓▓▓▓▓▓▓  #8BAC0F
Index 2: Dark (dark gray)         ▒▒▒▒▒▒▒▒  #306230
Index 3: Darkest (near black)     ░░░░░░░░  #0F380F

That is the entire color space. Every pixel on screen is one of these 4 values.
```

**How palette mapping created variety.** Even with only 4 shades, the Game Boy had a palette register that let you remap which shade each index pointed to. You could set index 0 to the darkest shade and index 3 to the lightest, effectively inverting the image. Or you could map multiple indices to the same shade, reducing contrast.

```
Default mapping:                Inverted mapping:
  Index 0 -> Lightest             Index 0 -> Darkest
  Index 1 -> Light                Index 1 -> Dark
  Index 2 -> Dark                 Index 2 -> Light
  Index 3 -> Darkest              Index 3 -> Lightest

Reduced contrast mapping:       "Flash white" effect:
  Index 0 -> Lightest             Index 0 -> Lightest
  Index 1 -> Lightest             Index 1 -> Lightest
  Index 2 -> Light                Index 2 -> Lightest
  Index 3 -> Dark                 Index 3 -> Lightest

The "flash white" mapping made all indices display the lightest shade,
creating a flash effect for damage feedback -- all without
changing any sprite data. The palette register change took
a single write instruction.
```

Games like Pokemon used palette manipulation extensively. The screen flash when entering a battle, the fade to black during transitions, and the different Pokemon types appearing in subtly different shades -- all palette manipulation.

### Why Hardware Did This

The fundamental reason is **memory cost.** In the early 1980s, RAM was extraordinarily expensive.

Consider the math for a NES-era display:

```
NES screen resolution: 256 x 240 pixels

If storing full RGB (24-bit color) per pixel:
  256 x 240 x 3 bytes = 184,320 bytes = ~180 KB

  The entire NES had 2 KB of RAM and 2 KB of video RAM.
  180 KB is 90x the entire system's memory. Impossible.

With 2-bit indexed color:
  256 x 240 x 0.25 bytes = 15,360 bytes = ~15 KB

  Still more than available RAM, but tile-based rendering
  meant you only stored unique 8x8 tile patterns (typically
  256 tiles = 4 KB) and a tile map (about 1 KB) that referenced
  which tile goes where. The palette table itself was only 32 bytes
  (8 palettes x 4 entries x 1 byte per entry).

  Total: about 5 KB for the entire screen. Fits in video RAM.
```

The indexed palette approach reduced memory requirements by roughly 36x compared to direct RGB storage. This is the same concept as database normalization in web development -- instead of storing the full color data everywhere, you store a small reference (the index) and look up the full data from a shared table.

### Creative Constraints and the Aesthetic of an Era

These hardware limitations forced artists to develop an entire discipline around working within tight color budgets. This discipline produced techniques and aesthetics that persist today even when the constraints are gone:

**Deliberate color choices.** When you only have 3 colors plus transparent for a sprite, every color must earn its place. NES artists learned to make one color serve double duty (an outline color that is also a shadow color), to use the background color as part of the design, and to choose colors that maximized contrast and readability at small sizes.

**Readable silhouettes.** With so few colors, characters had to be identifiable by shape alone. This pushed artists toward strong, distinctive silhouettes -- a design principle that modern game artists still consider essential.

**Color as information.** Palette swapping was not just a budget shortcut. It became a communication tool. Red enemies are stronger than blue enemies. Gold items are rarer than silver items. A character's palette tells you something about them before any text appears.

**The pixel art aesthetic.** The entire visual language of pixel art -- dithering, limited palettes, careful color ramps, outline styles -- emerged from these constraints. When modern indie games adopt a "retro" aesthetic, they are drawing from a visual tradition that was invented to solve engineering problems.

---

## 2. Color Theory for Palette Design

Understanding color theory is critical for creating palettes that look good and swap well. This section covers what you need to know to design effective game palettes, even if you have never studied art formally.

### What Makes a Good Palette

A good palette is not just a collection of nice-looking colors. It is a system where every color has a role and works in relationship with every other color.

**Hue variety.** A palette needs enough hue diversity to represent different materials (skin, metal, cloth, wood) without so much diversity that nothing feels cohesive. Most game palettes use 4-8 base hues and derive variations from them.

**Value range.** "Value" means lightness/darkness. A palette must span from very light (for highlights) to very dark (for shadows and outlines). If all your colors are mid-tones, the art will look flat and muddy.

```
Good value distribution:            Poor value distribution:

   Lightest  ████ highlight             ████ color A
              ▓▓▓ light midtone          ▓▓▓ color B
              ▒▒▒ midtone               ▒▒▒ color C
              ░░░ dark midtone           ░░░ color D
   Darkest   .... shadow/outline        .... color E

   Left: Full range from light         Right: Everything is mid-tone,
   to dark. Sprites will "pop."        sprites will look flat and
                                       have no depth.
```

**Saturation control.** Pure, fully saturated colors (like #FF0000 red) are visually aggressive. Experienced pixel artists use desaturated versions for large areas and reserve high saturation for small accents (sparks, magic effects, UI highlights).

### Shadows Are Not Just Darker

This is one of the most important principles in color theory for palettes, and one that beginners almost always get wrong.

When you darken a color, your instinct is to just reduce the brightness. A light blue skin highlight becomes a darker blue skin shadow. But in real life, shadows shift in **hue** as well as value.

```
WRONG approach (just darkening):

  Highlight:  (200, 150, 120)  -- peach skin
  Midtone:    (150, 112,  90)  -- darker peach
  Shadow:     (100,  75,  60)  -- even darker peach

  This looks dull and lifeless. Like someone turned down a dimmer switch.

RIGHT approach (hue shifting):

  Highlight:  (230, 180, 140)  -- warm peach (slightly yellow)
  Midtone:    (190, 130, 100)  -- warm-neutral skin
  Shadow:     (120,  70,  80)  -- cool reddish-purple

  The highlight shifts WARM (toward yellow/orange).
  The shadow shifts COOL (toward purple/blue).
  This creates visual richness that the eye reads as "real lighting."
```

This happens in reality because:
- Direct light (sun, lamp) tends to be warm (yellowish)
- Ambient light (sky, reflected environment) tends to be cool (bluish)
- Shadows receive less direct warm light and more ambient cool light
- Therefore shadows are relatively cooler in hue than highlights

The principle also works in reverse (cool highlights, warm shadows) for scenes lit by cool light sources (moonlight, fluorescent lights). The key is that highlights and shadows should **differ in hue**, not just in brightness.

```
Hue-shifting visualized on a color wheel:

            Yellow
              |
    Orange ---+--- Green
      |       |       |
      |   HIGHLIGHT   |     Highlights shift toward warm
      |       |       |
    Red ------+------ Cyan
      |       |       |
      |    SHADOW     |     Shadows shift toward cool
      |       |       |
    Magenta --+--- Blue
              |
            Violet

When designing a ramp for skin:
  Start at the warm side for highlights
  Move through the base hue
  End at the cool side for shadows
```

### Palette Ramps

A **palette ramp** is a sequence of colors organized from lightest to darkest within a single hue family. Each ramp represents one "material" or visual element.

```
Skin ramp (4 colors):
  [#F8D8B0]  [#D8A878]  [#A87848]  [#704028]
   highlight   midtone   shadow     deep shadow
   (warm)      (neutral)  (cool)    (coolest)

Metal ramp (4 colors):
  [#E8E8F0]  [#A0A0B8]  [#606880]  [#303848]
   shine      light      body       shadow
   (cool)     (neutral)  (blue)     (deep blue)

Foliage ramp (3 colors):
  [#A8D870]  [#588830]  [#284010]
   lit leaf    base       shadow
   (yellow)   (green)    (dark green)
```

Ramps typically have 3-5 entries. Fewer than 3 and the shading looks too harsh. More than 5 and you are spending palette entries that could be used for other materials.

### How Many Colors a Character Needs

A well-designed character sprite typically uses 12-16 colors total, distributed across several ramps:

```
Typical character palette budget:

Material          Ramp Size    Colors
--------------------------------------
Skin              4 colors     Highlight, midtone, shadow, deep shadow
Hair              3 colors     Highlight, base, shadow
Eyes              2 colors     Iris, pupil (or outline doubles as pupil)
Clothing (main)   3-4 colors   Highlight, base, shadow, (deep shadow)
Clothing (accent) 2-3 colors   Highlight, base, shadow
Boots/belt        2 colors     Base, shadow
Outline           1-2 colors   Dark outline (may vary: darker near shadows)
Metal/weapon      2-3 colors   Shine, body, shadow
--------------------------------------
Total:           ~15 colors

Index 0 is reserved for transparent.
That leaves 15 slots in a 16-color palette for actual art.
```

This is remarkably efficient. A 32x32 pixel character sprite with 16-color indexing uses 32 x 32 x 4 bits = 512 bytes. A full RGBA version would be 32 x 32 x 4 bytes = 4,096 bytes. The indexed version is 8x smaller.

### Standard Palette Databases

The pixel art community has developed standardized palettes that are designed for game art. These are worth knowing about:

**DB32 (DawnBringer 32).** A 32-color palette designed by the pixel artist DawnBringer. It provides a balanced set of hues with good value distribution. Many pixel art games and game jams use it as a starting point.

```
DB32 palette structure (simplified grouping):

Warm ramp:     [cream] [peach] [orange] [red-brown] [dark brown]
Cool ramp:     [light cyan] [sky blue] [medium blue] [dark blue] [navy]
Green ramp:    [lime] [grass green] [forest green] [dark green]
Purple ramp:   [lavender] [violet] [dark purple]
Neutral ramp:  [white] [light gray] [medium gray] [dark gray] [black]
Accent colors: [bright red] [yellow] [magenta]
```

**Lospec.** A website (lospec.com) that hosts thousands of curated pixel art palettes. You can search by number of colors, style, and tag. It is an invaluable resource for finding palettes that suit your game's mood.

**PICO-8 palette.** The PICO-8 fantasy console uses a fixed 16-color palette that has become iconic. Many game jam games and indie titles use it because its carefully chosen 16 colors can represent a surprisingly wide range of subjects.

### Designing Palettes That Swap Well

Not all palettes are created equal when it comes to swapping. If you want multiple color variants of the same sprite to all look good, the **palette structure** matters as much as the individual colors.

**The rule:** each palette entry should serve the same **structural role** across all variants. Entry 3 should always be "the midtone of the main color," whether that main color is red, blue, or green.

```
Palette designed for swapping:

Slot:   0     1       2       3       4       5     6       7
Role: trans  outline  shadow  mid    light  accent  skin-s  skin-l

Fire variant:
        --    #301008  #802010  #C04020  #F08040  #FFD040  #906030  #D0A070

Ice variant:
        --    #081830  #103060  #2060A0  #40A0E0  #80E0FF  #305090  #70A0D0

Poison variant:
        --    #082008  #106010  #20A020  #60E040  #C0FF60  #306030  #70D070

Notice: every variant has the same progression from dark to light.
The outline is always the darkest. The light tone is always the brightest.
The structural relationships are preserved. Only the hues change.
```

**Bad palette for swapping:** a palette where the roles are inconsistent. If your red variant uses slot 3 for the skin midtone but your blue variant uses slot 3 for the clothing highlight, swapping palettes will produce nonsensical color assignments.

**Design tip:** organize your palette by brightness, not by material. Slots 1-2 are always the darkest colors. Slots 3-4 are midtones. Slots 5-6 are highlights. Within each brightness band, you can have different hues, but the value structure stays constant across variants.

---

## 3. Modern GPU Implementation -- Complete Walkthrough

Now that you understand the history and the color theory, let us build the actual GPU implementation. This section walks through every step required to get palette swapping working in your OpenGL 3.3 engine.

### The Core Concept

The pipeline has three components:

```
The Palette Swap Pipeline:

                  Index Texture                  Palette Texture
                  (your sprite)                  (color lookup)
                  +----------+                   +--+--+--+--+--+--+
                  | 0 1 3 3 1|                   | C0 C1 C2 C3 C4 C5|  Row 0 (default)
                  | 1 3 5 5 3|                   | C0 C1 C2 C3 C4 C5|  Row 1 (fire)
                  | 0 3 5 4 3|                   | C0 C1 C2 C3 C4 C5|  Row 2 (ice)
                  | 0 1 3 3 1|                   | C0 C1 C2 C3 C4 C5|  Row 3 (poison)
                  +----------+                   +--+--+--+--+--+--+
                        |                               |
                        |    Fragment Shader             |
                        |    +-------------------+      |
                        +--->| 1. Read index      |      |
                             | 2. Use index as    |<-----+
                             |    x-coordinate    |
                             |    into palette    |
                             | 3. Output color    |
                             +-------------------+
                                     |
                                     v
                              Final pixel color
                              on screen
```

This is conceptually identical to how CSS custom properties work in web development. Imagine you have a CSS class that says `color: var(--primary)`. The element does not know what color it is. It stores a **reference** (the variable name), and the actual color is resolved at render time from a lookup table (the CSS custom property definitions). Change `--primary` and every element referencing it changes color. That is palette swapping.

### Step 1: Creating an Indexed-Color Texture

An indexed-color texture stores palette indices instead of actual RGB colors. Each pixel says "I am color number N" rather than "I am this specific shade of red."

**Encoding indices in the red channel.** Since OpenGL textures are RGBA, you use one channel to carry the index. The red channel is conventional:

```
Standard RGBA pixel (what you currently use in your Texture class):
  R = 180, G = 50, B = 50, A = 255    -- this IS the color (a red)

Indexed pixel (for palette swapping):
  R = 5, G = 0, B = 0, A = 255        -- this is NOT a color
                                        -- it means "look up entry 5 in the palette"

Transparent indexed pixel:
  R = 0, G = 0, B = 0, A = 0          -- alpha = 0 means transparent
                                        -- the index does not matter
```

**Why GL_NEAREST filtering is mandatory.** Your current Texture class already uses `GL_NEAREST`, which is correct. But understand why this is not optional for indexed textures:

```
What GL_LINEAR does (bilinear interpolation):

Pixel values:    3    5
When sampling between them at 50%, GL_LINEAR returns:
  (3 + 5) / 2 = 4

For normal textures, this is smooth blending. Nice.
For indexed textures, this is CATASTROPHIC.

Index 3 = red,  Index 5 = green,  Index 4 = blue

You wanted either red or green. You got blue -- a completely
unrelated color from a different palette entry.

GL_NEAREST returns the nearest texel without interpolation.
Between index 3 and index 5, it returns either 3 or 5.
You always get a valid palette index. Always.
```

**Texture format: GL_R8 vs GL_RGBA8.** You have two options for the index texture format:

```
Option A: GL_R8 (single-channel, 8-bit)
  - 1 byte per pixel (just the index)
  - More memory efficient
  - Transparency must be encoded differently (index 0 = transparent)
  - OpenGL code:
      glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                   width, height, 0,
                   GL_RED, GL_UNSIGNED_BYTE, pixelData);

Option B: GL_RGBA8 (four-channel, 8-bit each)
  - 4 bytes per pixel (index in R, alpha in A, G and B unused)
  - Uses 4x the memory of GL_R8
  - Transparency uses the alpha channel (familiar, standard)
  - Works with your existing Texture class without modification
  - OpenGL code:
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                   width, height, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, pixelData);

Recommendation: Start with GL_RGBA8 because it works with your existing
Texture class and SDL_image loading pipeline. Move to GL_R8 later
if memory becomes a concern (it won't for a 2D game).
```

**How Aseprite's indexed color mode maps to this pipeline.** Aseprite (the standard pixel art editor) has an "Indexed" color mode. In this mode, each pixel IS a palette index internally. The palette is shown as a separate panel, and when you paint, you are assigning indices, not colors.

The workflow for creating indexed sprites:

```
1. In Aseprite: New File -> Color Mode: Indexed
2. Design your palette in the palette panel (e.g., 16 colors)
3. Draw your sprite using only those palette colors
4. Export the palette as a .png (1 row of colored pixels)
5. Export the sprite as a .png

   IMPORTANT: When exporting the sprite, you need a script or
   post-processing step to convert the indexed image into the
   format OpenGL expects (index values in the R channel).

   Aseprite's "indexed" PNG export stores indices in a way
   that standard image loaders (like SDL_image) will
   automatically convert back to RGBA colors.

   You have two options:
   a. Write a converter tool that reads the indexed PNG
      and outputs R=index, G=0, B=0, A=alpha
   b. Use a color-matching approach in the shader
      (match the sampled color to the closest palette entry)

   Option (a) gives you a true indexed texture.
   Option (b) is a simpler workflow but slower at runtime.
```

### Step 2: Creating a Palette Texture

The palette texture holds the actual colors. It is a small texture where each pixel IS a color.

**1D layout: one row of colors (one palette).**

```
A 16-color palette as a 16x1 texture:

Pixel position:  0      1      2      3    ...   15
Color stored:   black  d.brn  brown  tan   ...  white

Texture dimensions: 16 pixels wide, 1 pixel tall
Memory: 16 x 1 x 4 bytes (RGBA) = 64 bytes
```

**2D layout: multiple rows (multiple palettes).**

```
A palette texture with 4 variants, 16 colors each:

          Col: 0      1      2      3      4    ...   15
              +------+------+------+------+------+---+------+
Row 0:        |trans | blk  |d.red | red  |l.red |...|white |  Default
              +------+------+------+------+------+---+------+
Row 1:        |trans | blk  |d.blue| blue |l.blue|...|white |  Ice variant
              +------+------+------+------+------+---+------+
Row 2:        |trans | blk  |d.grn |green |l.grn |...|white |  Forest variant
              +------+------+------+------+------+---+------+
Row 3:        |trans | blk  |d.purp|purple|l.purp|...|white |  Shadow variant
              +------+------+------+------+------+---+------+

Texture dimensions: 16 pixels wide, 4 pixels tall
Memory: 16 x 4 x 4 bytes = 256 bytes

To select the "ice" variant, sample from row 1.
To select the "shadow" variant, sample from row 3.
The uniform paletteRow tells the shader which row to use.
```

**Critical texture settings for the palette texture.** The palette texture needs specific OpenGL parameters:

```
Filtering: GL_NEAREST
  You MUST NOT interpolate between palette colors.
  If index 3 is red and index 4 is blue, sampling between them
  must NOT produce purple. GL_NEAREST ensures you get one or the other.

Wrapping: GL_CLAMP_TO_EDGE
  If a UV coordinate lands outside [0, 1] due to floating-point
  imprecision, you want it clamped to the nearest edge pixel,
  not wrapped around to the other side of the palette.
  This prevents sampling a color from the wrong end of the palette.

Both of these are already set in your Texture class, so your
existing texture loading code would work for palette textures.
```

**Creating the palette texture in code.** You can create a palette texture from raw pixel data without loading an image file:

```cpp
// Create a palette texture from raw color data
// Each color is 4 bytes: R, G, B, A

// Define a 16-color palette (one row)
unsigned char paletteData[] = {
    // Index 0: transparent
    0, 0, 0, 0,
    // Index 1: black outline
    20, 12, 8, 255,
    // Index 2: dark shadow
    60, 30, 20, 255,
    // Index 3: shadow
    100, 50, 40, 255,
    // Index 4: midtone
    160, 90, 60, 255,
    // Index 5: light midtone
    200, 140, 100, 255,
    // Index 6: highlight
    240, 200, 160, 255,
    // Index 7: bright highlight
    255, 240, 220, 255,
    // Indices 8-15: additional colors for hair, clothing, etc.
    80, 40, 30, 255,    // Index 8: dark hair
    120, 70, 50, 255,   // Index 9: hair midtone
    180, 120, 80, 255,  // Index 10: hair highlight
    30, 50, 100, 255,   // Index 11: dark clothing
    50, 80, 150, 255,   // Index 12: clothing midtone
    80, 120, 200, 255,  // Index 13: clothing highlight
    40, 40, 50, 255,    // Index 14: dark metal
    100, 100, 120, 255, // Index 15: metal highlight
};

GLuint paletteTextureID;
glGenTextures(1, &paletteTextureID);
glBindTexture(GL_TEXTURE_2D, paletteTextureID);

// Set filtering to NEAREST -- no interpolation between colors
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

// Set wrapping to CLAMP_TO_EDGE -- prevent bleeding at palette edges
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

// Upload the palette data
// Width = 16 (colors per palette), Height = 1 (one palette)
glTexImage2D(
    GL_TEXTURE_2D,
    0,              // mipmap level 0
    GL_RGBA,        // internal format
    16,             // width: 16 colors
    1,              // height: 1 palette row
    0,              // border (must be 0)
    GL_RGBA,        // source format
    GL_UNSIGNED_BYTE,
    paletteData     // pointer to pixel data
);

glBindTexture(GL_TEXTURE_2D, 0);
```

For a multi-palette texture (multiple rows), increase the height and provide data for all rows:

```cpp
// Multi-palette: 16 colors x 4 variants = 16 x 4 texture
// paletteData would be 16 * 4 * 4 bytes = 256 bytes
// (16 colors per row * 4 rows * 4 bytes per color)

int paletteWidth = 16;    // colors per palette
int paletteCount = 4;     // number of palette variants

glTexImage2D(
    GL_TEXTURE_2D,
    0,
    GL_RGBA,
    paletteWidth,           // width: colors per palette
    paletteCount,           // height: number of palettes
    0,
    GL_RGBA,
    GL_UNSIGNED_BYTE,
    multiPaletteData        // all palette rows, one after another
);
```

### Step 3: The Fragment Shader

This is where the palette lookup happens. Every line is explained in detail.

**Complete palette swap fragment shader:**

```glsl
#version 330 core

// Texture coordinate from the vertex shader, interpolated across the quad
in vec2 TexCoord;

// The final color output for this pixel
out vec4 FragColor;

// The index texture: stores palette indices in the R channel.
// Bound to texture unit 0.
uniform sampler2D indexTexture;

// The palette texture: a 2D texture where each row is a different palette.
// Bound to texture unit 1.
uniform sampler2D paletteTexture;

// Which palette row to use (0 = first row, 1 = second row, etc.)
// This is a float because we need it for UV math, but conceptually it is an integer.
uniform float paletteRow;

// How many colors are in each palette row (e.g., 16.0 for a 16-color palette).
uniform float paletteSize;

// How many palette rows exist in the palette texture (e.g., 4.0 for 4 variants).
uniform float paletteCount;

void main() {
    // STEP 1: Sample the index texture at the current UV coordinate.
    //
    // This reads the pixel from the sprite's index texture.
    // The R channel contains the palette index (0-255).
    // The A channel contains the alpha (0 = transparent, 255 = opaque).
    //
    // OpenGL normalizes unsigned byte values to [0.0, 1.0] when sampling,
    // so R=5 in the texture becomes 5.0/255.0 = 0.0196... in the shader.
    //
    vec4 indexSample = texture(indexTexture, TexCoord);

    // STEP 2: Handle transparency.
    //
    // If the alpha is near zero, this pixel is transparent.
    // Discard it so it does not write to the framebuffer at all.
    // This is the same transparency check your current sprite.frag would use.
    //
    if (indexSample.a < 0.01) {
        discard;
    }

    // STEP 3: Extract the palette index from the red channel.
    //
    // The red channel was normalized to [0.0, 1.0] by OpenGL.
    // Multiply by 255.0 to recover the original integer index (0-255).
    // Round to the nearest integer to avoid floating-point imprecision.
    //
    // Example: if the pixel had R=5 in the texture:
    //   indexSample.r = 5.0 / 255.0 = 0.019607...
    //   index = round(0.019607 * 255.0) = round(5.0) = 5.0
    //
    float index = round(indexSample.r * 255.0);

    // STEP 4: Calculate the U coordinate (horizontal position) in the palette texture.
    //
    // We want to sample the CENTER of the texel at position 'index' in the palette row.
    //
    // A 16-wide palette texture has texel centers at:
    //   Texel 0 center: (0 + 0.5) / 16 = 0.03125
    //   Texel 1 center: (1 + 0.5) / 16 = 0.09375
    //   Texel 2 center: (2 + 0.5) / 16 = 0.15625
    //   ...
    //   Texel 15 center: (15 + 0.5) / 16 = 0.96875
    //
    // The +0.5 is the HALF-TEXEL OFFSET. Without it, you sample at the left edge
    // of the texel, where GL_NEAREST might pick either this texel or the one to
    // its left due to floating-point imprecision. Sampling at the center guarantees
    // you hit the correct texel.
    //
    // This is the same texel-center sampling technique described in the texture atlas
    // documentation (10-texture-atlas.md) for preventing tile bleeding.
    //
    float u = (index + 0.5) / paletteSize;

    // STEP 5: Calculate the V coordinate (vertical position) in the palette texture.
    //
    // Same half-texel offset logic, but for selecting the palette row.
    //
    // A 4-row palette texture has row centers at:
    //   Row 0 center: (0 + 0.5) / 4 = 0.125
    //   Row 1 center: (1 + 0.5) / 4 = 0.375
    //   Row 2 center: (2 + 0.5) / 4 = 0.625
    //   Row 3 center: (3 + 0.5) / 4 = 0.875
    //
    float v = (paletteRow + 0.5) / paletteCount;

    // STEP 6: Sample the actual color from the palette texture.
    //
    // At UV = (u, v), we read the color that this index maps to
    // in the selected palette.
    //
    vec4 color = texture(paletteTexture, vec2(u, v));

    // STEP 7: Output the final color.
    //
    // The palette entry might have its own alpha (for semi-transparent effects),
    // or you might always want full opacity for opaque indexed pixels.
    //
    FragColor = color;
}
```

**The half-texel offset explained visually:**

```
A palette texture with 4 colors (4 texels wide):

Texel boundaries in UV space:
  |---0---|---1---|---2---|---3---|
  0     0.25    0.5    0.75     1.0

Texel centers (where you SHOULD sample):
      0.125   0.375   0.625   0.875

Without +0.5 offset, index 1 maps to:
  u = 1 / 4 = 0.25  <-- RIGHT ON THE BOUNDARY between texel 0 and 1
  Due to floating-point error, GPU might sample texel 0 or texel 1.
  Result: flickering colors, wrong palette entries.

With +0.5 offset, index 1 maps to:
  u = (1 + 0.5) / 4 = 0.375  <-- DEAD CENTER of texel 1
  GPU will always sample texel 1. No ambiguity.
```

**Handling the edge case where index 0 means transparent.** If you use GL_R8 (single-channel) format for the index texture, you do not have a separate alpha channel. In that case, you must designate one index value (typically 0) as "transparent":

```glsl
// Alternative transparency handling when using GL_R8 format:
// Index 0 is reserved for "transparent"

vec4 indexSample = texture(indexTexture, TexCoord);
float index = round(indexSample.r * 255.0);

// If index is 0, this pixel is transparent
if (index < 0.5) {
    discard;
}

// Proceed with palette lookup for indices 1-255
float u = (index + 0.5) / paletteSize;
float v = (paletteRow + 0.5) / paletteCount;
vec4 color = texture(paletteTexture, vec2(u, v));
FragColor = color;
```

### Step 4: Passing Palette Selection as a Uniform

On the C++ side, you set the `paletteRow` uniform to select which palette variant to use. This is conceptually identical to how you already pass `spriteTexture` as a uniform -- you are just adding a few more uniforms.

```cpp
// In your sprite rendering code (conceptual, fits your engine's architecture):

void renderPaletteSwappedSprite(
    Shader& paletteShader,
    const Camera& camera,
    Texture* indexTexture,      // the indexed sprite texture
    GLuint paletteTextureID,    // the palette texture
    int selectedPalette,        // which palette variant (0, 1, 2, ...)
    int paletteSize,            // colors per palette (e.g., 16)
    int paletteCount            // total palette variants (e.g., 4)
) {
    paletteShader.use();

    // Set projection and view matrices (same as your current Sprite::draw)
    int viewportWidth = camera.getViewportWidth();
    int viewportHeight = camera.getViewportHeight();
    glm::mat4 projection = glm::ortho(
        0.0f, static_cast<float>(viewportWidth),
        static_cast<float>(viewportHeight), 0.0f
    );
    glm::mat4 view = camera.getViewMatrix();

    paletteShader.setMat4("projection", projection);
    paletteShader.setMat4("view", view);
    // Set model matrix (position, scale -- same as Sprite::draw)
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(posX, posY, 0.0f));
    model = glm::scale(model, glm::vec3(spriteWidth, spriteHeight, 1.0f));
    paletteShader.setMat4("model", model);

    // Bind the index texture to texture unit 0
    glActiveTexture(GL_TEXTURE0);
    indexTexture->bind(0);
    paletteShader.setInt("indexTexture", 0);

    // Bind the palette texture to texture unit 1
    // This uses a DIFFERENT texture unit than the index texture.
    // OpenGL supports at least 16 texture units (GL_TEXTURE0 through GL_TEXTURE15).
    // You are already familiar with texture unit 0 from your sprite rendering.
    // This is the same concept, just using a second slot.
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, paletteTextureID);
    paletteShader.setInt("paletteTexture", 1);

    // Set palette selection uniforms
    paletteShader.setFloat("paletteRow", static_cast<float>(selectedPalette));
    paletteShader.setFloat("paletteSize", static_cast<float>(paletteSize));
    paletteShader.setFloat("paletteCount", static_cast<float>(paletteCount));

    // Draw the quad (same as your current sprite rendering)
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Unbind textures
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    indexTexture->unbind();
}
```

**The multi-texture unit concept, explained for web developers.** If textures are confusing with multiple units, think of it this way:

```
Web analogy:

In CSS, you can apply multiple background images to one element:

  background-image: url('sprite.png'), url('overlay.png');

The browser composites them together. Each image is like a "texture unit."

In OpenGL:
  GL_TEXTURE0 = the first "slot" for a texture
  GL_TEXTURE1 = the second "slot"
  ...up to GL_TEXTURE15 (at minimum)

You bind a different texture to each slot, and the shader can
sample from any slot it wants. The sampler2D uniform's integer value
tells the shader which slot to read from.

  uniform sampler2D indexTexture;   // set to 0 -> reads from GL_TEXTURE0
  uniform sampler2D paletteTexture; // set to 1 -> reads from GL_TEXTURE1
```

---

## 4. Palette Animation / Color Cycling

Palette animation is one of the most elegant tricks in game graphics history. The idea: instead of animating the sprite itself (changing pixel positions), you animate the **palette** (changing what colors the indices map to). The sprite data stays completely static while the colors flow through it.

### The Concept

Imagine a waterfall sprite. Every "water" pixel has the same index -- say, indices 8, 9, 10, 11 for four shades of blue from dark to light. Now, each frame, you shift which shade each index points to:

```
Frame 0:            Frame 1:            Frame 2:            Frame 3:
Index 8  -> dark    Index 8  -> med     Index 8  -> light   Index 8  -> white
Index 9  -> med     Index 9  -> light   Index 9  -> white   Index 9  -> dark
Index 10 -> light   Index 10 -> white   Index 10 -> dark    Index 10 -> med
Index 11 -> white   Index 11 -> dark    Index 11 -> med     Index 11 -> light

The sprite pixel data never changes. The COLORS rotate through the indices.

Visually, this creates the illusion of water flowing downward, because
colors that were at the top of the waterfall (light) shift to be at
positions where darker colors were, creating apparent motion.
```

### Classic Uses and Famous Examples

**Flowing water.** The most common use. Blue shades cycle through water-assigned indices, creating rippling motion with zero animation frames.

**Fire and lava.** Red/orange/yellow colors cycle through fire indices. Because fire is chaotic, even simple cycling looks convincingly animated.

**Glowing and pulsing.** A magic item can pulse by cycling its colors between bright and dim variants. This draws the player's eye without needing animation frames.

**Rainbow effects.** Cycling all hues through a set of indices creates rainbow shimmer. Used for rare items, power-ups, and invincibility states.

**LucasArts adventure games -- Monkey Island.** The Secret of Monkey Island (1990) and its sequel made legendary use of color cycling. The ocean at sunset uses palette cycling to create the impression of waves glittering with reflected sunlight. The lava on Monkey Island pulses and flows. The sunset sky transitions through colors. All of this is palette animation -- not a single pixel of the background image changes.

```
Monkey Island sunset ocean (simplified):

The ocean pixels use indices 20-27 (8 shades of blue/orange).

Static sprite data:
  .  .  20 21 22 23 24 25 26 27 20 21 22 .  .
  .  21 22 23 24 25 26 27 20 21 22 23 24 25 .
  22 23 24 25 26 27 20 21 22 23 24 25 26 27 22

By cycling palette entries 20-27, the entire ocean surface
appears to undulate and shimmer. Hundreds of "water" pixels
animate simultaneously by changing 8 palette entries per frame.

Compare to frame-by-frame:
  Frame-by-frame would require redrawing the entire ocean background
  for each animation frame. At 320x200 pixels, that's 64 KB per frame.
  With 30 animation frames, that's 1.92 MB -- impossible on 1990 hardware.

  Palette cycling: 8 colors x 4 bytes = 32 bytes changed per frame.
  That's 60,000x less data.
```

### Mathematical Cycling

The basic cycling formula shifts each palette entry by an offset that increases over time:

```
palette[i] = basePalette[(i + offset) % rangeSize]

Where:
  i          = the palette index being resolved
  offset     = the current animation offset (increases each frame)
  rangeSize  = how many entries participate in the cycle
  basePalette = the original, unmodified palette colors
```

Example with a 4-entry water cycle:

```
basePalette for indices 8-11:
  Entry 0: dark blue    (#103060)
  Entry 1: medium blue  (#2060A0)
  Entry 2: light blue   (#40A0E0)
  Entry 3: white-blue   (#80E0FF)

offset = 0:  dark, med, light, white   (original order)
offset = 1:  med, light, white, dark   (shifted right by 1)
offset = 2:  light, white, dark, med   (shifted right by 2)
offset = 3:  white, dark, med, light   (shifted right by 3)
offset = 4:  same as offset 0          (wrapped around)
```

### Ping-Pong Cycling

Instead of wrapping around (offset 3 jumps back to offset 0), ping-pong cycling reverses direction. This creates a pulsing or breathing effect rather than a flowing one.

```
Linear cycling (wrapping):
  0 -> 1 -> 2 -> 3 -> 0 -> 1 -> 2 -> 3 -> 0 -> ...
  Good for: flowing water, scrolling patterns

Ping-pong cycling (bouncing):
  0 -> 1 -> 2 -> 3 -> 2 -> 1 -> 0 -> 1 -> 2 -> 3 -> ...
  Good for: glowing, pulsing, breathing, charging up

Implementation:
  totalPhases = (rangeSize - 1) * 2;  // for 4 entries: 6 phases
  phase = frameCounter % totalPhases;
  if (phase >= rangeSize) {
      offset = totalPhases - phase;   // reverse direction
  } else {
      offset = phase;                 // forward direction
  }
```

### Per-Range Cycling

In most sprites, only certain indices should cycle. The skin and clothing colors should stay fixed while the water or fire colors animate.

```
A 16-color palette with selective cycling:

Index:  0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
Role:  trans outl shad mid  lite acc  skin skin WATR WATR WATR WATR fire fire fire fire
       |--- static, never cycle ---|  |- cycle range A -|  |-- cycle range B --|

Range A (water, indices 8-11): cycles at 8 fps
Range B (fire, indices 12-15): cycles at 12 fps (fire is faster than water)
Indices 0-7: never modified

Each range has its own:
  - Start index and end index
  - Cycle speed (frames per shift)
  - Cycle direction (forward, reverse, ping-pong)
```

### Complete Implementation Walkthrough

Here is a complete implementation of palette cycling using CPU-side palette modification. This approach modifies the palette texture data and re-uploads it to the GPU each frame.

```cpp
// PaletteCycleRange defines one range of palette entries to cycle
struct PaletteCycleRange {
    int startIndex;          // first palette entry in the range (inclusive)
    int endIndex;            // last palette entry in the range (inclusive)
    float cycleSpeed;        // cycles per second
    bool pingPong;           // true = bounce, false = wrap
    float accumulator;       // internal: accumulated time

    // Compute the current offset for this range given elapsed time
    int computeOffset(float deltaTime) {
        accumulator += deltaTime;
        int rangeSize = endIndex - startIndex + 1;

        // How many shifts have elapsed
        float shiftsPerSecond = cycleSpeed;
        float shiftInterval = 1.0f / shiftsPerSecond;

        // Wrap accumulator to avoid floating-point overflow over long play sessions
        float totalCycleDuration;
        if (pingPong) {
            totalCycleDuration = shiftInterval * (rangeSize - 1) * 2;
        } else {
            totalCycleDuration = shiftInterval * rangeSize;
        }
        if (accumulator >= totalCycleDuration) {
            accumulator -= totalCycleDuration;
        }

        int rawOffset = static_cast<int>(accumulator / shiftInterval);

        if (pingPong) {
            int totalPhases = (rangeSize - 1) * 2;
            int phase = rawOffset % totalPhases;
            if (phase >= rangeSize) {
                return totalPhases - phase;
            }
            return phase;
        } else {
            return rawOffset % rangeSize;
        }
    }
};

// Apply cycling to a palette's color data
void applyCycling(
    unsigned char* basePalette,   // original palette colors (RGBA, 4 bytes per entry)
    unsigned char* outputPalette, // output palette with cycling applied
    int paletteSize,              // total entries in palette
    PaletteCycleRange* ranges,    // array of cycle ranges
    int rangeCount,               // number of cycle ranges
    float deltaTime               // time since last frame
) {
    // Start with a copy of the base palette
    std::memcpy(outputPalette, basePalette, paletteSize * 4);

    // Apply each cycle range
    for (int r = 0; r < rangeCount; r++) {
        int offset = ranges[r].computeOffset(deltaTime);
        int rangeSize = ranges[r].endIndex - ranges[r].startIndex + 1;

        for (int i = ranges[r].startIndex; i <= ranges[r].endIndex; i++) {
            // Map this index to its shifted source in the base palette
            int localIndex = i - ranges[r].startIndex;
            int sourceLocal = (localIndex + offset) % rangeSize;
            int sourceIndex = ranges[r].startIndex + sourceLocal;

            // Copy RGBA (4 bytes) from shifted source to output
            outputPalette[i * 4 + 0] = basePalette[sourceIndex * 4 + 0]; // R
            outputPalette[i * 4 + 1] = basePalette[sourceIndex * 4 + 1]; // G
            outputPalette[i * 4 + 2] = basePalette[sourceIndex * 4 + 2]; // B
            outputPalette[i * 4 + 3] = basePalette[sourceIndex * 4 + 3]; // A
        }
    }

    // Upload the modified palette to the GPU
    glBindTexture(GL_TEXTURE_2D, paletteTextureID);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,              // mipmap level 0
        0,              // x offset (start of row)
        0,              // y offset (which row -- 0 for first palette)
        paletteSize,    // width: all entries in the row
        1,              // height: one row
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        outputPalette
    );
    glBindTexture(GL_TEXTURE_2D, 0);
}
```

### Modern Shader Approach

Instead of modifying and re-uploading the palette texture every frame, you can perform the cycling math entirely in the fragment shader. This avoids the CPU-to-GPU data transfer each frame.

```glsl
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D indexTexture;
uniform sampler2D paletteTexture;
uniform float paletteRow;
uniform float paletteSize;
uniform float paletteCount;

// Cycling parameters
uniform float cycleTime;       // total elapsed time (passed from CPU each frame)
uniform float cycleRangeStart; // first index in the cycle range (e.g., 8.0)
uniform float cycleRangeEnd;   // last index in the cycle range (e.g., 11.0)
uniform float cycleSpeed;      // shifts per second

void main() {
    vec4 indexSample = texture(indexTexture, TexCoord);

    if (indexSample.a < 0.01) {
        discard;
    }

    float index = round(indexSample.r * 255.0);

    // Check if this index falls within the cycling range
    if (index >= cycleRangeStart && index <= cycleRangeEnd) {
        // Compute cycle offset
        float rangeSize = cycleRangeEnd - cycleRangeStart + 1.0;
        float offset = floor(cycleTime * cycleSpeed);
        offset = mod(offset, rangeSize);

        // Shift the index within the range
        float localIndex = index - cycleRangeStart;
        float shiftedLocal = mod(localIndex + offset, rangeSize);
        index = cycleRangeStart + shiftedLocal;
    }

    // Standard palette lookup with the (possibly shifted) index
    float u = (index + 0.5) / paletteSize;
    float v = (paletteRow + 0.5) / paletteCount;
    vec4 color = texture(paletteTexture, vec2(u, v));

    FragColor = color;
}
```

The shader approach is more efficient (no texture re-upload) but is limited to simpler cycling patterns. For complex per-range cycling with multiple ranges, the CPU approach gives you more control. For a single cycling range per sprite, the shader approach is cleaner.

---

## 5. Advanced Techniques

Once you have the basic palette swap system working, you can build more sophisticated effects on top of it.

### Gradient Mapping

Instead of discrete palette entries, you can use a **gradient texture** -- a smooth, continuous ramp of colors. The index value (0.0 to 1.0, normalized) becomes a position along this gradient.

```
Discrete palette (16 entries):
  [C0][C1][C2][C3][C4][C5][C6][C7][C8][C9]....[C15]
  Hard boundaries between colors. Classic pixel art look.

Gradient map (smooth):
  [dark brown ~~~ warm brown ~~~ tan ~~~ cream ~~~ white]
  Smooth transitions. Painterly or HD look.

The fragment shader is nearly identical. Instead of
  u = (index + 0.5) / paletteSize
you use the raw normalized value:
  u = indexSample.r    // direct mapping, 0.0 = left edge, 1.0 = right edge
```

The gradient texture is typically wider (64 or 256 pixels) to give smooth interpolation. And critically, the gradient texture CAN use `GL_LINEAR` filtering -- you **want** blending between adjacent gradient samples for smooth results.

```glsl
// Gradient mapping fragment shader
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D indexTexture;    // stores luminance values (0.0 = dark, 1.0 = light)
uniform sampler2D gradientTexture; // smooth gradient (wider texture, GL_LINEAR filtering)
uniform float gradientRow;         // which gradient to use (for multiple gradient options)
uniform float gradientCount;       // total number of gradient rows

void main() {
    vec4 indexSample = texture(indexTexture, TexCoord);

    if (indexSample.a < 0.01) {
        discard;
    }

    // Use the red channel directly as position along the gradient
    // No rounding needed -- we WANT smooth values here
    float position = indexSample.r;

    // Sample the gradient
    float v = (gradientRow + 0.5) / gradientCount;
    vec4 color = texture(gradientTexture, vec2(position, v));

    FragColor = color;
}
```

Gradient mapping is particularly useful for the HD-2D aesthetic. You can apply it to lighting: a sprite's brightness channel maps through a gradient that shifts from cool shadow colors to warm highlight colors, producing the kind of rich color variation seen in Octopath Traveler.

### Palette Lerping

Smoothly interpolating between two palettes over time creates elegant transitions. Examples:

- **Day/night cycle:** gradually shift from a daytime palette (warm, bright) to a nighttime palette (cool, dark)
- **Poison wearing off:** shift from a green-tinted palette back to the normal palette
- **Entering a new biome:** crossfade from forest palette to desert palette
- **Seasonal changes:** spring palette to summer palette as in-game days pass

```glsl
// Palette lerping fragment shader
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D indexTexture;
uniform sampler2D paletteTexture;
uniform float paletteSize;
uniform float paletteCount;

// Two palette rows to blend between
uniform float paletteRowA;     // source palette (e.g., day palette, row 0)
uniform float paletteRowB;     // target palette (e.g., night palette, row 1)
uniform float blendFactor;     // 0.0 = 100% palette A, 1.0 = 100% palette B

void main() {
    vec4 indexSample = texture(indexTexture, TexCoord);

    if (indexSample.a < 0.01) {
        discard;
    }

    float index = round(indexSample.r * 255.0);
    float u = (index + 0.5) / paletteSize;

    // Sample BOTH palettes at the same index
    float vA = (paletteRowA + 0.5) / paletteCount;
    float vB = (paletteRowB + 0.5) / paletteCount;
    vec4 colorA = texture(paletteTexture, vec2(u, vA));
    vec4 colorB = texture(paletteTexture, vec2(u, vB));

    // Linearly interpolate between the two palette colors
    // mix() is GLSL's built-in lerp function
    // mix(a, b, t) = a * (1 - t) + b * t
    vec4 finalColor = mix(colorA, colorB, blendFactor);

    FragColor = finalColor;
}
```

On the CPU side, you would update `blendFactor` over time:

```cpp
// Smooth palette transition over 2 seconds
float transitionDuration = 2.0f;
float transitionProgress = 0.0f;  // starts at 0, reaches 1 when complete

void update(float deltaTime) {
    if (transitioning) {
        transitionProgress += deltaTime / transitionDuration;
        if (transitionProgress >= 1.0f) {
            transitionProgress = 1.0f;
            transitioning = false;
        }
        paletteShader.setFloat("blendFactor", transitionProgress);
    }
}
```

### Per-Pixel Palette Selection

Different parts of a sprite can use different palette rows. The trick: encode which palette to use in a second channel of the index texture.

```
Index texture encoding:
  R channel = palette index (which color within a row)
  G channel = palette row   (which palette to use)

This lets you do:
  Skin pixels:     R=5, G=0  -> row 0 (skin palette)
  Clothing pixels: R=3, G=1  -> row 1 (clothing palette)
  Hair pixels:     R=7, G=2  -> row 2 (hair palette)

Now you can swap skin tone independently of clothing color!
Each "layer" of the character references a different palette row.
```

```glsl
// Per-pixel palette selection fragment shader
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D indexTexture;
uniform sampler2D paletteTexture;
uniform float paletteSize;
uniform float paletteCount;

// Uniform array: which palette row to use for each "group"
// Group 0 might be skin, group 1 might be clothing, etc.
uniform float paletteSelections[8]; // support up to 8 groups

void main() {
    vec4 indexSample = texture(indexTexture, TexCoord);

    if (indexSample.a < 0.01) {
        discard;
    }

    float index = round(indexSample.r * 255.0);

    // The G channel encodes which group this pixel belongs to
    float group = round(indexSample.g * 255.0);

    // Look up which palette row this group should use
    int groupInt = int(group);
    float selectedRow = paletteSelections[groupInt];

    // Standard palette lookup
    float u = (index + 0.5) / paletteSize;
    float v = (selectedRow + 0.5) / paletteCount;
    vec4 color = texture(paletteTexture, vec2(u, v));

    FragColor = color;
}
```

This is powerful for character customization. Instead of separate texture layers (body, hair, clothing), you have ONE texture with different pixel groups, and each group can be independently recolored by choosing its palette row.

### Palette-Based Effects

Quick visual effects by manipulating the palette:

**Flash white on damage.** Set all palette entries to white for 2-3 frames, then restore. The entire character flashes. Classic NES/SNES damage feedback.

```
Normal palette:                  Flash palette:
  [trans][blk][brn][red]...       [trans][wht][wht][wht]...

Hold flash for 2 frames, then restore.
Alternate between normal and flash 3 times for a "flicker" effect.

Timeline:
  Frame 0-1:  FLASH (all white)
  Frame 2-3:  NORMAL
  Frame 4-5:  FLASH
  Frame 6-7:  NORMAL
  Frame 8-9:  FLASH
  Frame 10+:  NORMAL (done)
```

**Fade to grayscale on death.** Gradually replace palette colors with their luminance-equivalent gray values.

```
Normal:     [trans] [#301008] [#C04020] [#F08040] [#FFD040]
Grayscale:  [trans] [#141414] [#606060] [#909090] [#C0C0C0]

Lerp between the two over 1 second using the palette lerp technique.
```

**Freeze effect (blue tint).** Replace all palette entries with blue-shifted versions of themselves.

**Poison effect.** Shift all colors toward green. This can be done as a palette swap or as a palette lerp from normal to green-shifted.

### Combining Palette Swap with Multiply Tinting

You can use both techniques simultaneously. Use palette swapping for precise multi-color remapping (skin tones, where the highlight/shadow relationship matters) and multiply tinting for simple hue shifts (a solid-color cape).

```glsl
// Combined palette swap + tint fragment shader
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D indexTexture;
uniform sampler2D paletteTexture;
uniform float paletteRow;
uniform float paletteSize;
uniform float paletteCount;

// Additional tint applied AFTER palette lookup
uniform vec3 tintColor;     // e.g., (1.0, 1.0, 1.0) for no tint, (1.0, 0.5, 0.5) for warm
uniform float tintStrength;  // 0.0 = no tint, 1.0 = full tint

void main() {
    vec4 indexSample = texture(indexTexture, TexCoord);

    if (indexSample.a < 0.01) {
        discard;
    }

    float index = round(indexSample.r * 255.0);
    float u = (index + 0.5) / paletteSize;
    float v = (paletteRow + 0.5) / paletteCount;
    vec4 paletteColor = texture(paletteTexture, vec2(u, v));

    // Apply tint on top of the palette color
    vec3 tinted = paletteColor.rgb * mix(vec3(1.0), tintColor, tintStrength);

    FragColor = vec4(tinted, paletteColor.a);
}
```

### Dynamic Palette Generation

Instead of pre-authored palette textures, compute palette colors from game state:

```cpp
// Generate a palette based on player health
void generateHealthPalette(unsigned char* palette, float healthPercent) {
    // Full health: warm, vibrant colors
    // Low health: desaturated, reddish colors

    for (int i = 0; i < 16; i++) {
        // Get the base color for this entry
        float baseR = basePalette[i * 4 + 0] / 255.0f;
        float baseG = basePalette[i * 4 + 1] / 255.0f;
        float baseB = basePalette[i * 4 + 2] / 255.0f;

        // Compute luminance (grayscale value)
        float lum = 0.299f * baseR + 0.587f * baseG + 0.114f * baseB;

        // At low health, shift toward red and desaturate
        float desatFactor = healthPercent;  // 1.0 = full color, 0.0 = grayscale

        float r = baseR * desatFactor + lum * (1.0f - desatFactor);
        float g = baseG * desatFactor + lum * (1.0f - desatFactor);
        float b = baseB * desatFactor + lum * (1.0f - desatFactor);

        // At low health, push the red channel higher
        float redShift = (1.0f - healthPercent) * 0.3f;
        r = fmin(r + redShift, 1.0f);

        palette[i * 4 + 0] = static_cast<unsigned char>(r * 255.0f);
        palette[i * 4 + 1] = static_cast<unsigned char>(g * 255.0f);
        palette[i * 4 + 2] = static_cast<unsigned char>(b * 255.0f);
        palette[i * 4 + 3] = basePalette[i * 4 + 3]; // preserve alpha
    }
}
```

---

## 6. Indexed Sprites vs. Full-Color Sprites -- When to Use Which

This is a practical decision you will face for every sprite category in your game. Here is a comparison to help you decide.

| Criterion | Indexed Sprites (Palette Swap) | Full-Color Sprites (RGBA) |
|-----------|-------------------------------|--------------------------|
| **Memory per pixel** | 1 byte (8-bit index) or 0.5 bytes (4-bit) | 4 bytes (RGBA8) |
| **Color variety per sprite** | Limited to palette size (typically 16-256) | Unlimited (16 million colors) |
| **Color variants** | Nearly free (just a new palette row, ~64 bytes) | Full sprite redraw or tint shader |
| **Art workflow** | Artist must work in indexed mode | Standard art workflow |
| **Runtime color changes** | Trivially fast (swap palette row) | Requires shader tricks |
| **Palette animation** | Natural and elegant | Not possible without custom shader |
| **Visual fidelity** | Best for pixel art / retro style | Required for painterly / HD style |
| **Shader complexity** | Additional texture sample + math | Simple (just sample texture) |
| **Debugging** | Index textures look like noise in image viewers | Sprites look correct in any viewer |
| **Tool support** | Aseprite (excellent), others vary | Universal |

**When palette swapping is the right choice:**

- Enemy variants. You have 20 enemy types but want 4 color variants of each for difficulty tiers or biomes. Palette swapping gives you 80 visual variants from 20 sets of sprite data.
- Retro aesthetic. If your game embraces the pixel art look, palette limitations are part of the charm.
- Environmental color shifts. Day/night, seasons, and biome-specific coloring applied to many sprites simultaneously.
- Performance-sensitive scenarios. Mobile games or games with many sprites on screen benefit from the smaller memory footprint of indexed textures.

**When full-color textures are better:**

- Unique characters. A main character with a unique design that will never be recolored gains nothing from indexing.
- Complex gradients. Smooth color transitions, complex lighting bakes, or photorealistic textures cannot be reduced to 16 palette entries.
- Simple projects. If you only have a few sprites and no recoloring needs, the added complexity of the palette pipeline is not worth it.

**Hybrid approaches.** You can absolutely mix both approaches in the same game. Use indexed sprites for enemies and NPCs (which benefit from palette variants) and full-color sprites for the player character and unique bosses (which have complex, non-repeating art). The fragment shader used depends on which sprite type is being drawn -- you can switch shaders between draw calls.

```
Hybrid rendering approach:

For each sprite to draw:
  if (sprite.isIndexed) {
      paletteShader.use();
      // bind index texture to unit 0
      // bind palette texture to unit 1
      // set palette uniforms
      // draw
  } else {
      normalShader.use();     // your current sprite.frag
      // bind RGBA texture to unit 0
      // draw
  }
```

**Asset pipeline considerations.** If you adopt palette swapping, your art pipeline changes:

1. Artists work in Aseprite's indexed mode with a shared palette
2. Palette files are exported separately and managed as data
3. A converter tool transforms indexed PNGs into the R-channel format
4. Palette textures are constructed from palette files (possibly automated)
5. New palette variants can be created by non-artists (designers, programmers) since they only need to pick colors for each slot

This front-loaded pipeline investment pays off when you need to create many variants quickly. It is similar to how web design systems work: investing in a design token system up front makes creating new themes trivial later.

---

## 7. Real-World Case Studies

Studying how commercial games use palette techniques reveals both the power and the practical considerations of the approach.

### Shovel Knight

**Yacht Club Games, 2014.** Shovel Knight is perhaps the most famous modern example of deliberate NES-style palette constraints.

The developers imposed strict rules inspired by (but not identical to) NES hardware limits:
- Sprites use a limited number of colors per character
- Background palettes are restricted per-area
- Palette swapping is used for enemy variants across different areas

**Area-specific palettes.** The same enemy designs appear in different areas with different palettes. A green slime in the forest zone becomes a blue slime in the ice zone and a red slime in the fire zone. The sprite shape is identical; only the palette differs.

```
Shovel Knight enemy palette strategy:

Plains (World 1):     green-themed enemies
Ice (World 3):        blue-themed enemies    -- same sprites, cold palette
Fire (World 5):       red-themed enemies     -- same sprites, warm palette
Shadow (World 7):     purple-themed enemies  -- same sprites, dark palette

This approach:
  - Maintains visual consistency within each area
  - Communicates area identity through color
  - Quadruples enemy variety with minimal art cost
  - Feels authentic to the NES era it emulates
```

**Player palette.** Shovel Knight himself has alternate palettes unlocked as cosmetic rewards. These are classic palette swaps -- same sprite data, different color mapping. The player's palette is applied per-frame, meaning it works with all animation states.

### Dead Cells

**Motion Twin, 2018.** Dead Cells uses palette swapping extensively for its procedurally generated runs.

**Biome-specific enemy palettes.** Each biome in Dead Cells has a distinct color palette. When enemies appear in different biomes, they use different palette variants. A zombie in the Promenade of the Condemned has a different color scheme than the same zombie type in the Toxic Sewers.

**Why palette swapping matters for Dead Cells.** As a roguelike with procedural generation, Dead Cells needs to populate many biomes with enemy variety. Creating entirely new enemy art for each biome would be prohibitively expensive. Palette swapping lets a single enemy design serve multiple biomes while still feeling visually distinct in each.

```
Dead Cells palette variety strategy:

Base enemy sprites: ~50 unique enemy designs
Biome count: ~20 biomes
Palette variants per enemy: 2-4

Without palette swap: 50 enemies x 4 color variants = 200 sprite sets to draw
With palette swap: 50 sprite sets + 200 palette rows (each ~64 bytes)

Art time saved: roughly 75%, since palette variants take minutes to create
(just choosing new colors for each slot) while a full sprite set takes days.
```

### Celeste

**Matt Thorson and Noel Berry, 2018.** Celeste presents an interesting edge case in the palette swap discussion.

**Madeline's hair color changes with dash state.** Her hair shifts from red to blue when she uses her dash, indicating whether the dash is available (red = ready, blue = spent). At first glance, this looks like palette swapping. But the implementation is actually **multiply tinting**, not palette swapping.

```
Celeste's hair color technique:

The hair sprite is drawn in a neutral light color (near-white grayscale).
A tint color is applied as a uniform:
  - Red tint when dash is available
  - Blue tint when dash is used
  - White flash during dash animation

This is MULTIPLY TINTING, not CLUT palette swapping.

Why tinting works here:
  - The hair is a single "material" (one ramp from light to dark)
  - Only the hue needs to change, not the value structure
  - Tinting is simpler to implement than full palette swap
  - The hair color needs to transition smoothly (red -> blue fade)
    which is easier with tint lerp than palette lerp

Why palette swapping would be overkill:
  - Only 2-3 colors change (the hair ramp)
  - The rest of the character stays the same
  - No need for arbitrary remapping, just hue shift
```

This is an important lesson: **palette swapping is not always the right tool.** When only one hue needs to change and it does not require precise multi-step remapping, simple tinting is easier, faster, and more flexible.

### Hyper Light Drifter

**Heart Machine, 2016.** Hyper Light Drifter uses a severely restricted palette to create one of the most visually striking games of its era.

The entire game uses a palette of roughly 20-30 colors. Every environment, character, enemy, and effect is rendered from this shared palette. The result is an incredibly cohesive visual identity where everything feels like it belongs to the same world.

**How limited palettes create mood.** Hyper Light Drifter's palette is dominated by cyan, magenta, and dark blue-purple. These colors evoke a sense of alien technology, melancholy, and mystery. The limited palette forces every element to share the same color DNA, creating a mood that would be impossible with unlimited colors (which would tempt artists to use "correct" colors for each object rather than emotionally resonant ones).

```
Hyper Light Drifter's approximate palette structure:

Dark foundation:   [#0C0C1E]  [#1B1B3A]  [#2B2B5A]
Cool midtones:     [#3B5DC9]  [#4BBDCB]  [#7DCEA8]
Warm accents:      [#DB3370]  [#FF6B6B]  [#FFAD33]
Highlights:        [#E0F0E0]  [#FFFFFF]

Everything in the game is made from these ~12 colors.
Enemies, environments, UI, effects -- all the same palette.

This is not "palette swapping" in the traditional sense.
It is "palette discipline" -- choosing to constrain yourself
to a unified color set for aesthetic cohesion.
```

### Castlevania (NES/SNES)

**Konami, 1986-1994.** The Castlevania series is the canonical example of palette-swapped enemies as a game design tool.

The NES Castlevania games systematically reused enemy sprites with different palettes to indicate increasing difficulty:

```
Castlevania palette-swapped enemy progression:

Skeleton:
  White palette (early game):  low HP, low damage, slow
  Red palette (mid game):      higher HP, higher damage, faster
  Gold palette (late game):    very high HP, high damage, aggressive AI

Medusa Head:
  Blue palette (normal):       slow, predictable sine wave path
  Gold palette (hard):         faster, irregular movement

Fleaman (jumping hunchback):
  Gray palette (normal):       short jumps, predictable
  Red palette (hard):          high jumps, erratic movement

This is game design through art reuse:
  - Players learn to read color as difficulty signal
  - Developers get 2-3x enemy variety from each sprite set
  - The convention (darker/warmer = harder) becomes intuitive
```

The SNES Castlevania games (Super Castlevania IV, Dracula X) continued this tradition with the expanded SNES palette, giving enemies richer color ramps while maintaining the palette swap system for variants.

---

## 8. Implementation Architecture

This section outlines how to architect a palette swap system that integrates with your existing engine. All code in this section is pseudocode/conceptual -- it shows how the pieces connect within your engine's patterns (unique_ptr ownership, raw-pointer references, component-based entities).

### PaletteManager Class

A centralized manager that loads, stores, and provides palette textures.

```cpp
// PaletteManager.h (conceptual)
#pragma once

#include "Common.h"
#include <string>
#include <unordered_map>
#include <vector>

// Represents a single palette texture that may contain multiple palette rows
struct PaletteTexture {
    GLuint textureID;
    int width;       // colors per palette (e.g., 16)
    int height;      // number of palette variants (rows)
    std::string name;

    // Raw palette data kept in CPU memory for cycling/modification
    std::vector<unsigned char> cpuData;  // width * height * 4 bytes (RGBA)
};

class PaletteManager {
private:
    // All loaded palette textures, keyed by name
    std::unordered_map<std::string, std::unique_ptr<PaletteTexture>> palettes;

public:
    PaletteManager();
    ~PaletteManager();

    // Load a palette from an image file (e.g., a 16xN pixel PNG)
    // The image is interpreted directly: each pixel IS a color entry.
    PaletteTexture* loadPalette(const std::string& name, const std::string& filepath);

    // Create a palette from raw data
    PaletteTexture* createPalette(
        const std::string& name,
        const unsigned char* data,
        int colorsPerRow,
        int rowCount
    );

    // Get a loaded palette by name (returns non-owning pointer)
    PaletteTexture* getPalette(const std::string& name) const;

    // Update palette data on GPU (for color cycling or dynamic palettes)
    void updatePaletteRow(
        PaletteTexture* palette,
        int row,
        const unsigned char* rowData  // colorsPerRow * 4 bytes
    );

    // Bind a palette texture to a specific texture unit
    void bindPalette(PaletteTexture* palette, GLuint textureUnit) const;

    // Cleanup
    void clear();
};
```

```cpp
// PaletteManager.cpp (conceptual)

#include "PaletteManager.h"

PaletteManager::PaletteManager() {}

PaletteManager::~PaletteManager() {
    clear();
}

PaletteTexture* PaletteManager::loadPalette(
    const std::string& name,
    const std::string& filepath
) {
    // Load the image file -- each pixel is a palette color
    SDL_Surface* loadedSurface = IMG_Load(filepath.c_str());
    if (!loadedSurface) {
        std::cerr << "Failed to load palette: " << filepath << std::endl;
        return nullptr;
    }

    // Convert to RGBA32 for consistent format
    SDL_Surface* surface = SDL_ConvertSurfaceFormat(
        loadedSurface, SDL_PIXELFORMAT_RGBA32, 0
    );
    SDL_FreeSurface(loadedSurface);

    if (!surface) {
        std::cerr << "Failed to convert palette surface" << std::endl;
        return nullptr;
    }

    // Create the palette texture object
    auto palette = std::make_unique<PaletteTexture>();
    palette->name = name;
    palette->width = surface->w;
    palette->height = surface->h;

    // Store CPU copy of palette data for later modification
    int dataSize = surface->w * surface->h * 4;
    palette->cpuData.resize(dataSize);
    std::memcpy(palette->cpuData.data(), surface->pixels, dataSize);

    // Create OpenGL texture
    glGenTextures(1, &palette->textureID);
    glBindTexture(GL_TEXTURE_2D, palette->textureID);

    // CRITICAL: nearest filtering, no interpolation between palette entries
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload palette data to GPU
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA,
        surface->w, surface->h, 0,
        GL_RGBA, GL_UNSIGNED_BYTE,
        surface->pixels
    );

    SDL_FreeSurface(surface);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Store and return non-owning pointer
    PaletteTexture* ptr = palette.get();
    palettes[name] = std::move(palette);

    std::cout << "Palette loaded: " << name
              << " (" << ptr->width << " colors x " << ptr->height << " variants)"
              << std::endl;

    return ptr;
}

void PaletteManager::updatePaletteRow(
    PaletteTexture* palette,
    int row,
    const unsigned char* rowData
) {
    // Update CPU copy
    int rowBytes = palette->width * 4;
    std::memcpy(
        palette->cpuData.data() + (row * rowBytes),
        rowData,
        rowBytes
    );

    // Update GPU texture (only the modified row)
    glBindTexture(GL_TEXTURE_2D, palette->textureID);
    glTexSubImage2D(
        GL_TEXTURE_2D, 0,
        0, row,             // x offset, y offset (start of the row)
        palette->width, 1,  // width, height (one full row)
        GL_RGBA, GL_UNSIGNED_BYTE,
        rowData
    );
    glBindTexture(GL_TEXTURE_2D, 0);
}

void PaletteManager::bindPalette(PaletteTexture* palette, GLuint textureUnit) const {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, palette->textureID);
}

void PaletteManager::clear() {
    for (auto& pair : palettes) {
        if (pair.second->textureID != 0) {
            glDeleteTextures(1, &pair.second->textureID);
        }
    }
    palettes.clear();
}
```

### Integration with the Sprite Rendering Pipeline

The palette swap system plugs into your existing rendering pipeline. Your current flow is:

```
Current flow (Sprite::draw):
  1. shader.use()
  2. Set projection, view, model matrices
  3. Set spriteTexture uniform to 0
  4. Bind texture to unit 0
  5. Draw quad

Palette swap flow (same structure, more uniforms):
  1. paletteShader.use()
  2. Set projection, view, model matrices         (identical)
  3. Set indexTexture uniform to 0
  4. Bind index texture to unit 0
  5. Set paletteTexture uniform to 1               (new)
  6. Bind palette texture to unit 1                (new)
  7. Set paletteRow, paletteSize, paletteCount     (new)
  8. Draw quad                                     (identical)
```

The vertex shader (`sprite.vert`) does not change at all. Only the fragment shader is different.

### Efficient Batching

When many sprites share the same index texture but use different palettes, you can reduce state changes:

```
Inefficient (switch everything per sprite):
  For each skeleton enemy:
    Bind skeleton index texture     -- same every time
    Bind palette texture            -- same every time
    Set palette row                 -- changes per enemy
    Set model matrix                -- changes per enemy
    Draw

Efficient (batch by shared state):
  Bind skeleton index texture       -- once
  Bind palette texture              -- once
  Set paletteSize, paletteCount     -- once
  For each skeleton enemy:
    Set palette row                 -- changes per enemy
    Set model matrix                -- changes per enemy
    Draw

You eliminate redundant glBindTexture calls. For 50 skeleton enemies,
that is 49 fewer texture binds.
```

The most efficient approach, if you have many palette-swapped sprites:

```
Sort sprites by:
  1. Shader program (palette shader vs normal shader)
  2. Index texture (all skeletons together, all slimes together)
  3. Palette texture (usually the same for all variants)

Then iterate, only changing state when it differs from the previous sprite.
```

### Pseudocode for the Complete System

Putting everything together, here is how the full system operates in a single frame:

```
Game::render() {
    // Phase 1: Render normal (non-palette) sprites
    normalShader.use();
    set projection, view matrices;
    for each normalSprite in scene:
        bind sprite's RGBA texture to unit 0;
        set model matrix for this sprite;
        draw quad;

    // Phase 2: Render palette-swapped sprites
    paletteShader.use();
    set projection, view matrices;

    // Group by index texture for efficiency
    for each indexTextureGroup:
        bind this group's index texture to unit 0;
        bind this group's palette texture to unit 1;
        set paletteSize, paletteCount uniforms;

        for each sprite in this group:
            set paletteRow for this sprite's variant;
            set model matrix for this sprite;
            draw quad;

    // Phase 3: Update palette cycling (for animated palettes)
    paletteManager.updateCycles(deltaTime);
    // This modifies palette texture data and re-uploads changed rows
}

Game::update(float deltaTime) {
    // Update cycling accumulators for any animated palette ranges
    for each activeCycleRange:
        activeCycleRange.accumulator += deltaTime;
    // The actual palette modification happens in render phase
    // (because it involves GPU texture updates)
}
```

---

## 9. Palette Swapping for Your HD-2D Project

Your project aims for an HD-2D Stardew Valley clone with Octopath Traveler 2-style shaders. Here is how palette swapping fits into that vision.

**Enemy variants.** Stardew Valley has multiple types of slimes, bats, and other creatures that appear in different areas of the mines. Palette swapping is the natural tool for this. A single slime sprite set can serve 5+ difficulty tiers with different palettes. You get the Castlevania benefit: players learn to read color as difficulty, and you get massive content variety from minimal art.

**Seasonal changes.** Stardew Valley's world transforms across four seasons. Trees go from green (spring/summer) to orange/red (fall) to bare (winter). Grass, flowers, and crops all change color. Palette swapping is ideal here -- the same tree sprite with a different palette for each season. This is far more efficient than creating 4 separate tree sprite sets.

```
Seasonal palette for a tree:

               Leaves-Light  Leaves-Dark  Trunk-Light  Trunk-Dark
Spring:        #88CC44       #447722      #AA8844      #664422
Summer:        #66AA33       #336611      #998844      #554422
Fall:          #DD8822       #AA4411      #887744      #554433
Winter:        transparent   transparent  #887766      #665544

Same tree sprite. Same pixel data. Four distinct seasonal appearances.
The trunk colors also shift slightly (warmer in summer, cooler in winter).
```

**Day/night color shifts.** As the in-game clock progresses from morning to evening to night, the world's color palette should shift. Palette lerping (Section 5.2) enables this smoothly. You define a "morning" palette, a "noon" palette, a "sunset" palette, and a "night" palette, then interpolate between them based on the current time.

```
Day/night palette lerp timeline:

6:00 AM   Sunrise palette   (warm orange highlights, cool blue shadows)
     |     blend...
12:00 PM  Noon palette      (bright warm highlights, neutral shadows)
     |     blend...
6:00 PM   Sunset palette    (deep orange/red highlights, purple shadows)
     |     blend...
10:00 PM  Night palette     (cool blue-gray, dark, low contrast)
     |     blend...
6:00 AM   Sunrise palette   (cycle repeats)
```

**Magical effects.** When a character casts a spell or enters a magical state, palette swapping can communicate this instantly. A healing spell might shift the character's palette toward warm green. A curse might shift it toward sickly purple. A power-up might make the palette more saturated and vibrant.

**Combining with layered sprites.** Not all layers benefit equally from palette swapping. Here is a practical breakdown for a Stardew-style character:

```
Layer analysis for palette swap vs tinting:

Layer          Technique      Why
----------------------------------------------------------
Body/skin      Palette swap   Skin tones need precise multi-color ramps
                              (highlight, midtone, shadow all shift hue)
Hair           Palette swap   Hair has highlights, base, shadow that
                              should shift together in a coordinated way
Clothing       Tint           Single-material, one hue, tinting is simpler
                              and allows continuous color picker (more choices)
Accessories    Neither        Fixed colors, not customizable
Weapon         Tint or swap   Depends on whether weapons have material detail
                              (metallic needs swap, solid color needs tint)
```

**Performance considerations.** The palette lookup adds exactly one additional texture sample per pixel. In your fragment shader, instead of one `texture()` call, you have two (one for the index, one for the palette). On modern GPUs, this is negligible. A 2D game is never fragment-shader-bound unless you add extremely complex post-processing. The real cost is the increased shader complexity and the need to manage palette textures, which is a development/maintenance cost, not a runtime cost.

```
Performance comparison:

Normal sprite rendering:
  1 texture sample per pixel (sample the RGBA sprite texture)
  Cost: ~1 texture unit

Palette swap rendering:
  2 texture samples per pixel (sample index texture + sample palette texture)
  Cost: ~2 texture units

For a 1920x1080 screen filled entirely with palette-swapped sprites:
  Extra samples: 1920 x 1080 = 2,073,600 additional texture lookups per frame
  Modern GPU texture sampling rate: ~10+ billion samples per second
  Time added: ~0.0002 seconds = 0.2 milliseconds

  At 60 fps you have 16.67 ms per frame.
  0.2 ms is about 1.2% of your frame budget.
  In practice, 2D games use far less than the full screen for sprites,
  so the actual cost is even lower.

  Verdict: palette swapping has essentially zero performance impact
  in a 2D game on modern hardware.
```

**Fitting into the Octopath Traveler aesthetic.** Octopath Traveler's HD-2D style uses pixel art sprites with modern lighting, bloom, and depth-of-field. Palette swapping fits naturally into the sprite layer of this pipeline. The palette swap happens before any post-processing:

```
HD-2D rendering pipeline with palette swap:

1. Render all sprites to a framebuffer
   - Normal sprites: standard fragment shader
   - Palette-swapped sprites: palette fragment shader
   - Result: a flat 2D image with all sprites composited

2. Apply dynamic lighting
   - Point lights, ambient light, shadows
   - Operates on the composited image, unaware of palette swap

3. Apply post-processing
   - Bloom, depth of field, vignette, color grading
   - Also unaware of palette swap

The palette swap is "invisible" to all downstream effects.
It is purely a sprite-level technique. Post-processing treats
palette-swapped sprites exactly like any other sprite.
```

This separation of concerns means you can implement palette swapping now and add HD-2D post-processing effects later without any conflicts between the systems. They operate at different stages of the pipeline and do not interact.

---

## Glossary

| Term | Definition |
|------|-----------|
| **CLUT** | Color Look-Up Table. A table that maps index values to actual colors. The foundation of palette swapping. |
| **Indexed color** | An image format where each pixel stores an index into a palette rather than a direct color value. |
| **Palette** | An ordered set of colors. In palette swapping, the palette defines what each index value displays as. |
| **Palette ramp** | A sequence of colors within a palette organized from lightest to darkest for a single material/hue. |
| **Palette row** | One row of a 2D palette texture. Each row is a complete palette variant. |
| **Palette swap** | Changing a sprite's appearance by pointing it at a different palette, without modifying the sprite data. |
| **Color cycling** | Animating a palette by rotating colors through a range of indices over time. Creates motion without sprite changes. |
| **Ping-pong cycling** | Color cycling that reverses direction at range boundaries instead of wrapping, creating pulsing effects. |
| **Half-texel offset** | Adding 0.5 to texel coordinates before normalizing, ensuring the sample point hits the center of the target texel. Prevents edge-sampling artifacts. |
| **Index texture** | A texture where the red (or luminance) channel stores palette indices. Used as input to the palette lookup shader. |
| **Palette texture** | A small texture (typically 16-256 pixels wide, 1-N pixels tall) where each pixel IS a color in the palette. |
| **Gradient mapping** | A variation of palette lookup that uses a smooth gradient instead of discrete palette entries. Produces painterly rather than pixel-art results. |
| **Palette lerp** | Smoothly interpolating between two palettes over time. Used for gradual visual transitions (day/night, status effects). |
| **Master palette** | The hardware-level set of all possible colors. On the NES, this was 64 fixed colors. Modern hardware has no such limitation. |
| **PPU** | Picture Processing Unit. The NES's dedicated graphics chip that performed palette lookups in hardware. |
| **Hue shift** | Changing a color's position on the color wheel. In palette design, shadows often shift toward cool hues and highlights toward warm hues. |
| **Value** | The lightness or darkness of a color, independent of its hue. Critical for readable sprite art. |
| **Texel** | A single pixel within a texture. Distinguished from "pixel" (which refers to screen pixels). |
| **DB32** | DawnBringer 32-color palette. A widely used standard palette in the pixel art community. |
| **Lospec** | A community website hosting curated pixel art palettes. |

---

## Resources

- **Lospec Palette List**: lospec.com/palette-list -- thousands of curated pixel art palettes
- **NES PPU technical reference**: nesdev.org/wiki/PPU -- detailed hardware documentation
- **Mark Ferrari's color cycling art**: markferrari.com -- the LucasArts artist famous for palette animation
- **Aseprite documentation on indexed color**: aseprite.org/docs/color-mode -- working with indexed sprites
- **LearnOpenGL - Textures**: learnopengl.com/Getting-started/Textures -- OpenGL texture fundamentals
- **The Book of Shaders**: thebookofshaders.com -- fragment shader techniques
- **Shovel Knight Developer Blog**: yachtclubgames.com/blog -- articles on their NES-authentic color approach
