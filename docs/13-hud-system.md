# HUD System: Health/Stamina Bars and Debug Text

This document explains how to build a heads-up display (HUD) for a 2D game — fixed-position UI elements like health bars, stamina bars, and debug text that stay on screen regardless of camera movement.

## Table of Contents
- [Screen Space vs World Space](#screen-space-vs-world-space)
- [Rendering Colored Rectangles (Health/Stamina Bars)](#rendering-colored-rectangles-healthstamina-bars)
- [Text Rendering With SDL_ttf](#text-rendering-with-sdl_ttf)
- [HUD Class Architecture](#hud-class-architecture)
- [Implementation Details](#implementation-details)

---

## Screen Space vs World Space

### The Problem

Everything rendered through the camera moves when the player moves. The view matrix transforms all geometry relative to the camera position — this is **world space** rendering. Tiles, sprites, enemies — they all exist at fixed world coordinates and shift on screen as you scroll.

A HUD is fundamentally different. Health bars, score text, minimaps — these must stay **fixed on screen** no matter where the camera is looking.

### Web Development Analogy

```
World space = position: relative
    Elements scroll with the page.
    When the user scrolls down, content moves up.

Screen space = position: fixed
    Elements stay pinned to the viewport.
    Scrolling has no effect on their position.
```

A health bar at the bottom-right of the screen is like a `position: fixed; bottom: 40px; right: 40px;` element in CSS. The "page" (game world) scrolls underneath it, but the bar never moves.

### The Solution: Skip the View Matrix

The current rendering pipeline applies three matrix transforms in the vertex shader:

```glsl
gl_Position = projection * view * model * vec4(position, 0.0, 1.0);
```

| Matrix | Purpose |
|--------|---------|
| `projection` | Maps pixel coordinates to OpenGL's -1 to 1 clip space |
| `view` | Camera transform — offsets everything based on camera position |
| `model` | Object's own position and scale in the world |

For HUD elements, you remove the view matrix entirely:

```glsl
gl_Position = projection * model * vec4(position, 0.0, 1.0);
```

Now `model` positions things directly in **screen pixel coordinates**. If you place something at (1856, 860), it stays at that screen position regardless of where the camera is looking in the world.

### Why This Works

The orthographic projection is already set up to map pixel coordinates:

```cpp
glm::ortho(0.0f, viewportWidth, viewportHeight, 0.0f)
```

This says: "left edge is pixel 0, right edge is pixel 1920 (or whatever the viewport width is), top is pixel 0, bottom is pixel 1080."

With the view matrix included, pixel coordinates get shifted by the camera offset before this mapping. Without the view matrix, pixel coordinates go straight through — (100, 200) in the model matrix means pixel (100, 200) on screen. Period.

```
With view matrix (world space):
    Screen pixel = projection × camera_offset × world_position
    Position on screen CHANGES as camera moves

Without view matrix (screen space):
    Screen pixel = projection × screen_position
    Position on screen is FIXED
```

---

## Rendering Colored Rectangles (Health/Stamina Bars)

### Why You Need a New Shader

Existing shaders in the engine all sample from textures — they expect UV coordinates and a `sampler2D` to look up pixel colors from an image. Health/stamina bars are just **solid colored rectangles** — no texture needed.

The `debug_line` shaders are close (they render untextured geometry), but they:
1. Use `GL_LINES` — designed for wireframe, not filled shapes
2. Include a view matrix — they render in world space
3. Use `vec3` color — no alpha/transparency control

The bar shader needs:
1. **Filled quads** (`GL_TRIANGLES`, not `GL_LINES`)
2. **Screen space** (no view matrix)
3. **Per-draw color control** (`vec4` uniform for RGBA, so you can have semi-transparent backgrounds)

### The Unit Quad Pattern

This is the same pattern used in `Sprite::setupMesh()` — a quad from (0,0) to (1,1) made of 6 vertices (2 triangles):

```
(0,0)──────(1,0)        Triangle 1: (0,0), (1,0), (1,1)
  │ ╲        │          Triangle 2: (0,0), (1,1), (0,1)
  │   ╲      │
  │     ╲    │          Together they form a filled rectangle.
  │       ╲  │
(0,1)──────(1,1)
```

The key insight: **create the geometry once, then scale and position it with the model matrix each draw call**. This is exactly how the Sprite class works — one unit quad, reused for every sprite.

To draw a 24x180 pixel bar at position (1856, 860):

```cpp
glm::mat4 model = glm::mat4(1.0f);                           // Start with identity
model = glm::translate(model, glm::vec3(1856, 860, 0));      // Move to screen position
model = glm::scale(model, glm::vec3(24, 180, 1));            // Scale to bar dimensions
```

This stretches the unit quad from 1x1 to 24x180 pixels, positioned at (1856, 860). Same as Sprite::draw() but without multiplying by the view matrix.

### Web Development Analogy

```css
/* The unit quad is like a div with width: 1px; height: 1px; */
/* The model matrix is like transform: translate() scale() */

.health-bar {
    position: fixed;                    /* ← no view matrix (screen space) */
    bottom: 40px;
    right: 40px;
    width: 24px;                        /* ← glm::scale x */
    height: 180px;                      /* ← glm::scale y */
    background-color: rgba(255, 0, 0);  /* ← barColor uniform */
}
```

### Drawing a Bar With Fill Level

Each bar is actually **two quads drawn on top of each other**:

1. **Background quad** — full size, dark muted color, semi-transparent
2. **Fill quad** — partial height based on current value, bright color

```
100% health:          50% health:           0% health:
┌────────────┐       ┌────────────┐        ┌────────────┐
│ ░░░░░░░░░░ │       │ ░░░░░░░░░░ │        │ ░░░░░░░░░░ │
│ ░░░░░░░░░░ │       │ ░░░░░░░░░░ │        │ ░░░░░░░░░░ │
│ ████████ │       │ ░░░░░░░░░░ │        │ ░░░░░░░░░░ │
│ ████████ │       │ ░░░░░░░░░░ │        │ ░░░░░░░░░░ │
│ ████████ │       │ ████████ │        │ ░░░░░░░░░░ │
│ ████████ │       │ ████████ │        │ ░░░░░░░░░░ │
└────────────┘       └────────────┘        └────────────┘
  fill = full         fill = half           fill = empty
  ░ = background (dark red, semi-transparent)
  █ = fill (bright red, fully opaque)
```

The fill grows **from the bottom up** (Stardew Valley style). In the engine's coordinate system, Y=0 is the top of the screen, so "bottom" means higher Y values. The math:

```
fillHeight = totalHeight * (currentHealth / maxHealth)
fillY      = barY + totalHeight - fillHeight
```

Walking through the math:

```
Given: barY = 860, totalHeight = 180, maxHealth = 100

At 100% health (currentHealth = 100):
    fillHeight = 180 * (100/100) = 180
    fillY      = 860 + 180 - 180 = 860     ← fill starts at top of bar

At 50% health (currentHealth = 50):
    fillHeight = 180 * (50/100) = 90
    fillY      = 860 + 180 - 90 = 950      ← fill starts halfway down

At 0% health (currentHealth = 0):
    fillHeight = 180 * (0/100) = 0
    fillY      = 860 + 180 - 0 = 1040      ← fill starts at bottom (nothing visible)
```

### The Bar Shader Pair

**Vertex shader (`hud_bar.vert`):**

```glsl
#version 330 core
layout (location = 0) in vec2 aPos;

uniform mat4 model;
uniform mat4 projection;
// Note: NO view uniform — this is the key difference for screen-space rendering

void main() {
    gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
}
```

**Fragment shader (`hud_bar.frag`):**

```glsl
#version 330 core
out vec4 FragColor;

uniform vec4 barColor;  // vec4 not vec3, so you can control alpha (transparency)

void main() {
    FragColor = barColor;
}
```

Compare this to the `debug_line` shaders — almost identical structure but with a `model` matrix added (for scaling/positioning the quad) and `vec4` instead of `vec3` for color (so background bars can be semi-transparent).

### Bar Layout

The bars sit in the bottom-right corner of the screen, Stardew Valley style:

```
Screen (1920 x 1080):
┌──────────────────────────────────────────────────────┐
│                                                      │
│                                                      │
│                                                      │
│                                                      │
│                                          Stamina  Health
│                                          (yellow) (red)
│                                          ┌────┐  ┌────┐
│                                          │    │  │    │
│                                          │████│  │████│
│                                          │████│  │████│
│                                          └────┘  └────┘
│                                           24px    24px
│                                             12px gap
│                                                   40px from edge
└──────────────────────────────────────────────────────┘
```

The positions are calculated each frame from the current viewport size so the bars adapt when the window is resized:

```cpp
float healthX  = viewportWidth - 40 - 24;    // 40px from right edge, 24px bar width
float healthY  = viewportHeight - 40 - 180;  // 40px from bottom edge, 180px bar height
float staminaX = healthX - 12 - 24;          // 12px gap + another 24px bar width to the left
float staminaY = healthY;                     // same vertical position
```

### Blending for Transparency

The background bars use semi-transparent colors (like dark red with 50% opacity). This requires OpenGL blending to be enabled:

```cpp
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

This is the same blend mode used for sprite transparency. The formula:

```
final_color = source_alpha * source_color + (1 - source_alpha) * destination_color
```

So a bar with `barColor = vec4(0.5, 0.0, 0.0, 0.5)` (dark red, 50% alpha) will blend with whatever is behind it — you'll see the game world dimly through the background bar.

---

## Text Rendering With SDL_ttf

### Why Text Is Hard in OpenGL

In web development, rendering text is trivial:

```html
<p style="font-family: Cinzel; font-size: 24px; color: white;">Farm</p>
```

The browser handles:
- Loading the font file
- Rasterizing glyphs (converting vector curves to pixels)
- Layout (spacing, kerning, line breaks)
- Rendering to the screen

OpenGL has **none of that**. It only knows how to draw triangles and sample textures. To display text, you have to:

1. **Load a font file** (.ttf) and parse its glyph data
2. **Rasterize** the text string into a pixel buffer (a bitmap image)
3. **Upload** those pixels to an OpenGL texture on the GPU
4. **Draw** a textured quad using that texture

SDL_ttf handles steps 1 and 2 — it takes a `.ttf` font file and a string, and produces an `SDL_Surface*` containing the rendered text as pixels. From there, it's the same `SDL_Surface → glTexImage2D` pipeline that `Texture.cpp` already uses.

### The Pipeline

```
"Farm" (string)
    │
    ▼  TTF_RenderUTF8_Blended(font, "Farm", white)
SDL_Surface* (pixel buffer with anti-aliased rendered text)
    │
    ▼  SDL_ConvertSurfaceFormat(..., SDL_PIXELFORMAT_RGBA32)
SDL_Surface* (converted to RGBA format for OpenGL)
    │
    ▼  glTexImage2D(GL_TEXTURE_2D, ..., GL_RGBA, ..., surface->pixels)
OpenGL Texture (text image now lives on the GPU)
    │
    ▼  draw textured quad with screen-space shader
Text on screen!
```

This is the exact same flow as the `Texture` class constructor:

```
Texture.cpp:     IMG_Load() → convert surface → glTexImage2D()
HUD text:        TTF_Render() → convert surface → glTexImage2D()
```

The only difference is the source of the pixels: SDL_ttf (from font rendering) instead of SDL_image (from a PNG file).

### Web Development Analogy

Think of SDL_ttf as a **server-side rendering** step for text:

```
Web SSR:
    React component → renderToString() → HTML string → browser displays

OpenGL text:
    "Farm" string → TTF_RenderUTF8_Blended() → pixel buffer → GPU texture → quad draws it
```

In both cases, you pre-render content into a format the display system can handle, then hand it off.

### Caching: Only Re-render When State Changes

The text texture is **dynamic** — it changes when you enter a new location. But it does NOT change every frame. Re-rendering text every frame would be wasteful (font rasterization is expensive compared to just binding an existing texture).

The solution: cache the texture and only re-render when the location changes.

```cpp
// Only re-render text when location actually changes
if (currentLocation->getId() != cachedLocationId) {
    cachedLocationId = currentLocation->getId();
    renderTextToTexture(currentLocation->getId());  // Expensive: rasterize + upload
}
// Every frame: just bind the cached texture and draw
drawTextQuad();  // Cheap: bind + draw call
```

This is the same pattern as React's rendering model:

```javascript
// React: only re-render component when props/state change
useEffect(() => {
    // Re-render only when locationId changes
    setRenderedText(renderLocationName(locationId));
}, [locationId]);  // dependency array = "only when this changes"
```

### Key SDL_ttf Functions

| Function | Purpose |
|----------|---------|
| `TTF_Init()` | Initialize the SDL_ttf library (call once at startup) |
| `TTF_OpenFont(path, size)` | Load a .ttf font file at a given point size |
| `TTF_RenderUTF8_Blended(font, text, color)` | Render text to an anti-aliased SDL_Surface |
| `TTF_CloseFont(font)` | Free font resources |
| `TTF_Quit()` | Shut down SDL_ttf |

`TTF_RenderUTF8_Blended` is the highest quality rendering mode — it produces anti-aliased text with a transparent background. There are cheaper modes (`Solid`, `Shaded`) but `Blended` looks best.

### The Text Shader Pair

Text needs a different shader than bars because it **samples from a texture** (the rasterized text image) rather than outputting a solid color:

**Vertex shader (`hud_text.vert`):**

```glsl
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 projection;

void main() {
    gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
```

**Fragment shader (`hud_text.frag`):**

```glsl
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D textTexture;

void main() {
    vec4 texel = texture(textTexture, TexCoord);
    if (texel.a < 0.01) discard;   // Throw away fully transparent pixels
    FragColor = texel;
}
```

This is essentially a simplified version of `sprite.vert/frag`:
- Same structure (vertex + UV in, texture sample out)
- No view matrix (screen-space positioning)
- No UV offset/size uniforms (the text texture maps 1:1 to the quad — no spritesheet sub-regions)
- `discard` on transparent pixels so the text background doesn't draw as a visible rectangle

### Centering Text Horizontally

SDL_ttf provides the pixel dimensions of the rendered text through the surface's `w` and `h` fields. To center at the top of the screen:

```cpp
float textX = (viewportWidth / 2.0f) - (textWidth / 2.0f);   // Center horizontally
float textY = 20.0f;                                           // 20px from top

glm::mat4 model = glm::mat4(1.0f);
model = glm::translate(model, glm::vec3(textX, textY, 0));
model = glm::scale(model, glm::vec3(textWidth, textHeight, 1));
```

The unit quad gets scaled to the text's exact pixel dimensions and positioned so it's centered:

```
                     viewportWidth
├────────────────────────────────────────────────┤

         textX = (viewportWidth - textWidth) / 2
         ├─────────┤
         ┌─────────┐  ← textY = 20px from top
         │  Farm   │
         └─────────┘
         ├─────────┤
          textWidth (from SDL_Surface->w)
```

### Font Choice

Octopath Traveller 2 uses an elegant serif font for its UI. Free alternatives that match the aesthetic:

| Font | Style | License | Source |
|------|-------|---------|--------|
| **Cinzel** | Fantasy/medieval serif, very close to OT2 | OFL (free) | Google Fonts |
| **EB Garamond** | Classic elegant serif | OFL (free) | Google Fonts |

Download the `.ttf` file and place it at `assets/fonts/default.ttf`.

### New Dependency: SDL_ttf

SDL_ttf is a companion library to SDL2 that provides TrueType font rendering. It needs to be installed and linked:

```bash
# Arch Linux
sudo pacman -S sdl2_ttf

# Ubuntu/Debian
sudo apt install libsdl2-ttf-dev
```

In the Makefile, add `-lSDL2_ttf` to the LIBS line:

```makefile
LIBS = -lSDL2 -lSDL2_image -lSDL2_ttf -lGL
```

---

## HUD Class Architecture

### Where It Fits in the Ownership Model

Following the existing ownership pattern where Game owns everything via `unique_ptr`:

```
Game (central orchestrator, owns all resources)
  ├── unique_ptr<Window>
  ├── unique_ptr<Camera>
  ├── unique_ptr<Shader>  (tileShader, spriteShader, debugLineShader)
  ├── unique_ptr<Shader>  (hudBarShader, hudTextShader)     ← NEW
  ├── unique_ptr<Player>
  ├── unique_ptr<HUD>                                        ← NEW
  └── ...
```

### What the HUD Owns vs References

**Owns (responsible for creating and destroying):**
- Bar VAO/VBO — the unit quad mesh for colored rectangles
- Text VAO/VBO — the unit quad mesh with UV coordinates for textured rendering
- `TTF_Font*` — the loaded font handle
- `GLuint textTextureID` — the cached OpenGL texture for rendered text
- Cached location ID — to detect when text needs re-rendering

**References (passed in, not owned):**
- Shaders — Game owns these, HUD receives them by reference
- Player — reads health/maxHealth, doesn't own the player
- Location — reads the location ID for debug text, doesn't own it
- Camera — reads viewport size for positioning calculations

This follows the same pattern as `Location::render(Shader&, Camera&)` — the class doing the rendering receives shared resources by reference rather than owning them.

### The render() Signature

```cpp
void HUD::render(Shader& barShader, Shader& textShader,
                 const Camera& camera,
                 const Player& player,
                 const Location* currentLocation,
                 bool debugMode);
```

### Render Order

The HUD renders **last** in the frame — after all world-space geometry but before the buffer swap. This guarantees the HUD draws on top of everything:

```cpp
void Game::render() {
    window->clear(0.1f, 0.1f, 0.2f);

    // 1. World-space rendering (affected by camera)
    currentLocation->render(*tileShader, *camera);
    for (auto& enemy : enemies) {
        enemy->render(*spriteShader, *camera);
    }
    player->render(*spriteShader, *camera);

    if (debugMode) {
        currentLocation->renderDebug(*debugLineShader, *camera);
    }

    // 2. Screen-space rendering (NOT affected by camera) — always last
    hud->render(*hudBarShader, *hudTextShader, *camera,
                *player, currentLocation, debugMode);

    window->swapBuffers();
}
```

### What render() Does Step by Step

```
1. Build screen-space projection matrix
   └── glm::ortho(0, viewportWidth, viewportHeight, 0) — same as always, no view matrix

2. Enable blending
   └── glEnable(GL_BLEND) — needed for semi-transparent bar backgrounds

3. Draw stamina bar (yellow, hardcoded 100%)
   ├── Background: dark yellow, semi-transparent, full 24×180
   └── Fill: bright yellow, full height (100%)

4. Draw health bar (red, reads player.getHealth() / player.getMaxHealth())
   ├── Background: dark red, semi-transparent, full 24×180
   └── Fill: bright red, height proportional to current health

5. If debugMode is on:
   ├── Check if location changed → re-render text texture if needed
   └── Draw centered text at top of screen

6. Disable blending
   └── glDisable(GL_BLEND) — clean up state for next frame
```

---

## Implementation Details

### Files Summary

**New files to create:**

| File | Purpose |
|------|---------|
| `src/HUD.h` | HUD class declaration |
| `src/HUD.cpp` | HUD implementation (bar drawing, text rendering) |
| `shaders/hud_bar.vert` | Screen-space vertex shader (position only, no view matrix) |
| `shaders/hud_bar.frag` | Solid color fragment shader (vec4 barColor uniform) |
| `shaders/hud_text.vert` | Screen-space vertex shader (position + UV, no view matrix) |
| `shaders/hud_text.frag` | Textured fragment shader (samples textTexture, discards transparent) |
| `assets/fonts/default.ttf` | Serif font file (Cinzel or EB Garamond) |

**Existing files to modify:**

| File | Change |
|------|--------|
| `Makefile` | Add `-lSDL2_ttf` to LIBS |
| `src/Game.h` | Add `unique_ptr<HUD>` + shader members |
| `src/Game.cpp` | Create HUD in constructor, call render() in render loop |
| `src/Player.cpp` | Add missing `getMaxHealth()` implementation |

### Key Reference Files

These existing files contain patterns to follow:

- **`src/Tilemap.cpp` (setupDebugMesh + renderDebug)** — Shows exactly how to create a custom VAO/VBO and render non-textured geometry. The bar rendering follows this same pattern.
- **`src/Sprite.cpp` (setupMesh + draw)** — Shows the unit quad vertex layout and the model/view/projection pipeline. The HUD uses the same quad but skips the view matrix.
- **`src/Texture.cpp` (constructor)** — Shows the `SDL_Surface → glTexImage2D` upload pipeline. Text rendering uses the exact same flow with SDL_ttf as the pixel source.

### Implementation Order

The implementation naturally splits into two phases — bars can be done without any font dependency:

**Phase 1: Bars (no new dependencies)**
1. Fix `getMaxHealth()` in Player.cpp — tiny but blocks the health bar
2. Create bar shaders (`hud_bar.vert/frag`) — simplest new files
3. Create HUD class with bar rendering only — get colored rectangles on screen
4. Wire into Game — add ownership, call render(), test bars appear

**Phase 2: Text (requires SDL_ttf)**
5. Install SDL_ttf + update Makefile
6. Download font to `assets/fonts/default.ttf`
7. Create text shaders (`hud_text.vert/frag`)
8. Add text rendering to HUD — SDL_ttf integration
9. Test F3 toggle — location name shows/hides with debug mode

### Testing Checklist

1. `make clean && make run` — build with all new files
2. Bars appear in bottom-right immediately (health red, stamina yellow, both full)
3. Press F3 — location name ("Farm" / "Town") appears top-center
4. Walk to warp zone — location name updates to new location
5. Take damage (from enemy) — health bar fill shrinks from top
6. Resize window — bars stay anchored to bottom-right corner
