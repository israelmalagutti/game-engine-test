# Shader-Based Animation Techniques in 2D Games

This document covers how to create dynamic visual effects entirely inside GLSL shaders -- animations that happen without changing sprite frames. The shader itself creates motion, distortion, color shifts, and post-processing effects. This is the foundation of the "HD-2D" visual style used in Octopath Traveler 2 and the aesthetic this engine is targeting.

## Table of Contents
- [1. What Shader-Based Animation Means](#1-what-shader-based-animation-means)
- [2. UV Animation (Texture Coordinate Manipulation)](#2-uv-animation-texture-coordinate-manipulation)
  - [UV Scrolling](#uv-scrolling)
  - [UV Distortion](#uv-distortion)
  - [UV Rotation](#uv-rotation)
  - [Sprite Sheet Animation in the Shader](#sprite-sheet-animation-in-the-shader)
- [3. Vertex Displacement](#3-vertex-displacement)
  - [Wave Effect](#wave-effect)
  - [Wind Effect](#wind-effect)
  - [Squash and Stretch in the Vertex Shader](#squash-and-stretch-in-the-vertex-shader)
  - [Character Hit Flash via Vertex Offset](#character-hit-flash-via-vertex-offset)
- [4. Fragment Shader Effects](#4-fragment-shader-effects)
  - [Color Flash / Damage Flash](#color-flash--damage-flash)
  - [Outline / Border Effect](#outline--border-effect)
  - [Dissolve Effect](#dissolve-effect)
  - [Silhouette / Shadow](#silhouette--shadow)
  - [Palette Cycling in the Shader](#palette-cycling-in-the-shader)
  - [Pixelation Effect](#pixelation-effect)
  - [Chromatic Aberration](#chromatic-aberration)
  - [Bloom (Simplified)](#bloom-simplified)
  - [Vignette](#vignette)
- [5. Post-Processing Pipeline](#5-post-processing-pipeline)
- [6. Combining Shader Effects with Sprite Animation](#6-combining-shader-effects-with-sprite-animation)
- [7. Performance](#7-performance)
- [8. HD-2D Shader Techniques (Octopath Traveler Analysis)](#8-hd-2d-shader-techniques-octopath-traveler-analysis)

---

## 1. What Shader-Based Animation Means

In the previous documentation (`DOCS.md`), you learned about frame-by-frame sprite animation -- the artist draws each frame of a walk cycle, and the engine cycles through them over time. That is **content-driven** animation: the visual change comes from swapping between pre-made images.

Shader-based animation is fundamentally different. **The shader program itself creates the visual change.** A single, static sprite image goes into the shader, and the shader outputs something that moves, warps, glows, dissolves, or pulses -- all computed mathematically on the GPU, every single frame.

### The Key: A Time Uniform

The entire mechanism hinges on one concept: **passing a continuously increasing time value to the shader as a uniform.**

```
CPU Side (C++):
  Every frame:
    float currentTime = SDL_GetTicks() / 1000.0f;   // seconds since start
    shader.setFloat("uTime", currentTime);

GPU Side (GLSL):
  uniform float uTime;   // receives the time value
  // Use uTime in any mathematical expression to create change over time
```

Because `uTime` increases every frame, any expression that includes it will produce a different result each frame. That is animation -- a value that changes over time.

```
uTime = 0.0     uTime = 0.5     uTime = 1.0     uTime = 1.5
  Frame 1          Frame 30        Frame 60        Frame 90
  sin(0.0) = 0     sin(0.5) = 0.48  sin(1.0) = 0.84  sin(1.5) = 1.0

The sine function converts linear time into smooth oscillation.
This is the building block of almost every shader animation.
```

### Categories of Shader Animation

Shader-based animation techniques fall into four categories:

```
                        Shader Animation Taxonomy

  UV Manipulation          Vertex Displacement       Color Effects         Post-Processing
  (texture coords)        (geometry deformation)    (pixel colors)        (screen-wide)
  +-----------------+     +-----------------+       +-----------------+   +------------------+
  | UV scrolling    |     | Wave / ripple   |       | Damage flash    |   | Bloom            |
  | UV distortion   |     | Wind sway       |       | Outline         |   | Vignette         |
  | UV rotation     |     | Squash/stretch  |       | Dissolve        |   | Chromatic aberr. |
  | Sheet animation |     | Hit shake       |       | Silhouette      |   | Depth of field   |
  +-----------------+     +-----------------+       | Palette cycle   |   | Color grading    |
                                                    | Pixelation      |   +------------------+
  WHERE the texture       WHERE the vertices        +-----------------+
  is sampled              are positioned            WHAT COLOR the       Effects applied to
  (fragment shader)       (vertex shader)           pixel becomes        the ENTIRE screen
                                                    (fragment shader)    (separate render pass)
```

### Why This Is Separate From (and Complementary To) Sprite Animation

Shader animation does NOT replace frame-by-frame animation. They work on different layers of the rendering pipeline:

```
Frame-by-frame animation:
  Changes WHICH part of the texture is shown
  Happens on the CPU (update frame index, recalculate UVs)
  Changes the SOURCE DATA going into the shader

Shader animation:
  Changes HOW the texture is rendered
  Happens on the GPU (mathematical transformations)
  Changes the PROCESSING of whatever source data arrives

Together:
  A walking character (frame-by-frame) can ALSO have:
  - A glowing outline (shader)
  - Wind-blown hair (shader)
  - Damage flash when hit (shader)
  - Dissolving away on death (shader)
```

Think of it this way: frame-by-frame animation is the **content** and shader animation is the **presentation layer** on top. Just like in web development, where HTML is content and CSS transforms/filters are visual effects applied to that content.

### Web Development Parallel

If you have used CSS, you have already experienced shader-like effects:

```
CSS/Web equivalent              Game/Shader equivalent
----------------------------    ----------------------------
CSS filter: blur()              Gaussian blur post-processing
CSS filter: brightness()        Fragment shader brightness multiply
CSS filter: hue-rotate()        Fragment shader color rotation
CSS filter: drop-shadow()       Silhouette rendering at offset
CSS backdrop-filter             Post-processing on scene behind element
CSS transform: rotate()         UV rotation in fragment shader
CSS animation + @keyframes      Uniform time + sin/cos functions
SVG <feTurbulence>              Noise-based distortion
CSS mix-blend-mode              OpenGL blending modes

Key difference: CSS runs on the CPU compositor (or limited GPU),
while GLSL shaders run FULLY on the GPU, processing millions
of pixels per frame at 60+ fps with parallel execution.
```

---

## 2. UV Animation (Texture Coordinate Manipulation)

UV animation works by modifying the texture coordinates before sampling the texture. Instead of sampling the texture at the coordinates the vertex shader provides, you shift, warp, or rotate those coordinates. The texture itself never changes -- you just look at different parts of it, or look at it in a distorted way.

### UV Scrolling

UV scrolling is the simplest shader animation technique. You add a time-based offset to the texture coordinates, causing the texture to slide across the surface.

```
How UV scrolling works:

Time = 0.0                Time = 0.5                Time = 1.0
Texture UV range:         Texture UV range:          Texture UV range:
[0.0 ... 1.0]            [0.5 ... 1.5]             [1.0 ... 2.0]
                                                          |
+---+---+---+---+        +---+---+---+---+         With GL_REPEAT,
| A | B | C | D |        | C | D | A | B |         2.0 wraps to 0.0,
+---+---+---+---+        +---+---+---+---+         so it tiles seamlessly.

The texture appears to scroll left continuously.
No new texture data is created -- we just shift WHERE we sample.
```

This creates flowing water, conveyor belts, clouds, and scrolling backgrounds.

**Wrapping mode is critical.** For UV scrolling to work seamlessly, the texture must be set to `GL_REPEAT`:

```cpp
// On the CPU side, when setting up the texture:
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

// GL_REPEAT means:
//   UV = 0.0  -->  left edge of texture
//   UV = 0.5  -->  middle of texture
//   UV = 1.0  -->  wraps back to left edge (same as 0.0)
//   UV = 1.7  -->  same as UV = 0.7
//   UV = -0.3 -->  same as UV = 0.7
//
// The texture tiles infinitely in every direction.
```

Here is the complete vertex shader for UV scrolling. Notice that the scrolling can be done in either the vertex shader (modifying `TexCoord` before it goes to the fragment shader) or the fragment shader (modifying it right before sampling). Both work, but doing it in the vertex shader is slightly more efficient because the vertex shader runs per-vertex (6 vertices for a quad) while the fragment shader runs per-pixel (potentially thousands of pixels).

```glsl
// uv_scroll.vert -- Vertex shader with UV scrolling
#version 330 core

layout (location = 0) in vec2 aPos;       // Vertex position (quad corners)
layout (location = 1) in vec2 aTexCoord;   // Texture coordinate (0,0 to 1,1)

out vec2 TexCoord;   // Passed to fragment shader

uniform mat4 model;       // Object transform (position, scale, rotation)
uniform mat4 view;        // Camera transform
uniform mat4 projection;  // Orthographic projection

uniform float uTime;          // Continuously increasing time (seconds)
uniform vec2 uScrollSpeed;    // Direction and speed of scrolling
                               // e.g., vec2(0.5, 0.0) scrolls right at 0.5 UV units/sec
                               // e.g., vec2(0.0, -0.3) scrolls down at 0.3 UV units/sec
                               // e.g., vec2(0.2, 0.1) scrolls diagonally

void main() {
    // Standard vertex transformation (same as your existing sprite.vert)
    gl_Position = projection * view * model * vec4(aPos, 0.0, 1.0);

    // Apply time-based offset to texture coordinates
    // The mod/fract isn't strictly necessary because GL_REPEAT handles wrapping,
    // but it prevents the UV values from growing to extremely large numbers
    // over long play sessions, which could cause floating-point precision loss.
    TexCoord = aTexCoord + uScrollSpeed * uTime;
}
```

```glsl
// uv_scroll.frag -- Fragment shader (standard texture sampling)
#version 330 core

in vec2 TexCoord;    // Received from vertex shader (already scrolled)
out vec4 FragColor;  // Output pixel color

uniform sampler2D spriteTexture;   // The texture to render

void main() {
    // Sample the texture at the scrolled coordinates
    // GL_REPEAT wrapping makes this tile seamlessly
    vec4 texColor = texture(spriteTexture, TexCoord);

    // Discard fully transparent pixels
    if (texColor.a < 0.01) {
        discard;
    }

    FragColor = texColor;
}
```

On the CPU side, you would set the uniforms each frame:

```cpp
// In your render loop (C++ side):
float currentTime = SDL_GetTicks() / 1000.0f;
waterShader.use();
waterShader.setFloat("uTime", currentTime);
waterShader.setVec2("uScrollSpeed", 0.3f, 0.0f);  // scroll right
```

### UV Distortion

Instead of shifting UV coordinates by a constant amount, you shift them by a value computed from a trigonometric function. This creates wavy, organic distortion.

```
Constant offset (scrolling):     Sine-based offset (distortion):

  uv.x += 0.3                      uv.x += sin(uv.y * 10.0) * 0.02

  Before:    After:                 Before:        After:
  AAAAA      AAAAA                  AAAAAA         A A A A
  BBBBB      BBBBB                  BBBBBB          B B B B
  CCCCC      CCCCC                  CCCCCC         C C C C
  DDDDD      DDDDD                  DDDDDD          D D D D
  (shifted uniformly)               (each row shifted differently!)

The sine function takes the Y coordinate as input, so each horizontal
row of pixels gets a different X offset. This creates a wavy pattern.
Add time, and the wave moves.
```

The formula is:

```
newUV.x = uv.x + sin(uv.y * frequency + time * speed) * amplitude

Where:
  frequency  = How many waves appear vertically (higher = more waves)
  speed      = How fast the waves move (higher = faster)
  amplitude  = How far each pixel shifts (higher = more extreme distortion)
```

```
Frequency comparison (amplitude = 0.05):

freq = 2.0:                freq = 8.0:                freq = 20.0:
  One gentle wave            Several waves              Many tight waves
  ~~ /                      /\/\/\/\                   /\/\/\/\/\/\/\/\/\
    /                      /        \
   /                      /          \
~~                        /\/\/\/\
```

This creates heat shimmer, underwater distortion, magical portals, and dream sequences.

Here is the complete fragment shader for UV distortion:

```glsl
// uv_distortion.frag -- Fragment shader with wavy UV distortion
#version 330 core

in vec2 TexCoord;    // Original texture coordinates from vertex shader
out vec4 FragColor;  // Output pixel color

uniform sampler2D spriteTexture;   // The texture to render

uniform float uTime;       // Continuously increasing time (seconds)

// Distortion parameters -- pass these as uniforms to tune the effect
// from C++ without recompiling the shader
uniform float uFrequency;   // Number of waves (try 8.0 - 20.0)
uniform float uSpeed;       // Wave movement speed (try 1.0 - 5.0)
uniform float uAmplitude;   // Distortion strength (try 0.005 - 0.05)
                             // Keep amplitude SMALL or the image becomes unreadable

void main() {
    // Start with the original UV coordinates
    vec2 distortedUV = TexCoord;

    // Apply horizontal distortion based on vertical position and time
    // sin() outputs -1.0 to 1.0, so the maximum shift is +/- amplitude
    distortedUV.x += sin(TexCoord.y * uFrequency + uTime * uSpeed) * uAmplitude;

    // Optionally also distort vertically based on horizontal position
    // Using cosine instead of sine creates a more organic, circular motion
    // because sin and cos are 90 degrees out of phase
    distortedUV.y += cos(TexCoord.x * uFrequency + uTime * uSpeed) * uAmplitude;

    // Sample the texture at the distorted coordinates
    vec4 texColor = texture(spriteTexture, distortedUV);

    if (texColor.a < 0.01) {
        discard;
    }

    FragColor = texColor;
}
```

**Layered distortion** creates richer effects. Just like audio synthesis, combining multiple sine waves at different frequencies creates complex waveforms:

```glsl
// uv_layered_distortion.frag -- Multiple overlapping distortion waves
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform float uTime;

void main() {
    vec2 uv = TexCoord;

    // Layer 1: Slow, large waves (the base motion)
    uv.x += sin(uv.y * 4.0 + uTime * 1.5) * 0.015;
    uv.y += cos(uv.x * 4.0 + uTime * 1.2) * 0.015;

    // Layer 2: Medium-speed, medium waves (adds complexity)
    uv.x += sin(uv.y * 10.0 + uTime * 3.0) * 0.008;
    uv.y += cos(uv.x * 10.0 + uTime * 2.5) * 0.008;

    // Layer 3: Fast, fine waves (adds shimmer/detail)
    uv.x += sin(uv.y * 25.0 + uTime * 5.0) * 0.003;
    uv.y += cos(uv.x * 25.0 + uTime * 4.5) * 0.003;

    // The three layers combine to create organic, non-repeating distortion
    // that looks much more natural than a single sine wave.
    // This is the same principle as Fourier synthesis in audio.

    vec4 texColor = texture(spriteTexture, uv);

    if (texColor.a < 0.01) {
        discard;
    }

    FragColor = texColor;
}
```

### UV Rotation

UV rotation spins the texture around a pivot point. This requires a 2D rotation matrix applied to the texture coordinates.

```
UV Rotation around center (0.5, 0.5):

  Original:          Rotated 45 deg:        Rotated 90 deg:
  +--------+         +--------+             +--------+
  | A    B |         |   /\   |             | C  A   |
  |        |         |  /  \  |             |        |
  |        |         | /    \ |             |        |
  | C    D |         |/  ..  \|             | D  B   |
  +--------+         +--------+             +--------+

Steps:
  1. Translate UV so pivot is at origin: uv -= pivot
  2. Apply rotation matrix
  3. Translate back: uv += pivot
```

The 2D rotation matrix is:

```
| cos(angle)  -sin(angle) |
| sin(angle)   cos(angle) |

Applied to a 2D vector:
  rotated.x = original.x * cos(angle) - original.y * sin(angle)
  rotated.y = original.x * sin(angle) + original.y * cos(angle)
```

Complete shader for UV rotation:

```glsl
// uv_rotation.frag -- Fragment shader with rotating texture coordinates
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform float uTime;
uniform float uRotationSpeed;   // Radians per second (try 1.0 - 3.0)

void main() {
    // Define the pivot point -- (0.5, 0.5) is the center of the quad
    vec2 pivot = vec2(0.5, 0.5);

    // Calculate the rotation angle based on time
    float angle = uTime * uRotationSpeed;

    // Precompute sin and cos (the GPU does this very efficiently)
    float cosAngle = cos(angle);
    float sinAngle = sin(angle);

    // Step 1: Translate UV coordinates so the pivot is at the origin
    vec2 uv = TexCoord - pivot;

    // Step 2: Apply the 2D rotation matrix
    vec2 rotatedUV;
    rotatedUV.x = uv.x * cosAngle - uv.y * sinAngle;
    rotatedUV.y = uv.x * sinAngle + uv.y * cosAngle;

    // Step 3: Translate back so the pivot returns to its original position
    rotatedUV += pivot;

    // Sample the texture at the rotated coordinates
    // GL_REPEAT wrapping will tile the texture if rotation pushes UVs out of [0,1]
    vec4 texColor = texture(spriteTexture, rotatedUV);

    if (texColor.a < 0.01) {
        discard;
    }

    FragColor = texColor;
}
```

This is useful for spinning items (coins, collectibles), rotating saw blades, vortex/portal effects, or spinning UI indicators.

### Sprite Sheet Animation in the Shader

Your current engine computes the sprite sheet UV offset on the CPU and passes `uvOffset` and `uvSize` uniforms to the tile vertex shader. This works, but for hundreds of animated tiles (flowing water, flickering torches, waving grass), you can move that calculation into the vertex shader itself.

The idea: instead of the CPU computing which frame to show and updating uniforms per-tile, the shader receives the frame count, sheet layout, and current time, and computes the correct UV offset itself.

```
CPU-driven (current approach):         Shader-driven (new approach):

  CPU:                                   CPU:
    frame = int(time / frameDur) % N       (nothing per-tile!)
    uvOffset.x = frame * frameW
    uvOffset.y = row * frameH              Sends once per frame:
    upload uvOffset uniform                  uTime, uFrameDuration
                                             uSheetColumns, uSheetRows
  GPU:                                       uAnimationRow
    TexCoord = uvOffset + aTexCoord * uvSize
                                           GPU:
  Must update uniform for EVERY tile.        Computes frame from uTime
  100 water tiles = 100 uniform updates.     Computes UV offset from frame
                                             All water tiles share one draw call.
```

Complete shaders for shader-driven sprite sheet animation:

```glsl
// sheet_animation.vert -- Vertex shader that computes sprite sheet UVs
#version 330 core

layout (location = 0) in vec2 aPos;       // Vertex position
layout (location = 1) in vec2 aTexCoord;   // Texture coordinate (0-1 for the full quad)

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Sprite sheet parameters
uniform float uTime;            // Current time in seconds
uniform float uFrameDuration;   // Seconds per frame (e.g., 0.15)
uniform int uSheetColumns;      // Number of columns in the sheet (e.g., 4)
uniform int uSheetRows;         // Number of rows in the sheet (e.g., 4)
uniform int uAnimationRow;      // Which row of the sheet to animate (e.g., 0 for first row)
uniform int uFrameCount;        // Number of frames in this animation
                                 // (may be less than uSheetColumns if the row is not full)

void main() {
    // Standard vertex transformation
    gl_Position = projection * view * model * vec4(aPos, 0.0, 1.0);

    // Compute the current frame index from time
    // int() truncates (rounds down), mod loops back to 0
    int currentFrame = int(uTime / uFrameDuration) % uFrameCount;

    // Compute the size of one frame in UV space (0.0 to 1.0)
    float frameWidth = 1.0 / float(uSheetColumns);    // e.g., 1/4 = 0.25
    float frameHeight = 1.0 / float(uSheetRows);      // e.g., 1/4 = 0.25

    // Compute the UV offset for this frame
    // Column = currentFrame (horizontal position in the sheet)
    // Row = uAnimationRow (which row to use)
    vec2 uvOffset = vec2(
        float(currentFrame) * frameWidth,      // X offset: which column
        float(uAnimationRow) * frameHeight     // Y offset: which row
    );

    // Scale the quad's texture coordinates to the size of one frame,
    // then offset to the correct position in the sheet
    TexCoord = uvOffset + aTexCoord * vec2(frameWidth, frameHeight);
}
```

```glsl
// sheet_animation.frag -- Standard fragment shader (same as sprite.frag)
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;

void main() {
    vec4 texColor = texture(spriteTexture, TexCoord);

    if (texColor.a < 0.01) {
        discard;
    }

    FragColor = texColor;
}
```

The advantage: you can batch all animated tiles that share the same sheet and animation timing into a single draw call. The GPU handles the frame calculation for every vertex in parallel. This is especially valuable for tilemap animations where hundreds of water tiles all animate in sync.

---

## 3. Vertex Displacement

Vertex displacement changes the **position** of vertices rather than how the texture is sampled. This deforms the geometry itself -- the shape of the quad changes, not just the texture on it.

This happens in the **vertex shader**, because that is where vertex positions are processed.

```
Fragment shader effects:              Vertex shader effects:
Change WHAT you see                   Change WHERE you see it

+--------+     +--------+            +--------+     +~~------+
| Normal |     | Tinted |            | Normal |     |/ Normal \
| sprite | --> | sprite |            | sprite | --> |  sprite  |
|        |     |        |            |        |     |\        /
+--------+     +--------+            +--------+     +--~~~~--+

Same shape, different colors          Different shape, same colors
```

### Wave Effect

The wave effect displaces vertex Y positions based on their X position and time, creating a rippling or waving motion.

```
Wave displacement over time:

Time = 0.0:       Time = 0.5:       Time = 1.0:
+---------+       +---~~~~--+       +~~------~~+
|         |       | /      \|       |/          \
|  FLAG   |       |/ FLAG   |       | FLAG      |
|         |       |         \       |\          /
+---------+       +---____--+       +--~~~~~~--+

Each vertex's Y position is offset by:
  offset = sin(vertex.x * frequency + time * speed) * amplitude

Vertices at different X positions get different Y offsets,
creating a wave shape that moves over time.
```

Complete vertex shader for the wave effect:

```glsl
// wave.vert -- Vertex shader with wave displacement
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform float uTime;
uniform float uWaveFrequency;    // How many waves across the sprite (try 3.0 - 10.0)
uniform float uWaveSpeed;        // How fast waves move (try 2.0 - 6.0)
uniform float uWaveAmplitude;    // How tall the waves are in pixels (try 2.0 - 10.0)

void main() {
    // Start with the original vertex position
    vec2 pos = aPos;

    // Displace Y based on X position and time
    // This creates a wave that travels horizontally across the sprite
    pos.y += sin(pos.x * uWaveFrequency + uTime * uWaveSpeed) * uWaveAmplitude;

    // Transform the displaced position through the MVP pipeline
    gl_Position = projection * view * model * vec4(pos, 0.0, 1.0);

    // Pass texture coordinates unchanged -- the texture is mapped onto
    // the deformed geometry, so it stretches with the wave
    TexCoord = aTexCoord;
}
```

```glsl
// wave.frag -- Standard fragment shader (no special logic needed)
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;

void main() {
    vec4 texColor = texture(spriteTexture, TexCoord);

    if (texColor.a < 0.01) {
        discard;
    }

    FragColor = texColor;
}
```

**Important caveat for quads:** Your sprite quad has only 4 unique vertex positions (the corners). A sine wave sampled at only 4 points will not look like a smooth wave -- it will look like a tilted or skewed rectangle. For visible wave effects, you need to **subdivide the quad** into many smaller quads (a grid mesh), so there are enough vertices to show the wave curvature:

```
4-vertex quad (no subdivision):        16x4 vertex grid (subdivided):

A---------B                            A--+--+--+--+--+--+--B
|         |                            |  |  |  |  |  |  |  |
|         |      After wave,           +--+--+--+--+--+--+--+
|         |      only corners          |  |  |  |  |  |  |  |
|         |      move, creating        +--+--+--+--+--+--+--+
C---------D      a parallelogram       |  |  |  |  |  |  |  |
                 (not a wave)          C--+--+--+--+--+--+--D

                                       After wave, each vertex
                                       moves independently,
                                       creating a smooth wave shape.
```

This is why wave/wind effects on simple quads often use UV distortion in the fragment shader instead (Section 2) -- it works per-pixel and does not require mesh subdivision.

### Wind Effect

Wind simulation for grass and vegetation uses vertex displacement, but with a key insight: **only the top vertices should move.** Grass is anchored at its roots (bottom), and sways at the tips (top).

```
Without anchoring:               With anchoring (correct):

Original:  Wind:                  Original:  Wind:
+---+      +---+                  +---+        +---+
|   |        |   |                |   |          /   |
|   |  -->     |   |              |   |  -->    /   |
|   |            |   |            |   |        /   |
+---+              +---+         +---+        +---+
                                              ^
(Entire sprite                   (Bottom stays fixed,
 slides sideways)                 top sways -- correct!)
```

The trick: use the texture V coordinate (vertical) to scale the displacement. In standard UV mapping, V=0.0 is the top of the texture and V=1.0 is the bottom. If we invert this (or use 1.0 - V), we get a value that is 1.0 at the top and 0.0 at the bottom -- the perfect multiplier for top-only displacement.

```
Texture V coordinate as displacement scale:

V = 0.0 (top)    -->  displacement = 1.0 * amplitude  (maximum sway)
V = 0.25         -->  displacement = 0.75 * amplitude
V = 0.5 (middle) -->  displacement = 0.5 * amplitude
V = 0.75         -->  displacement = 0.25 * amplitude
V = 1.0 (bottom) -->  displacement = 0.0 * amplitude  (no movement)
```

Complete vertex shader for wind-affected vegetation:

```glsl
// wind.vert -- Vertex shader for grass/vegetation wind sway
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform float uTime;
uniform float uWindStrength;   // Maximum displacement in pixels (try 3.0 - 8.0)
uniform float uWindSpeed;     // How fast the wind oscillates (try 1.0 - 4.0)
uniform float uWindFrequency; // Spatial variation (try 0.5 - 2.0)
                                // Higher values make nearby grass sway differently

void main() {
    vec2 pos = aPos;

    // Compute the anchoring factor from the texture V coordinate
    // aTexCoord.y = 0.0 at top, 1.0 at bottom
    // We want maximum displacement at the top and zero at the bottom
    float anchorFactor = 1.0 - aTexCoord.y;

    // Compute the wind displacement
    // Using both position and time in the sin() creates spatial variation:
    // nearby grass blades sway at slightly different phases
    float windOffset = sin(
        pos.x * uWindFrequency +   // Spatial variation based on X position
        uTime * uWindSpeed          // Movement over time
    ) * uWindStrength;

    // Add a second, slower wave for more organic motion
    // (otherwise the wind looks too mechanical/repetitive)
    windOffset += sin(
        pos.x * uWindFrequency * 0.4 +   // Lower frequency
        uTime * uWindSpeed * 0.6          // Slower speed
    ) * uWindStrength * 0.3;              // Smaller amplitude

    // Apply displacement, scaled by anchor factor
    // Bottom vertices (anchorFactor = 0.0) don't move at all
    // Top vertices (anchorFactor = 1.0) move the full amount
    pos.x += windOffset * anchorFactor;

    // Optional: slight Y compression when displaced (grass bends, doesn't stretch)
    // The further the top bends horizontally, the more the height decreases
    pos.y += abs(windOffset * anchorFactor) * -0.15;

    gl_Position = projection * view * model * vec4(pos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
```

```glsl
// wind.frag -- Standard fragment shader
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;

void main() {
    vec4 texColor = texture(spriteTexture, TexCoord);

    if (texColor.a < 0.01) {
        discard;
    }

    FragColor = texColor;
}
```

### Squash and Stretch in the Vertex Shader

Squash and stretch is a fundamental animation principle. When a character lands from a jump, they compress vertically (squash). When they leap upward, they elongate (stretch). This can be done procedurally in the vertex shader without modifying the sprite art.

```
Stretch (jumping up):    Normal:         Squash (landing):

  +----+                 +--------+      +------------+
  |    |                 |        |      |            |
  |    |                 |        |      +------------+
  |    |                 |        |
  |    |                 +--------+
  +----+

  scaleX = 0.8           scaleX = 1.0    scaleX = 1.2
  scaleY = 1.3           scaleY = 1.0    scaleY = 0.7

Note: scaleX * scaleY should stay approximately constant (volume preservation).
```

The key: scaling should happen relative to the base (bottom) of the sprite, not the center. A character standing on the ground should squash downward from their head, not from their middle (which would push their feet into the ground).

```glsl
// squash_stretch.vert -- Vertex shader for squash/stretch effect
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Squash/stretch parameters
uniform float uSquashX;    // Horizontal scale factor (1.0 = normal, 1.2 = wider)
uniform float uSquashY;    // Vertical scale factor (1.0 = normal, 0.7 = shorter)
                            // For volume preservation: squashX * squashY ~ 1.0

void main() {
    vec2 pos = aPos;

    // Scale relative to the bottom of the sprite
    // In a standard quad, the bottom vertices have Y = 0 and top vertices have Y = spriteHeight
    // We want the bottom to stay fixed, so we scale the offset from the bottom

    // If your quad is defined with Y=0 at the bottom:
    //   pos.x is scaled around the center X (shift to center, scale, shift back)
    //   pos.y is scaled from the bottom (just multiply, since bottom is Y=0)

    // Scale X around the horizontal center of the sprite
    // Assuming the quad goes from X=0 to X=spriteWidth:
    float centerX = 0.5;  // If using normalized coordinates [0,1]
    pos.x = centerX + (pos.x - centerX) * uSquashX;

    // Scale Y from the bottom (Y=0 is bottom, so just multiply)
    // If Y=0 is the bottom of your quad, this naturally keeps the bottom fixed
    pos.y = pos.y * uSquashY;

    gl_Position = projection * view * model * vec4(pos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
```

```glsl
// squash_stretch.frag -- Standard fragment shader
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;

void main() {
    vec4 texColor = texture(spriteTexture, TexCoord);

    if (texColor.a < 0.01) {
        discard;
    }

    FragColor = texColor;
}
```

On the CPU side, you would animate the squash values with a spring or simple timer:

```cpp
// When the character lands:
squashX = 1.3f;   // Wider
squashY = 0.7f;   // Shorter

// Each frame, lerp back to normal:
squashX = squashX + (1.0f - squashX) * deltaTime * 8.0f;
squashY = squashY + (1.0f - squashY) * deltaTime * 8.0f;
// After ~0.3 seconds, both values return to 1.0
```

### Character Hit Flash via Vertex Offset

When a character takes damage, you can briefly jitter their vertex positions to create a per-sprite "screen shake" effect. This is distinct from camera shake (which moves the entire view) -- it only affects the damaged entity.

```glsl
// hit_shake.vert -- Vertex shader with damage jitter
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform float uHitTimer;     // Counts down from 1.0 to 0.0 after being hit
                               // Set to 1.0 when damage occurs, decreases each frame
uniform float uShakeIntensity; // Maximum displacement in pixels (try 2.0 - 5.0)

void main() {
    vec2 pos = aPos;

    // Only apply shake when the timer is active (> 0.0)
    if (uHitTimer > 0.0) {
        // Generate pseudo-random offset based on the vertex position and timer
        // Using different multipliers for X and Y prevents the shake from being
        // purely diagonal
        float shakeX = sin(uHitTimer * 73.0) * uShakeIntensity * uHitTimer;
        float shakeY = cos(uHitTimer * 97.0) * uShakeIntensity * uHitTimer;

        // Apply the shake
        // Multiply by uHitTimer so the shake decreases as the timer runs out
        pos.x += shakeX;
        pos.y += shakeY;
    }

    gl_Position = projection * view * model * vec4(pos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
```

```glsl
// hit_shake.frag -- Standard fragment shader
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;

void main() {
    vec4 texColor = texture(spriteTexture, TexCoord);

    if (texColor.a < 0.01) {
        discard;
    }

    FragColor = texColor;
}
```

An alternative approach is the **impact bulge**: instead of jittering, briefly expand all vertices outward from the sprite's center, then snap back. This creates a "pop" effect that reads well even on small pixel art.

---

## 4. Fragment Shader Effects

Fragment shader effects modify the **color** of each pixel without changing the geometry. These are the most versatile shader effects because the fragment shader runs once per pixel, giving you per-pixel control.

### Color Flash / Damage Flash

The simplest and most universally useful shader effect in games. When an entity takes damage, its entire sprite flashes white (or red) for a few frames, then returns to normal.

```
Normal:            Flash (frame 1-3):     Back to normal:
+--------+         +--------+            +--------+
|  @@@@  |         |  ####  |            |  @@@@  |
| @@  @@ |    -->  | ##  ## |    -->     | @@  @@ |
| @@@@@@ |         | ###### |            | @@@@@@ |
|  @  @  |         |  #  #  |            |  @  @  |
+--------+         +--------+            +--------+
(normal colors)    (all white)           (normal colors)

@ = textured pixel    # = white pixel
```

The technique: use `mix()` to interpolate between the original texture color and the flash color.

```
mix(A, B, t):
  t = 0.0  -->  returns A (original color)
  t = 0.5  -->  returns average of A and B
  t = 1.0  -->  returns B (flash color)

By controlling t (the flashAmount uniform), you control how much
of the flash color appears. Animate t from 1.0 to 0.0 for a
flash-then-fade effect.
```

Complete fragment shader:

```glsl
// damage_flash.frag -- Fragment shader with damage flash effect
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;

uniform float uFlashAmount;  // 0.0 = normal, 1.0 = full flash
                               // Animate this on the CPU: set to 1.0 on hit,
                               // then decrease to 0.0 over ~0.1 seconds
uniform vec3 uFlashColor;    // Color to flash (white = vec3(1.0), red = vec3(1.0, 0.0, 0.0))

void main() {
    vec4 texColor = texture(spriteTexture, TexCoord);

    if (texColor.a < 0.01) {
        discard;
    }

    // Mix between the original color and the flash color
    // uFlashAmount = 0.0: texColor.rgb (normal)
    // uFlashAmount = 1.0: uFlashColor (full flash)
    vec3 finalColor = mix(texColor.rgb, uFlashColor, uFlashAmount);

    // Preserve the original alpha (transparency) so the sprite silhouette stays correct
    FragColor = vec4(finalColor, texColor.a);
}
```

**Variations:**

For an **additive emission flash** (the sprite glows brighter rather than turning solid white), add instead of mix:

```glsl
// Additive flash -- sprite glows, original colors still visible
vec3 finalColor = texColor.rgb + uFlashColor * uFlashAmount;
// Clamp to prevent values > 1.0 causing artifacts
finalColor = min(finalColor, vec3(1.0));
```

For a **flash then fade** on the CPU side:

```cpp
// On hit:
flashTimer = 0.15f;   // Flash duration in seconds

// Each frame:
if (flashTimer > 0.0f) {
    flashTimer -= deltaTime;
    float flashAmount = flashTimer / 0.15f;  // 1.0 to 0.0 over 0.15 seconds
    shader.setFloat("uFlashAmount", flashAmount);
} else {
    shader.setFloat("uFlashAmount", 0.0f);
}
```

### Outline / Border Effect

An outline shader draws a colored border around the sprite's visible pixels. It works by checking if any neighboring pixel is transparent while the current pixel is opaque -- if so, the current pixel is on the **edge** of the sprite and should be drawn in the outline color.

```
Original sprite:          With outline:

    . . . . . .              . . . . . .
    . . @ @ . .              . # # # # .
    . @ @ @ @ .              # @ @ @ @ #
    . @ @ @ @ .    -->       # @ @ @ @ #
    . . @ @ . .              . # @ @ # .
    . . . . . .              . . # # . .

. = transparent    @ = sprite pixel    # = outline pixel

The outline appears WHERE a visible pixel borders a transparent pixel.
```

The algorithm: for each pixel, sample the texture at 4 (or 8) neighboring positions. If the current pixel is visible (alpha > 0) and at least one neighbor is transparent (alpha = 0), this pixel is an edge pixel -- draw it in the outline color.

Alternatively (and more commonly), if the current pixel is transparent but at least one neighbor is visible, draw the outline color at THIS position. This creates the outline OUTSIDE the sprite rather than on its edge pixels.

```glsl
// outline.frag -- Fragment shader that draws an outline around the sprite
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform vec2 uTexelSize;      // Size of one pixel in UV space: vec2(1.0/textureWidth, 1.0/textureHeight)
                                // e.g., for a 32x32 texture: vec2(1.0/32.0, 1.0/32.0)
uniform vec3 uOutlineColor;   // Color of the outline (e.g., vec3(1.0, 1.0, 0.0) for yellow)
uniform float uOutlineThickness;  // How many pixels wide the outline is (1.0 = 1 pixel)

void main() {
    // Sample the current pixel
    vec4 texColor = texture(spriteTexture, TexCoord);

    // If the current pixel is visible (opaque), draw it normally
    if (texColor.a > 0.01) {
        FragColor = texColor;
        return;
    }

    // If we reach here, the current pixel is TRANSPARENT.
    // Check if any neighboring pixel is OPAQUE.
    // If so, we are just outside the sprite's edge -- draw the outline here.

    float outlineStep = uOutlineThickness;

    // Sample 4 neighbors (up, down, left, right)
    float alphaUp    = texture(spriteTexture, TexCoord + vec2(0.0, -uTexelSize.y * outlineStep)).a;
    float alphaDown  = texture(spriteTexture, TexCoord + vec2(0.0,  uTexelSize.y * outlineStep)).a;
    float alphaLeft  = texture(spriteTexture, TexCoord + vec2(-uTexelSize.x * outlineStep, 0.0)).a;
    float alphaRight = texture(spriteTexture, TexCoord + vec2( uTexelSize.x * outlineStep, 0.0)).a;

    // Sample 4 diagonal neighbors for an 8-directional outline (smoother corners)
    float alphaUpLeft    = texture(spriteTexture, TexCoord + vec2(-uTexelSize.x, -uTexelSize.y) * outlineStep).a;
    float alphaUpRight   = texture(spriteTexture, TexCoord + vec2( uTexelSize.x, -uTexelSize.y) * outlineStep).a;
    float alphaDownLeft  = texture(spriteTexture, TexCoord + vec2(-uTexelSize.x,  uTexelSize.y) * outlineStep).a;
    float alphaDownRight = texture(spriteTexture, TexCoord + vec2( uTexelSize.x,  uTexelSize.y) * outlineStep).a;

    // If any neighbor is opaque, this transparent pixel is on the outline border
    float maxNeighborAlpha = max(
        max(max(alphaUp, alphaDown), max(alphaLeft, alphaRight)),
        max(max(alphaUpLeft, alphaUpRight), max(alphaDownLeft, alphaDownRight))
    );

    if (maxNeighborAlpha > 0.01) {
        // Draw the outline pixel
        FragColor = vec4(uOutlineColor, 1.0);
    } else {
        // Fully transparent -- not near any sprite pixel
        discard;
    }
}
```

**Important:** This shader requires the sprite's texture to have transparent padding around the visible pixels, because the outline is drawn in the transparent area OUTSIDE the sprite. If your sprite fills its texture edge-to-edge, the outline pixels at the edges will be clipped. Add at least `outlineThickness` pixels of transparent padding.

**Performance note:** This shader does 8 extra texture samples per pixel (for the 8-neighbor version). For 2D games with modest resolution sprites, this is completely fine. A 64x64 sprite has 4096 pixels, each doing 9 texture samples = ~37K samples. A modern GPU handles billions of samples per second.

### Dissolve Effect

The dissolve effect makes a sprite appear to burn, disintegrate, or materialize by progressively discarding pixels. It uses a noise texture (or mathematical noise) as a threshold map.

```
Dissolve progression (threshold increasing from 0.0 to 1.0):

threshold = 0.0:    threshold = 0.3:    threshold = 0.6:    threshold = 1.0:
+--------+          +--------+          +--------+          +--------+
|@@@@@@@@|          |@@.@@@.@|          |@...@...|          |........|
|@@@@@@@@|          |.@@@.@@@|          |..@@...@|          |........|
|@@@@@@@@|          |@@.@@.@@|          |.@..@...|          |........|
|@@@@@@@@|          |@@@.@@.@|          |........|          |........|
+--------+          +--------+          +--------+          +--------+
(fully visible)     (starting to        (mostly gone)       (fully dissolved)
                     dissolve)

The noise texture determines WHICH pixels disappear first.
Pixels where noise value < threshold are discarded.
```

```glsl
// dissolve.frag -- Fragment shader with dissolve effect
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;   // The sprite to dissolve
uniform sampler2D noiseTexture;    // A noise texture (grayscale, values 0.0 to 1.0)
                                    // Can use Perlin noise, Worley noise, or any grayscale image

uniform float uDissolveThreshold;  // 0.0 = fully visible, 1.0 = fully dissolved
                                    // Animate this from 0.0 to 1.0 for dissolve-out
                                    // or from 1.0 to 0.0 for materialize-in

uniform vec3 uEdgeColor;          // Color of the dissolve edge (e.g., orange for fire)
uniform float uEdgeWidth;         // Width of the glowing edge (try 0.02 - 0.1)

void main() {
    vec4 texColor = texture(spriteTexture, TexCoord);

    // Skip already-transparent pixels
    if (texColor.a < 0.01) {
        discard;
    }

    // Sample the noise texture to get a dissolve value for this pixel
    // The noise value ranges from 0.0 to 1.0
    float noiseValue = texture(noiseTexture, TexCoord).r;

    // If the noise value is below the threshold, discard this pixel
    if (noiseValue < uDissolveThreshold) {
        discard;
    }

    // Edge glow: pixels just above the threshold get a bright edge color
    // This creates a glowing border at the dissolve boundary
    float distFromEdge = noiseValue - uDissolveThreshold;
    if (distFromEdge < uEdgeWidth) {
        // How close to the edge (0.0 = at edge, 1.0 = far from edge)
        float edgeFactor = distFromEdge / uEdgeWidth;

        // Mix the edge color into the sprite color
        // Pixels right at the dissolve boundary are fully edgeColor
        // Pixels further away gradually return to normal sprite color
        vec3 edgedColor = mix(uEdgeColor, texColor.rgb, edgeFactor);

        // Make the edge brighter (additive) for a glow effect
        edgedColor += uEdgeColor * (1.0 - edgeFactor) * 0.5;

        FragColor = vec4(edgedColor, texColor.a);
        return;
    }

    // Pixels far from the dissolve edge render normally
    FragColor = texColor;
}
```

**If you do not have a noise texture,** you can generate noise mathematically in the shader. Here is a simple pseudo-random hash function that produces noise-like values from UV coordinates:

```glsl
// Mathematical noise generation (no texture needed)
float hash(vec2 p) {
    // This produces a pseudo-random value between 0.0 and 1.0 for any input vec2
    // It is NOT true randomness -- the same input always produces the same output
    // But it LOOKS random, which is all we need for visual effects
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

// Use it instead of the noise texture:
float noiseValue = hash(TexCoord * 50.0);  // * 50.0 controls noise granularity
```

### Silhouette / Shadow

A flat shadow is rendered by drawing the sprite as a solid dark color at an offset position. This is typically done as a separate draw call BEFORE the main sprite, so the shadow appears behind the character.

```
Without shadow:              With shadow:

        +--------+                  +--------+
        |  char  |                  |  char  |
        +--------+            +--------+
                              |shadow  |
                              +--------+
                              (solid dark, offset down-right)
```

```glsl
// shadow.frag -- Fragment shader that renders sprite as a flat shadow
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform vec4 uShadowColor;   // e.g., vec4(0.0, 0.0, 0.0, 0.4) -- black at 40% opacity

void main() {
    vec4 texColor = texture(spriteTexture, TexCoord);

    // Discard transparent pixels (shadow only appears where sprite pixels exist)
    if (texColor.a < 0.01) {
        discard;
    }

    // Replace the color entirely with the shadow color
    // Keep the shadow's alpha multiplied by the sprite's alpha
    // so that semi-transparent parts of the sprite create semi-transparent shadow
    FragColor = vec4(uShadowColor.rgb, uShadowColor.a * texColor.a);
}
```

On the CPU side, draw the shadow by rendering the same sprite with an offset model matrix:

```cpp
// Render shadow first (behind the character)
shadowShader.use();
glm::mat4 shadowModel = characterModel;
shadowModel = glm::translate(shadowModel, glm::vec3(4.0f, 6.0f, 0.0f)); // offset
// Optionally scale Y to flatten: shadowModel = glm::scale(shadowModel, vec3(1.0, 0.5, 1.0));
shadowShader.setVec4("uShadowColor", 0.0f, 0.0f, 0.0f, 0.35f);
drawSprite(shadowShader, shadowModel);

// Then render the character normally
spriteShader.use();
drawSprite(spriteShader, characterModel);
```

### Palette Cycling in the Shader

This is covered in detail in `DOCS.md` under Technique 3 (CLUT). Here is the shader approach for cycling palette colors over time without modifying any texture data on the CPU:

```glsl
// palette_cycle.frag -- Fragment shader with animated palette cycling
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D indexTexture;     // Sprite with palette indices in the R channel
uniform sampler2D paletteTexture;   // 2D palette texture (colors along X, time frames along Y)
                                     // Width = number of colors (e.g., 16)
                                     // Height = number of cycle frames (e.g., 8)

uniform float uTime;               // Current time in seconds
uniform float uCycleSpeed;         // How fast the palette cycles (try 1.0 - 4.0)
uniform float uPaletteSize;        // Number of colors in the palette (e.g., 16.0)
uniform float uCycleFrames;        // Number of palette animation frames (e.g., 8.0)

void main() {
    vec4 indexSample = texture(indexTexture, TexCoord);

    if (indexSample.a < 0.01) {
        discard;
    }

    // Extract the palette index from the red channel
    float index = indexSample.r * 255.0;

    // Compute which palette frame to use based on time
    // mod() wraps the value so it cycles from 0 to cycleFrames-1 and back
    float cycleFrame = mod(uTime * uCycleSpeed, uCycleFrames);

    // Calculate UV into the palette texture
    float u = (index + 0.5) / uPaletteSize;         // Which color (X axis)
    float v = (cycleFrame + 0.5) / uCycleFrames;    // Which cycle frame (Y axis)

    // Look up the actual color from the palette
    vec4 color = texture(paletteTexture, vec2(u, v));

    FragColor = color;
}
```

This creates flowing water, pulsing lava, rainbow effects, and glowing magical items -- all without any CPU-side texture updates. The entire animation is computed per-pixel on the GPU.

### Pixelation Effect

Pixelation deliberately reduces the apparent resolution by quantizing UV coordinates into a coarser grid. Each "block" of pixels samples the same point in the texture, creating large, blocky pixels.

```
Original (high res):        Pixelated (low res):

  @@.@@.@@.@@.              @@@@....@@@@
  .@@.@@.@@.@@              @@@@....@@@@
  @@.@@.@@.@@.              @@@@....@@@@
  .@@.@@.@@.@@              @@@@....@@@@
  @@.@@.@@.@@.              ....@@@@....
  .@@.@@.@@.@@              ....@@@@....

UV coordinates are "snapped" to a grid, so groups of pixels
all sample the same texel, creating the blocky look.
```

The formula:

```
pixelatedUV = floor(uv * resolution) / resolution

Where resolution controls how many "big pixels" appear:
  resolution = 32  -->  32x32 blocks (very pixelated)
  resolution = 128 -->  128x128 blocks (mild pixelation)
  resolution = textureWidth  -->  original resolution (no change)
```

```glsl
// pixelation.frag -- Fragment shader with pixelation effect
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform float uPixelResolution;   // Number of "big pixels" across the sprite
                                    // Lower = more pixelated
                                    // Animate this for a "digitize" transition:
                                    // go from 8.0 (very blocky) to 128.0 (smooth)

void main() {
    // Quantize the UV coordinates to a grid
    vec2 pixelatedUV = floor(TexCoord * uPixelResolution) / uPixelResolution;

    // Sample the texture at the quantized position
    // All pixels within the same grid cell sample the same texel
    vec4 texColor = texture(spriteTexture, pixelatedUV);

    if (texColor.a < 0.01) {
        discard;
    }

    FragColor = texColor;
}
```

Use cases: retro zoom transitions, "digital" or "glitch" aesthetics, screen effects when entering/leaving a digital world, or a deliberate low-resolution rendering style.

### Chromatic Aberration

Chromatic aberration simulates a lens defect where different colors of light refract at slightly different angles, causing the red, green, and blue channels of an image to separate spatially.

```
Normal:                    Chromatic aberration:

  +--------+               R channel shifted left
  | RRGGBB |               G channel stays centered
  | RRGGBB |               B channel shifted right
  | RRGGBB |
  +--------+               R  G  B       Result:
                            RR . .        reddish left edge
                            . GG .        normal center
                            . . BB        bluish right edge
```

This is a post-processing effect used heavily in Octopath Traveler for dramatic moments, damage states, and transitions.

```glsl
// chromatic_aberration.frag -- Fragment shader with color channel separation
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform float uAberrationAmount;   // Offset in UV space (try 0.002 - 0.01)
                                     // Larger values = more extreme separation
uniform vec2 uAberrationDirection; // Direction of separation
                                     // vec2(1.0, 0.0) = horizontal
                                     // vec2(0.0, 1.0) = vertical
                                     // vec2(1.0, 1.0) = diagonal (normalize first)

void main() {
    // Normalize the direction vector
    vec2 dir = normalize(uAberrationDirection);

    // Compute the offset for each channel
    vec2 offset = dir * uAberrationAmount;

    // Sample each color channel from a slightly different position
    // Red is shifted in one direction, blue in the opposite, green stays centered
    float r = texture(spriteTexture, TexCoord - offset).r;  // Red shifted one way
    float g = texture(spriteTexture, TexCoord).g;            // Green stays centered
    float b = texture(spriteTexture, TexCoord + offset).b;   // Blue shifted the other way

    // Use the alpha from the centered sample to preserve sprite shape
    float a = texture(spriteTexture, TexCoord).a;

    if (a < 0.01) {
        discard;
    }

    FragColor = vec4(r, g, b, a);
}
```

For a more sophisticated version, scale the aberration by distance from the screen center (lens effects are stronger at the edges):

```glsl
// Distance-based chromatic aberration (stronger at edges)
vec2 center = vec2(0.5, 0.5);
float distFromCenter = length(TexCoord - center);
float scaledAmount = uAberrationAmount * distFromCenter * 2.0;
vec2 offset = dir * scaledAmount;
```

### Bloom (Simplified)

Bloom makes bright areas of the image bleed light into surrounding pixels, creating a glowing, ethereal quality. This is one of the most important effects in the HD-2D visual style -- Octopath Traveler uses bloom extensively on torches, magic effects, sunlit areas, and glowing objects.

```
Without bloom:                  With bloom:

+-------------------+           +-------------------+
|                   |           |    ░░░            |
|     [torch]       |           |  ░░[torch]░░     |
|                   |    -->    |    ░░░            |
|     [player]      |           |     [player]      |
|                   |           |                   |
+-------------------+           +-------------------+

The torch's bright pixels "bleed" light into nearby dark pixels.
The player (not bright enough) is unaffected.
```

Bloom is a **multi-pass** effect. It cannot be done in a single shader pass because it requires reading pixel values from a wide area around each pixel (blurring), which would be prohibitively expensive to do inline. The standard pipeline:

```
Step 1: Render scene normally to an FBO (framebuffer object)
        This gives you the scene as a texture.

Step 2: Brightness extraction
        Render a full-screen quad with a shader that outputs only
        pixels above a brightness threshold. Everything else becomes black.

        Input:  scene texture
        Output: bright-only texture

Step 3: Gaussian blur (horizontal pass)
        Blur the bright-only texture horizontally.

        Input:  bright-only texture
        Output: horizontally blurred texture

Step 4: Gaussian blur (vertical pass)
        Blur the horizontally blurred texture vertically.

        Input:  horizontally blurred texture
        Output: fully blurred texture (the "bloom" texture)

Step 5: Composite
        Add the bloom texture on top of the original scene.

        Input:  original scene + bloom texture
        Output: final image with bloom

Pipeline diagram:
  Scene FBO ─────────────────────────────────────────→ Composite
      │                                                    ↑
      └──→ Bright Extract ──→ H-Blur ──→ V-Blur ──────────┘
```

**Why separate horizontal and vertical blur passes?** A 2D Gaussian blur with a kernel of radius N requires N*N texture samples per pixel. For N=16, that is 256 samples per pixel -- very expensive. But a 2D Gaussian blur can be mathematically decomposed (separated) into two 1D passes: horizontal then vertical. Each 1D pass only needs N samples. Total: N + N = 2*16 = 32 samples. This is called a **separable filter** and is a fundamental optimization in graphics programming.

Here is the brightness extraction shader:

```glsl
// bloom_extract.frag -- Extract bright pixels from the scene
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D sceneTexture;       // The rendered scene
uniform float uBrightnessThreshold;   // Minimum brightness to extract (try 0.7 - 0.9)

void main() {
    vec4 color = texture(sceneTexture, TexCoord);

    // Calculate perceived brightness using luminance weights
    // These weights (0.2126, 0.7152, 0.0722) match human eye sensitivity
    // Green contributes most to perceived brightness, blue the least
    float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Only output pixels above the threshold
    if (brightness > uBrightnessThreshold) {
        // Output the bright pixel as-is
        FragColor = color;
    } else {
        // Below threshold -- output black (this pixel won't contribute to bloom)
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
```

Here is the Gaussian blur shader (used for both horizontal and vertical passes):

```glsl
// gaussian_blur.frag -- 1D Gaussian blur (used for both H and V passes)
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D inputTexture;    // Texture to blur
uniform vec2 uBlurDirection;      // vec2(1.0/width, 0.0) for horizontal
                                    // vec2(0.0, 1.0/height) for vertical
uniform float uBlurRadius;        // How far to sample (try 4.0 - 16.0)

void main() {
    vec4 result = vec4(0.0);
    float totalWeight = 0.0;

    // Sample along the blur direction with Gaussian-distributed weights
    // The Gaussian function: weight = exp(-0.5 * (offset/sigma)^2)
    float sigma = uBlurRadius * 0.5;   // Standard deviation

    // Sample 2*radius+1 pixels along the blur direction
    for (float i = -uBlurRadius; i <= uBlurRadius; i += 1.0) {
        // Gaussian weight for this sample
        float weight = exp(-0.5 * (i / sigma) * (i / sigma));

        // Sample the texture at the offset position
        vec2 samplePos = TexCoord + uBlurDirection * i;
        result += texture(inputTexture, samplePos) * weight;

        totalWeight += weight;
    }

    // Normalize by total weight so brightness is preserved
    FragColor = result / totalWeight;
}
```

And the compositing shader that combines the original scene with the bloom:

```glsl
// bloom_composite.frag -- Combine original scene with bloom
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D sceneTexture;   // Original rendered scene
uniform sampler2D bloomTexture;   // Blurred bright pixels
uniform float uBloomIntensity;    // How strong the bloom is (try 0.3 - 1.5)

void main() {
    vec3 sceneColor = texture(sceneTexture, TexCoord).rgb;
    vec3 bloomColor = texture(bloomTexture, TexCoord).rgb;

    // Additive blending: add the bloom on top of the scene
    vec3 result = sceneColor + bloomColor * uBloomIntensity;

    // Tone mapping: prevent values from exceeding 1.0
    // Reinhard tone mapping is simple and effective
    result = result / (result + vec3(1.0));

    FragColor = vec4(result, 1.0);
}
```

### Vignette

Vignette darkens the edges of the screen and brightens the center, drawing the player's eye to the center of the action. This creates a subtle cinematic quality. Octopath Traveler uses vignetting consistently for its warm, intimate atmosphere.

```
Vignette effect:

  +------------------------+
  |##                    ##|     ## = heavily darkened
  |#                      #|     #  = moderately darkened
  |                        |        = center, unaffected
  |        [Player]        |
  |                        |
  |#                      #|
  |##                    ##|
  +------------------------+
```

The technique: compute the distance from the current pixel to the screen center. Pixels near the center are unchanged; pixels further away are darkened.

```glsl
// vignette.frag -- Fragment shader with vignette effect
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D sceneTexture;  // The rendered scene (from an FBO)
uniform float uVignetteRadius;   // How far the clear area extends (try 0.5 - 0.8)
uniform float uVignetteSoftness; // How gradual the darkening is (try 0.2 - 0.5)
uniform float uVignetteStrength; // Maximum darkening at edges (try 0.3 - 0.8)

void main() {
    vec3 color = texture(sceneTexture, TexCoord).rgb;

    // Calculate distance from the center of the screen
    // TexCoord ranges from (0,0) at bottom-left to (1,1) at top-right
    // Center is at (0.5, 0.5)
    vec2 center = TexCoord - vec2(0.5, 0.5);

    // Distance from center (0.0 at center, ~0.707 at corners)
    float dist = length(center);

    // Compute the vignette factor using smoothstep
    // smoothstep returns 0.0 when dist < radius, 1.0 when dist > radius+softness,
    // and smoothly interpolates in between
    float vignette = smoothstep(uVignetteRadius, uVignetteRadius + uVignetteSoftness, dist);

    // Apply the darkening
    // vignette = 0.0 at center (no darkening)
    // vignette = 1.0 at edges (maximum darkening)
    color *= 1.0 - vignette * uVignetteStrength;

    FragColor = vec4(color, 1.0);
}
```

---

## 5. Post-Processing Pipeline

Post-processing is the technique of rendering the entire game scene to an off-screen texture (via a Framebuffer Object / FBO), then drawing that texture to the screen with additional shader effects applied. This is how ALL screen-wide effects work.

### The Web Development Analogy

Post-processing is conceptually identical to something you already know from web development:

```
Web dev:
  <div style="filter: blur(2px) brightness(1.2) contrast(1.1)">
    <canvas id="game"></canvas>   <!-- the rendered game -->
  </div>

  The browser renders the game first, then applies the CSS filters
  to the entire rendered result.

Game dev:
  1. Render scene to FBO (off-screen texture)
  2. Apply shader effects to that texture
  3. Draw the result to the screen

Same concept, but in game dev you write the filter yourself in GLSL,
giving you unlimited control.
```

### Framebuffer Objects (FBOs)

An FBO lets you redirect rendering from the screen to a texture. Everything you draw between binding and unbinding the FBO goes to the texture instead of the screen.

```
Normal rendering:                    Rendering to FBO:

  glClear(...)                         glBindFramebuffer(GL_FRAMEBUFFER, fbo)
  drawSprite(...)                      glClear(...)
  drawTilemap(...)                     drawSprite(...)
  SDL_GL_SwapWindow(...)               drawTilemap(...)
                                       glBindFramebuffer(GL_FRAMEBUFFER, 0)  // unbind
      |                                    |
      V                                    V
  Goes directly                        Goes to a TEXTURE
  to the screen                        (not visible yet)
                                           |
                                           V
                                       Draw a full-screen quad with that texture
                                       + post-processing shader
                                           |
                                           V
                                       NOW it goes to the screen
```

### The Full-Screen Quad

To display the FBO's texture on screen, you render a quad that covers the entire viewport. The vertex shader for this is trivially simple:

```glsl
// fullscreen_quad.vert -- Vertex shader for a full-screen quad
#version 330 core

layout (location = 0) in vec2 aPos;       // Quad vertices: (-1,-1), (1,-1), (1,1), (-1,1)
layout (location = 1) in vec2 aTexCoord;   // UV: (0,0), (1,0), (1,1), (0,1)

out vec2 TexCoord;

void main() {
    // No model/view/projection needed -- the quad is already in clip space (-1 to 1)
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
```

```glsl
// passthrough.frag -- Simplest post-processing shader: just display the FBO texture
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D screenTexture;   // The FBO's color attachment (the rendered scene)

void main() {
    FragColor = texture(screenTexture, TexCoord);
}
```

### Multi-Pass Rendering

Some effects require multiple passes. The concept: render the scene to FBO A, then render a full-screen quad using FBO A's texture as input, writing the result to FBO B. Then use FBO B's texture as input for the next pass, writing to FBO C (or back to A). Finally, the last pass writes to the screen.

```
Multi-pass pipeline:

  Pass 0: Render scene     -->  FBO_Scene (scene texture)
  Pass 1: Bright extract   -->  FBO_Bright (bright pixels only)
  Pass 2: Horizontal blur  -->  FBO_BlurH (horizontally blurred brights)
  Pass 3: Vertical blur    -->  FBO_BlurV (fully blurred brights = bloom)
  Pass 4: Composite        -->  Screen (scene + bloom + vignette + color grading)

Each pass:
  1. Bind the output FBO (or screen for the last pass)
  2. Bind the input texture(s) from previous passes
  3. Use the appropriate shader for this effect
  4. Draw a full-screen quad
```

This is called a **render pipeline** or **post-processing chain**. Each effect is a link in the chain. You can add, remove, or reorder effects by changing which shaders are used in which pass.

### The Full HD-2D Post-Processing Chain

Here is the complete chain that Octopath Traveler-style games use, from scene rendering to final pixel output:

```
HD-2D Post-Processing Pipeline
================================

  Step 1: SCENE RENDER
    Bind: FBO_Scene
    Draw: all sprites, tilemaps, particles, UI
    Output: Full scene as a texture

    +-----------------------+
    | [mountains]           |
    | [trees]  [torch]      |
    |       [player]        |
    | [grass] [grass]       |
    +-----------------------+

         |
         v

  Step 2: BRIGHT EXTRACTION
    Bind: FBO_Bright
    Input: FBO_Scene texture
    Shader: bloom_extract.frag (threshold = 0.8)
    Output: Only pixels above brightness threshold

    +-----------------------+
    |                       |
    |         [torch]       |   <-- only the torch is bright enough
    |                       |
    |                       |
    +-----------------------+

         |
         v

  Step 3: HORIZONTAL BLUR
    Bind: FBO_BlurH
    Input: FBO_Bright texture
    Shader: gaussian_blur.frag (direction = horizontal)
    Output: Horizontally smeared bright pixels

    +-----------------------+
    |                       |
    |    =====[torch]=====  |
    |                       |
    |                       |
    +-----------------------+

         |
         v

  Step 4: VERTICAL BLUR
    Bind: FBO_BlurV
    Input: FBO_BlurH texture
    Shader: gaussian_blur.frag (direction = vertical)
    Output: Fully blurred bright pixels (the bloom texture)

    +-----------------------+
    |         .::.          |
    |       .:[torch]:.     |
    |         .::.          |
    |                       |
    +-----------------------+

         |
         v

  Step 5: COMPOSITE + VIGNETTE + COLOR GRADING
    Bind: Screen (default framebuffer)
    Inputs: FBO_Scene texture + FBO_BlurV texture
    Shader: final_composite.frag
    Output: Final image on screen

    +-----------------------+
    |##     .::.          ##|   <-- vignette darkens edges
    |#    .:[torch]:.      #|   <-- bloom adds glow
    |        [player]       |   <-- scene is visible
    |#  [grass] [grass]    #|   <-- color grading shifts hue
    |##                   ##|
    +-----------------------+
```

Here is a complete final composite shader that applies bloom, vignette, and color grading in a single pass:

```glsl
// final_composite.frag -- Combines all post-processing effects
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

// Input textures from previous passes
uniform sampler2D sceneTexture;    // Original rendered scene
uniform sampler2D bloomTexture;    // Blurred bright pixels

// Bloom controls
uniform float uBloomIntensity;     // Bloom strength (try 0.5 - 1.5)

// Vignette controls
uniform float uVignetteRadius;     // Clear area radius (try 0.6)
uniform float uVignetteSoftness;   // Fade distance (try 0.3)
uniform float uVignetteStrength;   // Darkening amount (try 0.5)

// Color grading controls
uniform float uContrast;           // Contrast multiplier (try 1.0 - 1.3)
uniform float uSaturation;         // Saturation multiplier (try 0.8 - 1.5)
uniform vec3 uColorTint;           // Overall color tint (e.g., warm = vec3(1.1, 1.0, 0.9))

void main() {
    // --- BLOOM ---
    vec3 sceneColor = texture(sceneTexture, TexCoord).rgb;
    vec3 bloomColor = texture(bloomTexture, TexCoord).rgb;
    vec3 color = sceneColor + bloomColor * uBloomIntensity;

    // --- COLOR GRADING ---
    // Contrast: push colors away from or toward middle gray
    color = (color - 0.5) * uContrast + 0.5;

    // Saturation: increase or decrease color intensity
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, uSaturation);

    // Color tint: multiply by a slight color shift
    color *= uColorTint;

    // --- VIGNETTE ---
    vec2 center = TexCoord - vec2(0.5);
    float dist = length(center);
    float vignette = smoothstep(uVignetteRadius, uVignetteRadius + uVignetteSoftness, dist);
    color *= 1.0 - vignette * uVignetteStrength;

    // --- TONE MAPPING ---
    // Reinhard tone mapping prevents colors from exceeding 1.0
    color = color / (color + vec3(1.0));

    // --- GAMMA CORRECTION ---
    // Convert from linear to sRGB space for correct display
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
```

---

## 6. Combining Shader Effects with Sprite Animation

Shader effects are **additive to** sprite animation, not a replacement for it. The most compelling visual results come from layering both together. A walking character (frame-by-frame sprite animation) can simultaneously have an outline shader, damage flash, squash/stretch, and wind-blown hair -- all computed by the shader on top of the base sprite frames.

### Shader Switching by Entity State

The cleanest architectural approach is to use different shader programs for different visual states. Each shader handles one category of effect.

```
Entity State                Active Shader               Effect
-----------                 -------------               ------
Normal                      sprite_shader               Standard rendering
Damaged (just hit)          flash_shader                White/red flash
Poisoned                    poison_shader               Green tint + desaturation
Underwater                  water_shader                Wave distortion + blue tint
Selected (by cursor)        outline_shader              Colored outline
Dying                       dissolve_shader             Dissolve away
Invisible (stealth)         ghost_shader                Semi-transparent + shimmer
```

On the CPU side, shader switching looks like:

```cpp
// In your entity render function:
void Entity::render(/* parameters */) {
    Shader* activeShader = normalShader;   // Default

    if (isDamaged && flashTimer > 0.0f) {
        activeShader = flashShader;
        activeShader->use();
        activeShader->setFloat("uFlashAmount", flashTimer / flashDuration);
        activeShader->setVec3("uFlashColor", 1.0f, 1.0f, 1.0f);
    } else if (isPoisoned) {
        activeShader = poisonShader;
        activeShader->use();
        activeShader->setFloat("uTime", currentTime);
    } else if (isUnderwater) {
        activeShader = waterShader;
        activeShader->use();
        activeShader->setFloat("uTime", currentTime);
        activeShader->setFloat("uDistortionAmount", 0.01f);
    } else {
        activeShader->use();
    }

    // Set common uniforms (model, view, projection) regardless of which shader
    activeShader->setMat4("model", modelMatrix);
    activeShader->setMat4("view", viewMatrix);
    activeShader->setMat4("projection", projectionMatrix);

    // Draw the sprite -- the bound shader determines the visual effect
    sprite->draw();
}
```

### Combining Multiple Effects in One Shader

Sometimes you need multiple effects simultaneously (e.g., a poisoned character who also takes damage should flash AND have a green tint). Rather than creating every possible combination of shaders, you can build a "multi-effect" shader with toggles:

```glsl
// multi_effect.frag -- Fragment shader with multiple toggleable effects
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;

// Effect toggles and parameters
uniform float uFlashAmount;       // 0.0 = off, 1.0 = full flash
uniform vec3 uFlashColor;         // Flash color (white or red)

uniform float uTintAmount;        // 0.0 = off, 1.0 = full tint
uniform vec3 uTintColor;          // Tint color (green for poison, blue for ice)

uniform float uDesaturation;      // 0.0 = full color, 1.0 = grayscale

uniform float uAlphaOverride;     // 1.0 = normal, 0.5 = semi-transparent

void main() {
    vec4 texColor = texture(spriteTexture, TexCoord);

    if (texColor.a < 0.01) {
        discard;
    }

    vec3 color = texColor.rgb;

    // Apply desaturation (if any)
    if (uDesaturation > 0.0) {
        float gray = dot(color, vec3(0.2126, 0.7152, 0.0722));
        color = mix(color, vec3(gray), uDesaturation);
    }

    // Apply color tint (if any)
    if (uTintAmount > 0.0) {
        color = mix(color, color * uTintColor, uTintAmount);
    }

    // Apply flash (if any) -- flash overrides tint because it should be the top-level effect
    if (uFlashAmount > 0.0) {
        color = mix(color, uFlashColor, uFlashAmount);
    }

    FragColor = vec4(color, texColor.a * uAlphaOverride);
}
```

The tradeoff: a multi-effect shader does more work per pixel (evaluating all conditions), but it avoids the complexity of managing many specialized shader programs. For 2D games with modest pixel counts, the extra GPU cost is negligible.

---

## 7. Performance

### Fragment Shader Complexity

The GPU executes your fragment shader once per pixel that the sprite covers. A 64x64 sprite at 3x scale = 192x192 pixels = 36,864 fragment shader invocations per sprite. Multiply by the number of sprites on screen.

```
Cost factors per fragment shader invocation:

  Cheapest:
    - Math operations (add, multiply, sin, cos)     ~1 cycle each
    - Reading uniform values                         ~1 cycle
    - Comparing values (if/else)                     ~1-4 cycles

  Moderate:
    - Texture sampling (one sample)                  ~4-10 cycles
    - The outline shader samples 8 neighbors         ~8x the texture cost

  Expensive:
    - Dependent texture reads (UV computed from       ~10-20 cycles
      another texture sample, like palette lookup)
    - Loops with many iterations                      varies
    - Discard/early fragment termination              can break GPU parallelism
```

For context, a modern GPU handles roughly 10 billion texture samples per second. Even the most expensive 2D shader effects are trivial for the hardware.

### Overdraw

Overdraw occurs when multiple sprites overlap on screen. The fragment shader runs for EVERY pixel of EVERY sprite that covers a screen pixel, even if a later sprite completely obscures an earlier one.

```
Three overlapping sprites:

  Sprite 1 (back):    Sprite 2 (middle):   Sprite 3 (front):
  +--------+
  |xxxxxxxx|
  |xxxx+--------+
  |xxxx|yyyyyyyy|
  +----+yyyy+--------+
       |yyyy|zzzzzzzz|
       +----+zzzzzzzz|
            |zzzzzzzz|
            +--------+

The "x" area at top-left: fragment shader runs 1 time
The overlapping area:      fragment shader runs 2-3 times
The "z" area at bottom:    fragment shader runs 1 time

Total GPU work is greater than the visible pixel count.
```

Mitigations:
- **Use `discard` for fully transparent pixels.** This tells the GPU to skip blending and further processing for those fragments. Your current shaders already do this.
- **Draw front-to-back** with depth testing enabled. The GPU's early-Z test will skip fragment shader execution for pixels already covered by a closer sprite. However, this conflicts with alpha blending for semi-transparent sprites, so it is usually only viable for fully opaque geometry.
- **Minimize sprite quad sizes.** A 32x32 pixel art character should not use a 256x256 quad with mostly transparent space. Tight bounding quads reduce unnecessary fragment shader invocations.

### Post-Processing Cost

Post-processing shaders run on a full-screen quad, so their cost scales with **screen resolution**, not scene complexity. A scene with 1 sprite or 1000 sprites takes the same post-processing time.

```
Post-processing cost at different resolutions:

  Resolution      Pixels       Fragments per effect pass
  800x600         480,000      480K
  1280x720        921,600      ~1M
  1920x1080       2,073,600    ~2M
  2560x1440       3,686,400    ~3.7M
  3840x2160       8,294,400    ~8.3M

Each post-processing pass processes ALL these pixels.
5 passes (bloom pipeline) at 1080p = ~10M fragment invocations.
Still trivial for a modern GPU.
```

### Profiling with OpenGL Timer Queries

To measure how long the GPU actually spends on your shader effects, use OpenGL timer queries:

```cpp
// Create timer query objects
GLuint queryStart, queryEnd;
glGenQueries(1, &queryStart);
glGenQueries(1, &queryEnd);

// Bracket the rendering code you want to measure
glQueryCounter(queryStart, GL_TIMESTAMP);

// ... your rendering code here (draw calls, shader effects) ...

glQueryCounter(queryEnd, GL_TIMESTAMP);

// Wait for results (do this next frame to avoid stalling)
GLuint64 startTime, endTime;
glGetQueryObjectui64v(queryStart, GL_QUERY_RESULT, &startTime);
glGetQueryObjectui64v(queryEnd, GL_QUERY_RESULT, &endTime);

float gpuTimeMs = (endTime - startTime) / 1000000.0f;
// Print or log gpuTimeMs -- this is the actual GPU time for that section
```

---

## 8. HD-2D Shader Techniques (Octopath Traveler Analysis)

The "HD-2D" visual style pioneered by Octopath Traveler achieves its distinctive look through a specific combination of shader techniques applied to pixel art. This section breaks down each technique and explains how they combine.

### Depth-of-Field (Tilt-Shift Blur)

Octopath Traveler applies selective blur based on the vertical position on screen (a tilt-shift camera simulation). Objects at the top and bottom of the screen are blurred, while the middle band where the player exists remains sharp.

```
Tilt-Shift Depth of Field:

  +---------------------------+
  | ░░░░ blurred ░░░░░░░░░░░ |  <-- far background (blurred)
  | ░░░ blurred ░░░░░░░░░░░░ |
  |                           |  <-- transition zone (partially blurred)
  |       SHARP FOCUS         |  <-- player area (sharp)
  |       [player]            |
  |                           |  <-- transition zone
  | ░░░ blurred ░░░░░░░░░░░░ |  <-- near foreground (blurred)
  +---------------------------+

Blur amount is a function of distance from the focus band:
  blurAmount = smoothstep(focusStart, focusEnd, abs(screenY - focusCenter))
```

This is implemented as a post-processing pass. The blur amount is passed as a per-pixel value computed from the screen Y coordinate (or from a depth buffer if you have one):

```glsl
// tilt_shift.frag -- Tilt-shift depth-of-field effect
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D sceneTexture;
uniform float uFocusCenter;     // Y position of sharp focus (0.0-1.0, try 0.5)
uniform float uFocusRange;      // Half-height of the sharp band (try 0.15)
uniform float uMaxBlur;         // Maximum blur radius in texels (try 3.0)
uniform vec2 uTexelSize;        // vec2(1.0/screenWidth, 1.0/screenHeight)

void main() {
    // Calculate how far this pixel is from the focus band
    float distFromFocus = abs(TexCoord.y - uFocusCenter);
    float blurFactor = smoothstep(uFocusRange, uFocusRange + 0.2, distFromFocus);

    // If in the focus band, render sharply
    if (blurFactor < 0.01) {
        FragColor = texture(sceneTexture, TexCoord);
        return;
    }

    // Apply a simple box blur scaled by the blur factor
    float blurRadius = uMaxBlur * blurFactor;
    vec4 color = vec4(0.0);
    float totalWeight = 0.0;

    // Sample in a small area around the current pixel
    for (float x = -blurRadius; x <= blurRadius; x += 1.0) {
        for (float y = -blurRadius; y <= blurRadius; y += 1.0) {
            float weight = 1.0 - length(vec2(x, y)) / (blurRadius * 1.414);
            weight = max(weight, 0.0);
            vec2 offset = vec2(x, y) * uTexelSize;
            color += texture(sceneTexture, TexCoord + offset) * weight;
            totalWeight += weight;
        }
    }

    FragColor = color / totalWeight;
}
```

**Note:** The above uses a nested loop, which is expensive. A production implementation would use the separable Gaussian blur approach (two passes, as described in the bloom section) with a per-pixel blur radius. The above code is for conceptual clarity.

### Dynamic Point Lights

Each light source in the scene (torches, campfires, lanterns, magic spells) affects nearby sprites. This creates pools of warm light in an otherwise dim or cool-toned world.

```
Scene without point lights:          Scene with point lights:

  +-----------------------+          +-----------------------+
  | [torch]  [NPC]        |          | [TORCH]  [npc]        |
  |                       |          |  .:'':.               |
  |       [player]        |          |       [player]        |
  |                       |          |                       |
  | [lamp]         [tree] |          | [LAMP]         [tree] |
  +-----------------------+          |  .:'':.               |
  Everything same brightness         +-----------------------+
                                     Lights create warm pools,
                                     unlit areas are darker.
```

Point lights are typically implemented in the fragment shader by passing an array of light positions, colors, and radii. Each pixel computes its total illumination by summing contributions from all nearby lights:

```glsl
// point_lights.frag -- Dynamic point light illumination
#version 330 core

in vec2 TexCoord;
in vec2 WorldPos;    // The world-space position of this pixel
                      // (passed from vertex shader as a varying)
out vec4 FragColor;

uniform sampler2D spriteTexture;

// Ambient lighting (baseline illumination -- prevents pure black shadows)
uniform vec3 uAmbientColor;    // e.g., vec3(0.15, 0.1, 0.2) for a cool night

// Point light array
#define MAX_LIGHTS 16
uniform int uNumLights;                      // How many lights are active
uniform vec2 uLightPositions[MAX_LIGHTS];    // World-space position of each light
uniform vec3 uLightColors[MAX_LIGHTS];       // Color of each light (warm orange, cool blue, etc.)
uniform float uLightRadii[MAX_LIGHTS];       // How far each light reaches
uniform float uLightIntensities[MAX_LIGHTS]; // Brightness multiplier

void main() {
    vec4 texColor = texture(spriteTexture, TexCoord);

    if (texColor.a < 0.01) {
        discard;
    }

    // Start with ambient lighting
    vec3 totalLight = uAmbientColor;

    // Accumulate contribution from each point light
    for (int i = 0; i < uNumLights; i++) {
        // Distance from this pixel to the light
        float dist = distance(WorldPos, uLightPositions[i]);

        // Attenuation: light falls off with distance
        // smoothstep creates a smooth falloff from full brightness to zero
        float attenuation = 1.0 - smoothstep(0.0, uLightRadii[i], dist);

        // Add this light's contribution
        totalLight += uLightColors[i] * uLightIntensities[i] * attenuation;
    }

    // Apply lighting to the sprite color
    // Multiply blending: dark areas stay dark, bright areas take on light color
    FragColor = vec4(texColor.rgb * totalLight, texColor.a);
}
```

To pass the world-space position to the fragment shader, modify the vertex shader:

```glsl
// point_lights.vert -- Vertex shader that passes world position
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
out vec2 WorldPos;    // World-space position for lighting calculations

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // Compute world-space position (before view/projection transforms)
    vec4 worldPosition = model * vec4(aPos, 0.0, 1.0);
    WorldPos = worldPosition.xy;

    // Standard clip-space position
    gl_Position = projection * view * worldPosition;

    TexCoord = aTexCoord;
}
```

### God Rays / Volumetric Light

God rays are shafts of light that appear to stream through openings (windows, gaps in foliage, holes in cave ceilings). In Octopath Traveler, these are prominent in forest and cathedral scenes.

A simplified 2D approach uses radial blur from the light source position:

```
God rays concept:

  Light source at top-center
         *
        /|\
       / | \
      /  |  \        Rays radiate outward from the source.
     /   |   \       Pixels between you and the source are
    / [trees] \      "bright". Pixels behind opaque objects
   /     |     \     are "dark" (in shadow).
  /   [player]  \
```

```glsl
// god_rays.frag -- Simplified radial god rays (post-processing)
#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D sceneTexture;
uniform vec2 uLightScreenPos;    // Light source position in screen UV space (0-1)
uniform float uRayDensity;       // Number of samples along each ray (try 32.0 - 100.0)
uniform float uRayDecay;         // How quickly rays fade (try 0.96 - 0.99)
uniform float uRayWeight;        // Contribution of each sample (try 0.01 - 0.05)
uniform float uRayExposure;      // Overall brightness (try 0.3 - 1.0)

void main() {
    // Direction from this pixel toward the light source
    vec2 deltaTexCoord = (TexCoord - uLightScreenPos);
    deltaTexCoord *= 1.0 / uRayDensity;  // Step size along the ray

    vec2 samplePos = TexCoord;
    vec4 color = texture(sceneTexture, samplePos);
    float illuminationDecay = 1.0;

    // March along the ray toward the light source, accumulating brightness
    for (float i = 0.0; i < uRayDensity; i += 1.0) {
        samplePos -= deltaTexCoord;
        vec4 sampleColor = texture(sceneTexture, samplePos);

        // Accumulate: each sample along the ray contributes to the ray brightness
        sampleColor *= illuminationDecay * uRayWeight;
        color += sampleColor;

        // Decay: samples further from this pixel contribute less
        illuminationDecay *= uRayDecay;
    }

    FragColor = color * uRayExposure;
}
```

### Normal-Mapped Sprites

Normal mapping gives pixel art sprites the appearance of 3D surface detail by storing per-pixel surface direction information. Each sprite has TWO textures: the regular color texture (diffuse) and a normal map texture that encodes which direction each pixel's surface faces.

```
Diffuse texture (what you see):     Normal map (surface directions):

  +--------+                         +--------+
  | @@@@   |                         | ^^>>   |   ^ = pointing up
  | @@@@@@ |                         | ^>v>^> |   > = pointing right
  | @@@@@@ |                         | ^>vv>^ |   v = pointing down
  | @@@@   |                         | ^^>>   |   < = pointing left
  +--------+                         +--------+

The normal map is stored as an RGB image:
  R channel = X direction (-1 to 1, stored as 0 to 255)
  G channel = Y direction (-1 to 1, stored as 0 to 255)
  B channel = Z direction (0 to 1, stored as 128 to 255)

A flat surface facing the camera has normal (0, 0, 1) = RGB(128, 128, 255)
which is the characteristic blue/purple color you see in normal map images.
```

When a point light hits a normal-mapped sprite, pixels whose normals face TOWARD the light are brightened, and pixels whose normals face AWAY are darkened. This creates the illusion of 3D bumps and grooves on a flat sprite.

```glsl
// normal_mapped.frag -- Fragment shader with per-pixel normal-mapped lighting
#version 330 core

in vec2 TexCoord;
in vec2 WorldPos;
out vec4 FragColor;

uniform sampler2D spriteTexture;    // Color/diffuse texture
uniform sampler2D normalTexture;    // Normal map texture

uniform vec3 uAmbientColor;
uniform vec2 uLightPos;            // World-space light position
uniform vec3 uLightColor;
uniform float uLightRadius;
uniform float uLightIntensity;

void main() {
    vec4 texColor = texture(spriteTexture, TexCoord);

    if (texColor.a < 0.01) {
        discard;
    }

    // Sample the normal map and convert from [0,1] range to [-1,1] range
    vec3 normal = texture(normalTexture, TexCoord).rgb;
    normal = normalize(normal * 2.0 - 1.0);

    // Calculate light direction (from pixel toward light, in 2D + assumed Z)
    // We treat the light as being slightly in front of the sprite (Z > 0)
    vec3 lightDir = vec3(uLightPos - WorldPos, 50.0);  // 50.0 = light Z height
    float dist = length(lightDir);
    lightDir = normalize(lightDir);

    // Attenuation based on distance
    float attenuation = 1.0 - smoothstep(0.0, uLightRadius, length(uLightPos - WorldPos));

    // Diffuse lighting: dot product of surface normal and light direction
    // If the surface faces the light (positive dot product), it is lit
    // If the surface faces away (negative), it is in shadow (clamped to 0)
    float diffuse = max(dot(normal, lightDir), 0.0);

    // Final lighting
    vec3 lighting = uAmbientColor + uLightColor * uLightIntensity * diffuse * attenuation;

    FragColor = vec4(texColor.rgb * lighting, texColor.a);
}
```

### How These Techniques Combine

The HD-2D look is not any single technique -- it is the layered combination of all of them applied to simple pixel art:

```
Layer 1: Pixel art sprites and tiles
  The base content. Drawn by artists in a retro style.
  Uses GL_NEAREST filtering to keep pixels crisp.

Layer 2: Dynamic point lighting
  Torches, lanterns, and ambient color create pools of light and shadow.
  Applied per-sprite in the fragment shader.

Layer 3: Normal-mapped lighting (optional per sprite)
  Key objects (stone walls, wooden beams, character faces) get normal maps
  that interact with the dynamic lights, creating depth on flat sprites.

Layer 4: Post-processing bloom
  Bright light sources (fire, magic, sun reflections) bleed glow into
  surrounding pixels, creating a warm, soft atmosphere.

Layer 5: Depth-of-field blur
  Objects far from the player (background, foreground) are blurred,
  directing focus to the gameplay area.

Layer 6: Vignette
  Screen edges darken, drawing the eye inward. Subtle but effective.

Layer 7: Color grading
  A warm color shift (boosted reds, reduced blues) creates the "golden hour"
  feeling characteristic of Octopath Traveler.

Layer 8: Ambient occlusion (advanced)
  Darkening in corners and crevices where objects meet. In 2D, this can be
  approximated by drawing shadow sprites at object bases or using SSAO
  on a depth buffer.

Together:

  Pixel art alone:                   Pixel art + HD-2D pipeline:
  +-----------------+                +-----------------+
  | Simple, flat,   |                | Atmospheric,    |
  | retro look      |      -->       | cinematic,      |
  | (SNES style)    |                | depth-rich      |
  +-----------------+                | ("what retro    |
                                     |  games looked   |
                                     |  like in our    |
                                     |  imagination")  |
                                     +-----------------+
```

The beauty of this approach is that each technique is **independent and composable**. You can add them one at a time: start with just bloom, then add vignette, then add lighting, and so on. Each layer adds visual richness without requiring changes to the others.

### Implementation Priority for Your Engine

Based on visual impact vs. implementation complexity:

```
Priority  Technique                    Impact    Complexity    Dependencies
--------  ---------------------------  --------  -----------   ------------
1         Color flash (damage)         High      Very Low      None
2         Vignette                     Medium    Low           FBO
3         Bloom                        Very High Medium        FBO, multi-pass
4         Point lights                 Very High Medium        World coords in shader
5         Depth-of-field               High      Medium        FBO, blur
6         UV distortion (water)        Medium    Low           Time uniform
7         Wind effect (vegetation)     Medium    Low           Time uniform
8         Dissolve effect              Medium    Low-Medium    Noise texture
9         Outline                      Medium    Low           Texel size
10        Color grading                Medium    Low           FBO
11        Normal-mapped lighting       High      High          Normal map textures
12        God rays                     High      High          FBO, ray marching
13        Chromatic aberration         Low-Med   Low           FBO

Start with items 1-3. They give the most dramatic visual improvement
for the least implementation effort. Items 4-6 bring you solidly into
HD-2D territory. Items 7-13 are polish and advanced techniques.
```

---

## Summary

Shader-based animation is the technique of creating visual change through mathematical computation in GLSL shaders rather than by swapping between pre-made images. A continuously increasing time uniform is the foundation -- it drives sine waves, threshold comparisons, UV offsets, and every other time-varying effect.

The techniques fall into four categories:
- **UV manipulation** (scrolling, distortion, rotation) changes WHERE the texture is sampled
- **Vertex displacement** (waves, wind, squash/stretch) changes WHERE the geometry appears
- **Fragment color effects** (flash, outline, dissolve, pixelation) changes WHAT COLOR each pixel is
- **Post-processing** (bloom, vignette, color grading, depth-of-field) applies effects to the ENTIRE rendered scene via framebuffer objects

These techniques are additive to traditional frame-by-frame sprite animation. A walking character uses frame-by-frame for the body motion and shader effects for damage flash, wind-blown details, outlines, and environmental tinting. Different shaders (or a multi-effect shader with toggles) are selected based on entity state.

The HD-2D visual style of Octopath Traveler is achieved by combining these techniques: pixel art rendered with GL_NEAREST filtering, dynamic point lights, bloom on bright areas, tilt-shift depth-of-field, vignette, and warm color grading. Each technique is independent and can be added incrementally.

For your engine, start with the damage flash (trivial to add, immediately useful), then set up the FBO pipeline for post-processing, then add bloom and vignette. From there, point lighting and depth-of-field will bring the engine into full HD-2D territory.
