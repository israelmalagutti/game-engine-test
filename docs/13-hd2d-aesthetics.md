# HD-2D Aesthetics

This document explores the visual style of "HD-2D" games like Octopath Traveler and outlines a roadmap for achieving similar effects in this engine.

## Table of Contents
- [What is HD-2D?](#what-is-hd-2d)
- [Core Visual Elements](#core-visual-elements)
- [Depth and Parallax](#depth-and-parallax)
- [Dynamic Lighting](#dynamic-lighting)
- [Post-Processing Effects](#post-processing-effects)
- [Roadmap: Current Engine to HD-2D](#roadmap-current-engine-to-hd-2d)

---

## What is HD-2D?

**HD-2D** is a visual style pioneered by Square Enix in **Octopath Traveler** (2018). It combines:

- **2D pixel art sprites and tiles** (retro aesthetic)
- **3D lighting and effects** (modern techniques)
- **Depth-of-field blur** (cinematic feel)
- **Bloom and glow** (ethereal atmosphere)
- **Parallax layers** (environmental depth)

The result feels like "what retro games looked like in our imagination" - nostalgic yet visually rich.

### Visual Comparison

```
Traditional 2D:                    HD-2D:
┌────────────────────┐            ┌────────────────────┐
│                    │            │ ░░░ (blurred BG)   │
│    [Tree] [House]  │            │   🌟 ← glow        │
│       [Player]     │            │    [Tree] [House]  │
│    ▓▓▓▓▓▓▓▓▓▓▓▓   │            │       [Player]     │
│    (flat ground)   │            │   💡 point light   │
└────────────────────┘            │    ▓▓▓▓▓▓▓▓▓▓▓▓   │
                                  │ ░░░ (blurred FG)   │
Flat, uniform                     └────────────────────┘
                                  Depth, lighting, atmosphere
```

### Games Using HD-2D Style

| Game | Key Visual Features |
|------|---------------------|
| Octopath Traveler 1 & 2 | Depth-of-field, volumetric lighting, bloom |
| Triangle Strategy | Tilt-shift camera, dramatic lighting |
| Live A Live (Remake) | Varied lighting per era, stylized effects |
| Dragon Quest III (Remake) | Classic aesthetic with modern effects |

---

## Core Visual Elements

HD-2D achieves its look through several layered techniques.

### 1. Pixel Art with High Resolution Rendering

Sprites and tiles are pixel art, but rendered at high resolution:

```
Pixel art sprite:        Rendered at 4x:
┌────────┐               ┌────────────────────────────────┐
│▓▓  ▓▓  │               │▓▓▓▓▓▓▓▓    ▓▓▓▓▓▓▓▓          │
│  ▓▓    │    →          │▓▓▓▓▓▓▓▓    ▓▓▓▓▓▓▓▓          │
│▓▓▓▓▓▓▓▓│               │        ▓▓▓▓▓▓▓▓              │
│▓▓    ▓▓│               │        ▓▓▓▓▓▓▓▓              │
└────────┘               │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
16x16 pixels             └────────────────────────────────┘
                         Still pixel-perfect, but larger
```

Key settings:
- Use `GL_NEAREST` filtering (no interpolation)
- Scale sprites by integer multiples (2x, 3x, 4x)
- High-resolution effects applied on top

### 2. Depth Layering

The world is composed of multiple depth layers:

```
Layer Stack (front to back):

Far Background ─────────── z = -100 (mountains, sky)
         │
Background Objects ─────── z = -50  (distant trees)
         │
Playable Layer ─────────── z = 0    (player, NPCs, ground)
         │
Foreground Objects ──────── z = 50   (close objects)
         │
Near Foreground ─────────── z = 100  (grass, particles)
```

### 3. Selective Focus (Depth of Field)

Objects at different depths have different blur amounts:

```
Depth of Field Effect:

Far (blurred):     ░░░░░░░░░░░░
Mid (sharp):       ████████████  ← Player's layer
Near (blurred):    ░░░░░░░░░░░░

Creates "camera lens" effect, draws eye to sharp area
```

### 4. Dynamic Point Lights

Light sources affect nearby sprites:

```
Without lighting:          With point light:
┌────────────────────┐    ┌────────────────────┐
│    [Torch]         │    │    [Torch]💡       │
│    [Player]        │    │    [Player] ←lit   │
│    [Ground]        │    │    [Ground] ←lit   │
│                    │    │           shadows  │
└────────────────────┘    └────────────────────┘
```

---

## Depth and Parallax

**Parallax scrolling** creates depth by moving layers at different speeds.

### The Concept

Layers further from the camera move slower than close layers:

```
Camera moves right →

Far layer (moves slow):     [Mountain]            [Mountain]
                                    ↓ shift 10px

Mid layer (moves normal):         [Trees]    →      [Trees]
                                    ↓ shift 50px

Near layer (moves fast):              [Grass] →         [Grass]
                                    ↓ shift 100px
```

### Parallax Factor

Each layer has a **parallax factor** that determines scroll speed:

```cpp
// Conceptual parallax calculation
float parallaxFactor;  // 0.0 = static, 1.0 = moves with camera, >1.0 = moves faster

// Far background (moves slow)
parallaxFactor = 0.2f;  // Moves at 20% of camera speed

// Mid-ground (moves with camera)
parallaxFactor = 1.0f;  // Moves at 100% of camera speed

// Foreground (moves fast)
parallaxFactor = 1.5f;  // Moves at 150% of camera speed
```

### Implementation Concept

```cpp
// Render layer with parallax offset
void renderParallaxLayer(Shader& shader, const Camera& camera,
                         Texture* layerTexture, float parallaxFactor) {
    Vector2 camPos = camera.getPosition();

    // Apply parallax factor to camera position
    float offsetX = camPos.x * parallaxFactor;
    float offsetY = camPos.y * parallaxFactor;

    // Build view matrix with parallax offset
    glm::mat4 view = glm::translate(glm::mat4(1.0f),
                                     glm::vec3(-offsetX, -offsetY, 0.0f));

    shader.setMat4("view", view);
    // ... render layer ...
}
```

### Web Development Analogy

Parallax scrolling is common in web design:

```css
/* CSS Parallax (same concept, different implementation) */
.parallax-container {
    perspective: 1px;
    overflow-x: hidden;
    overflow-y: auto;
}

.parallax-layer-back {
    transform: translateZ(-2px) scale(3);  /* Moves slower */
}

.parallax-layer-front {
    transform: translateZ(0);              /* Normal scroll */
}
```

---

## Dynamic Lighting

HD-2D uses real-time lighting to create atmosphere and depth.

### Point Lights

Point lights illuminate nearby objects in a radius:

```
Point Light Diagram:

         ┌───────────────────────┐
         │         ●             │
         │       /│\  light      │
         │      / │ \            │
         │     /  │  \  falloff  │
         │    /   │   \          │
         │   ▓▓▓▓▓▓▓▓▓▓▓         │
         │       lit area        │
         └───────────────────────┘

Brightness = 1.0 / (distance² × attenuation)
```

### Light Data Structure

```cpp
// Conceptual point light
struct PointLight {
    glm::vec2 position;    // World position
    glm::vec3 color;       // RGB color (e.g., warm orange for torch)
    float intensity;       // Brightness multiplier
    float radius;          // Maximum effect distance
};
```

### Fragment Shader Lighting

```glsl
// Conceptual lighting fragment shader
#version 330 core

in vec2 TexCoord;
in vec2 WorldPos;  // Passed from vertex shader

out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform vec3 ambientLight;  // Base lighting (so shadows aren't pure black)

// Point lights (could be array for multiple lights)
uniform vec2 lightPos;
uniform vec3 lightColor;
uniform float lightRadius;
uniform float lightIntensity;

void main() {
    vec4 texColor = texture(spriteTexture, TexCoord);

    // Calculate distance to light
    float dist = distance(WorldPos, lightPos);

    // Attenuation (falloff with distance)
    float attenuation = 1.0 - smoothstep(0.0, lightRadius, dist);

    // Calculate light contribution
    vec3 lighting = ambientLight + lightColor * lightIntensity * attenuation;

    // Apply lighting to texture color
    FragColor = vec4(texColor.rgb * lighting, texColor.a);
}
```

### Multiple Lights

Real HD-2D scenes have many lights. Use a uniform array:

```glsl
// Multiple point lights
#define MAX_LIGHTS 32

uniform int numLights;
uniform vec2 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightRadii[MAX_LIGHTS];

void main() {
    vec3 totalLight = ambientLight;

    for (int i = 0; i < numLights; i++) {
        float dist = distance(WorldPos, lightPositions[i]);
        float attenuation = 1.0 - smoothstep(0.0, lightRadii[i], dist);
        totalLight += lightColors[i] * attenuation;
    }

    FragColor = vec4(texColor.rgb * totalLight, texColor.a);
}
```

### Day/Night Cycle

Ambient light color creates time-of-day atmosphere:

```cpp
// Time-of-day ambient lighting
glm::vec3 getAmbientLight(float timeOfDay) {
    // timeOfDay: 0.0 = midnight, 0.5 = noon, 1.0 = midnight

    if (timeOfDay < 0.25f) {
        // Night to dawn
        return glm::mix(
            glm::vec3(0.1f, 0.1f, 0.2f),  // Night (blue-ish)
            glm::vec3(0.8f, 0.5f, 0.3f),  // Dawn (orange)
            timeOfDay / 0.25f
        );
    }
    // ... more time transitions ...
}
```

---

## Post-Processing Effects

Post-processing applies effects to the **entire rendered image** after all objects are drawn.

### How Post-Processing Works

```
Rendering Pipeline with Post-Processing:

1. Render scene to FRAMEBUFFER (off-screen texture)
   ┌────────────────────┐
   │ Scene rendered     │
   │ to texture         │
   └────────────────────┘
            ↓
2. Apply post-process shader to full-screen quad
   ┌────────────────────┐
   │ Bloom, blur, color │
   │ grading applied    │
   └────────────────────┘
            ↓
3. Display final image on screen
```

### Framebuffer Setup (Conceptual)

```cpp
// Create framebuffer for off-screen rendering
GLuint framebuffer, colorTexture;

glGenFramebuffers(1, &framebuffer);
glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

// Attach color texture
glGenTextures(1, &colorTexture);
glBindTexture(GL_TEXTURE_2D, colorTexture);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screenWidth, screenHeight,
             0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                       GL_TEXTURE_2D, colorTexture, 0);
```

### Bloom Effect

Bloom makes bright areas glow:

```
Original:              With Bloom:
┌────────────────┐    ┌────────────────┐
│    [Light]     │    │   ░[Light]░    │
│                │ →  │  ░░░░░░░░░░░   │
│    [Player]    │    │    [Player]    │
└────────────────┘    └────────────────┘
                      Bright areas spread
```

Bloom process:
1. Extract bright pixels (above threshold)
2. Blur the bright pixels
3. Add blurred bright image back to original

```glsl
// Bloom extraction shader (simplified)
void main() {
    vec3 color = texture(sceneTexture, TexCoord).rgb;

    // Calculate brightness
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));

    // Only keep bright pixels
    if (brightness > 0.8) {
        FragColor = vec4(color, 1.0);
    } else {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
```

### Depth of Field

Blur based on distance from focus plane:

```
Focus at player (z = 0):

z = -50:  ░░░░░░░░  (blurred - too far)
z = 0:    ████████  (sharp - in focus)
z = 50:   ░░░░░░░░  (blurred - too close)
```

Requires:
- Depth buffer (stores Z value per pixel)
- Blur shader that reads depth
- Configurable focus distance and range

### Color Grading

Adjust colors for mood/atmosphere:

```glsl
// Simple color grading
void main() {
    vec3 color = texture(sceneTexture, TexCoord).rgb;

    // Increase contrast
    color = (color - 0.5) * 1.2 + 0.5;

    // Warm color shift (Octopath's golden glow)
    color.r *= 1.1;
    color.b *= 0.9;

    // Vignette (darken edges)
    vec2 center = TexCoord - 0.5;
    float vignette = 1.0 - dot(center, center) * 0.5;
    color *= vignette;

    FragColor = vec4(color, 1.0);
}
```

### Web Development Analogy

Post-processing is like CSS filters applied to a whole element:

```css
/* CSS equivalent concepts */
.game-canvas {
    filter: blur(2px);           /* Like depth-of-field */
    filter: brightness(1.2);     /* Like bloom */
    filter: contrast(1.1)        /* Like color grading */
           saturate(1.2);
}
```

---

## Roadmap: Current Engine to HD-2D

Here's a progressive path from the current engine to HD-2D visuals.

### Phase 1: Foundation (Current State)

What we have:
- ✅ Basic sprite rendering
- ✅ Texture loading
- ✅ Camera with world boundaries
- ✅ MVP matrix pipeline

### Phase 2: Efficient Terrain

Build upon these docs:
- Implement basic tilemap (`11-terrain-architecture.md`)
- Add texture atlas support (`10-texture-atlas.md`)
- Optimize with culling (`09-tilemap-fundamentals.md`)

```
Milestone: Render large tilemap efficiently
```

### Phase 3: Multiple Layers

Add depth:
- Ground layer (always behind)
- Object layer (sorted by Y)
- Foreground layer (always in front)

```cpp
// Conceptual layer rendering
void Game::render() {
    // Far background (sky, mountains)
    backgroundLayer.render(shader, camera, 0.2f);  // Slow parallax

    // Ground tilemap
    groundTilemap.render(shader, camera);

    // Objects and entities (sorted by Y for depth)
    renderEntitiesSortedByY();

    // Foreground (grass, particles)
    foregroundLayer.render(shader, camera, 1.5f);  // Fast parallax
}
```

```
Milestone: Parallax scrolling with depth
```

### Phase 4: Basic Lighting

Add point lights:
- Pass world position to fragment shader
- Implement simple point light calculation
- Add ambient light for base illumination

```
Milestone: Torches illuminate nearby area
```

### Phase 5: Framebuffer Setup

Prepare for post-processing:
- Render to off-screen texture
- Create full-screen quad
- Pass-through shader (displays framebuffer)

```
Milestone: Render through framebuffer (no visible change)
```

### Phase 6: Post-Processing

Add effects one at a time:
1. **Vignette** (easiest - just darken edges)
2. **Color grading** (adjust contrast, warmth)
3. **Bloom** (extract bright → blur → combine)
4. **Depth of field** (requires depth buffer)

```
Milestone: Scene has cinematic post-processing
```

### Phase 7: Polish

Final touches:
- Day/night cycle (animated ambient light)
- Weather effects (rain particles, fog)
- Screen transitions (fade, wipe)
- UI overlays (health bars, menus)

```
Milestone: Full HD-2D visual style
```

### Difficulty Progression

```
Phase 1-2: ████████░░░░░░░░░░░░  Beginner (current docs)
Phase 3:   ████████████░░░░░░░░  Intermediate
Phase 4-5: ████████████████░░░░  Advanced
Phase 6-7: ████████████████████  Expert
```

---

## Summary

### HD-2D Core Techniques

| Technique | Purpose | Complexity |
|-----------|---------|------------|
| Pixel art at high res | Retro aesthetic | Low |
| Multiple layers | Depth perception | Medium |
| Parallax scrolling | Environmental depth | Medium |
| Point lights | Dynamic atmosphere | Medium-High |
| Framebuffer rendering | Enable post-processing | Medium |
| Bloom | Glowing highlights | High |
| Depth of field | Cinematic focus | High |
| Color grading | Mood and atmosphere | Medium |

### Key Concepts

1. **Layers create depth** - Render far-to-near with parallax
2. **Lighting creates atmosphere** - Point lights + ambient
3. **Post-processing creates polish** - Apply to whole image
4. **Pixel art stays crisp** - GL_NEAREST, integer scaling

### Recommended Learning Order

```
09-tilemap-fundamentals    → Understand terrain grids
        ↓
10-texture-atlas           → Efficient tile textures
        ↓
11-terrain-architecture    → Tilemap class design
        ↓
12-rendering-optimization  → Performance for many tiles
        ↓
13-hd2d-aesthetics        → Visual effects (this doc)
        ↓
[Implementation]          → Build each phase progressively
```

### Resources for Further Learning

- **LearnOpenGL.com** - Framebuffers, post-processing, lighting
- **Octopath Traveler GDC Talk** - Official breakdown of HD-2D techniques
- **Shader Toy** - Experiment with GLSL effects
- **The Book of Shaders** - Deep dive into fragment shaders
