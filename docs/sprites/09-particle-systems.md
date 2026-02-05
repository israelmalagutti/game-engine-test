# Particle Systems in 2D Games

This document covers everything you need to understand about particle systems: what they are, how they work at every level, how to render them efficiently, and how to create specific visual effects. It connects each concept to your existing engine architecture (Sprite, Entity, the game loop) and draws parallels to web development when helpful.

## Table of Contents
- [What a Particle System Is](#what-a-particle-system-is)
- [Anatomy of a Particle](#anatomy-of-a-particle)
- [The Particle Lifecycle](#the-particle-lifecycle)
  - [Emitter Configuration](#emitter-configuration)
- [Particle Simulation -- The Update Loop](#particle-simulation----the-update-loop)
- [Particle Rendering](#particle-rendering)
  - [Simple Approach: One Quad per Particle](#simple-approach-one-quad-per-particle)
  - [Blending Modes](#blending-modes)
  - [Optimized Approach: Instanced Rendering](#optimized-approach-instanced-rendering)
  - [Batch Rendering](#batch-rendering)
- [Memory Management: Object Pooling](#memory-management-object-pooling)
- [Common Particle Effects -- Recipes](#common-particle-effects----recipes)
  - [Fire](#fire)
  - [Smoke](#smoke)
  - [Rain](#rain)
  - [Snow](#snow)
  - [Dust and Dirt Puffs](#dust-and-dirt-puffs)
  - [Sparks and Impact](#sparks-and-impact)
  - [Magic and Spell Effects](#magic-and-spell-effects)
  - [Leaves](#leaves)
- [Advanced Techniques](#advanced-techniques)
- [Performance Considerations](#performance-considerations)
- [Particle Systems in HD-2D Games](#particle-systems-in-hd-2d-games)
- [Implementation Architecture](#implementation-architecture)
- [Glossary](#glossary)

---

## What a Particle System Is

A particle system is a technique for simulating **fuzzy, chaotic, organic phenomena** that are difficult or impossible to represent with a single sprite or rigid geometry. Instead of drawing one carefully authored image, you draw **hundreds or thousands of tiny, simple elements** -- "particles" -- each following basic rules of motion and appearance. The collective behavior of these simple elements produces the visual impression of complex natural phenomena.

Think about fire. Fire is not an object with a clear boundary. It is a turbulent mass of glowing gas with no fixed shape, constantly changing, flickering, rising. You cannot draw a single sprite of fire and have it look alive. But if you spawn dozens of small, bright particles at the base, have them drift upward while shrinking and changing from yellow to orange to red to transparent, the result reads convincingly as fire to the human eye.

Each particle is a tiny entity with position, velocity, a limited lifetime, and visual properties like color and size. Particles are **born**, they **live** for a brief time while being updated each frame, and then they **die**. The system continuously spawns new particles to replace the dying ones, creating a steady-state visual effect.

```
What a particle system produces:

Not this (a static sprite):          This (many moving particles):

    ┌──────────┐                           *  .
    │  /\  /\  │                         .  * .  *
    │ /  \/  \ │                        *  . *  .
    │/   FIRE  \│                      .  *  . *  .
    │__________│                        * .  *  .
                                       . *  . *
    A single image.                  [emitter source]
    Looks flat and dead.
                                     Dozens of tiny dots,
                                     each drifting, fading.
                                     Looks alive and dynamic.
```

Particle systems are used for an enormous range of visual effects:

- **Fire and flames** -- glowing particles that rise and fade
- **Smoke** -- gray particles that drift and expand
- **Rain** -- fast-falling elongated particles
- **Snow** -- slow-drifting, wandering white particles
- **Dust** -- tiny particles kicked up by movement
- **Sparks** -- bright particles bursting from impacts
- **Magic effects** -- spiraling, glowing particles for spells
- **Explosions** -- a burst of particles in all directions
- **Leaves** -- particles that drift in wind
- **Bubbles** -- rising, wobbling translucent circles
- **Blood** -- directional burst particles on hit (if your game has that)
- **Environmental atmosphere** -- dust motes floating in sunlight, fireflies at night

### The Web Development Parallel

If you have ever used a JavaScript canvas confetti library, a CSS particle animation, or even created a `requestAnimationFrame` loop that spawns and moves dots across the screen, you have already built a particle system. The concept is identical:

```javascript
// A minimal web particle system (conceptual)
// This is the SAME logic as a game particle system,
// just rendered to a <canvas> instead of OpenGL

class Particle {
    constructor(x, y) {
        this.x = x;
        this.y = y;
        this.vx = (Math.random() - 0.5) * 100;  // random velocity
        this.vy = -Math.random() * 200;           // upward burst
        this.life = 1.0;                           // seconds remaining
        this.age = 0;
    }

    update(dt) {
        this.vy += 400 * dt;   // gravity pulls down
        this.x += this.vx * dt;
        this.y += this.vy * dt;
        this.age += dt;
    }

    isDead() {
        return this.age >= this.life;
    }
}

// In your animation loop:
// 1. Spawn new particles
// 2. Update all particles
// 3. Remove dead particles
// 4. Draw surviving particles
```

The only difference between this and a game engine particle system is the rendering backend (OpenGL quads instead of canvas circles) and the performance techniques needed when you scale to thousands of particles.

---

## Anatomy of a Particle

A particle is intentionally **simple**. It is not an Entity in the game architecture sense -- it has no collision detection, no AI, no complex behavior, no name, no interaction with other game systems. It is a tiny visual-only element with just enough data to move and draw.

Here is every property a particle typically needs:

```
Particle struct:
┌───────────────────────────────────────────────────────────┐
│                                                           │
│  Position (x, y)        Where the particle is right now   │
│  Velocity (vx, vy)      How fast and in what direction    │
│  Acceleration (ax, ay)  Forces applied each frame         │
│                          (gravity, wind)                  │
│                                                           │
│  Lifetime               Total seconds this particle lives │
│  Age                    Seconds it has been alive so far   │
│                                                           │
│  Size                   Current visual size (pixels/units) │
│  Start Size             Size at birth                     │
│  End Size               Size at death                     │
│                                                           │
│  Color (r, g, b, a)     Current visual color and opacity  │
│  Start Color            Color at birth                    │
│  End Color              Color at death                    │
│                                                           │
│  Rotation               Current rotation angle (degrees)  │
│  Rotation Speed         How fast it spins (degrees/sec)   │
│                                                           │
│  Texture / Sprite       What image to render              │
│                          (or just a colored quad)         │
│                                                           │
│  Active                 Is this particle alive?           │
│                                                           │
└───────────────────────────────────────────────────────────┘
```

### Position, Velocity, Acceleration

This is the same physics model as your `Enemy::update()`. In your enemy code:

```cpp
// Enemy.cpp (your existing code)
Vector2 direction = targetPosition - position;
Vector2 normalized = direction.normalized();
position = position + normalized * speed * deltaTime;
```

A particle works the same way but even simpler -- it does not need a target. It just has a velocity and optionally an acceleration (like gravity):

```
Each frame:
  velocity += acceleration * deltaTime
  position += velocity * deltaTime
```

That is it. No pathfinding, no target seeking, no collision. Pure ballistic motion.

### Lifetime and Age

Every particle has a finite lifespan. `lifetime` is how many seconds it is allowed to live (set at spawn). `age` is how many seconds it has been alive (incremented each frame by `deltaTime`). When `age >= lifetime`, the particle dies.

The **age-to-lifetime ratio** (`age / lifetime`, always 0.0 to 1.0) is incredibly useful. It tells you "how far through its life is this particle?" and is used to interpolate every visual property:

```
t = age / lifetime

t = 0.0   ──── birth (full size, full opacity, start color)
t = 0.25  ──── 25% through life
t = 0.5   ──── halfway
t = 0.75  ──── 75% through life
t = 1.0   ──── death (faded out, possibly shrunk, end color)
```

### Size Over Lifetime

Many effects require particles that grow or shrink. Fire particles might start small and grow slightly, then shrink before dying. Explosion particles might start large and shrink. Smoke particles grow as they dissipate.

The simplest approach is **linear interpolation** (lerp) between start and end sizes:

```
currentSize = startSize + (endSize - startSize) * t

where t = age / lifetime

Example (fire particle):
  startSize = 8.0 pixels
  endSize   = 2.0 pixels

  At birth (t=0):     size = 8 + (2-8) * 0   = 8
  At midlife (t=0.5): size = 8 + (2-8) * 0.5 = 5
  At death (t=1):     size = 8 + (2-8) * 1   = 2

  The particle shrinks from 8px to 2px over its life.
```

### Color and Opacity Over Lifetime

Same interpolation, but applied to RGBA channels. The most common pattern is fading opacity to zero at death, so particles do not just pop out of existence:

```
currentColor.r = startColor.r + (endColor.r - startColor.r) * t
currentColor.g = startColor.g + (endColor.g - startColor.g) * t
currentColor.b = startColor.b + (endColor.b - startColor.b) * t
currentColor.a = startColor.a + (endColor.a - startColor.a) * t

Example (fire particle):
  startColor = (1.0, 1.0, 0.3, 1.0)   -- bright yellow, fully opaque
  endColor   = (1.0, 0.2, 0.0, 0.0)   -- red-orange, fully transparent

  At birth:     bright yellow, fully visible
  At midlife:   orange, half transparent
  At death:     red, invisible (faded out completely)
```

### Rotation

Particles can spin for visual variety. A `rotationSpeed` in degrees per second causes the particle to rotate over time:

```
currentRotation = initialRotation + rotationSpeed * age
```

This is purely cosmetic. Rotation does not affect the particle's physics or trajectory. It just makes the visual output less uniform and more organic -- especially noticeable with non-circular textures like leaf sprites or spark shapes.

### Why Particles Are Cheap

The critical insight is what particles do NOT have:

```
What an Entity (Player, Enemy) has:          What a Particle has:
  - Collision detection                        (none)
  - AI / state machine                         (none)
  - Input handling                             (none)
  - Interaction with other entities            (none)
  - Complex animation state                    (none)
  - Inventory / stats / health                 (none)
  - Unique name and identity                   (none)
  - Individual render setup (VAO, VBO)         (shared -- all particles
                                                use the same mesh)

  Update cost: HIGH per entity                 Update cost: TRIVIAL
  But you have 5-50 entities                   But you have 100-10000 particles
```

A particle is a struct with a few floats. Updating it is an addition and a multiplication. This is why you can have thousands of them -- each one is nearly free individually, and the aggregate looks spectacular.

---

## The Particle Lifecycle

### Birth (Emission)

Particles are spawned by an **emitter**. The emitter defines WHERE and HOW particles are created. Think of an emitter as a factory function with randomized parameters:

```
Emitter (the fire pit):
┌───────────────────────┐
│  Spawn Position: The  │ ──── "Where does each particle start?"
│   base of the fire    │      (with random offset within a shape)
│                       │
│  Emission Rate:       │ ──── "How many particles per second?"
│   50 particles/sec    │
│                       │
│  Initial Velocity:    │ ──── "How fast and which direction?"
│   Upward, 30-80 px/s  │      (randomized within a range)
│                       │
│  Initial Lifetime:    │ ──── "How long does each particle live?"
│   0.5 - 1.2 seconds   │      (randomized within a range)
│                       │
│  Initial Size:        │ ──── "How big at birth?"
│   6-12 pixels          │      (randomized)
│                       │
│  Start/End Color:     │ ──── "What color journey?"
│   yellow -> red        │
└───────────────────────┘
```

Randomization is key. If every particle had identical properties, the effect would look mechanical and artificial. By giving each property a **range** (min to max) and picking a random value within that range for each new particle, you get natural-looking variation:

```
50 particles spawned per second, each with:
  velocity.y = random(-80, -30)    // upward, varying speed
  lifetime   = random(0.5, 1.2)   // varying duration
  size       = random(6, 12)      // varying size

  No two particles are identical.
  The collective behavior looks organic.
```

### Life (Simulation)

Each frame, every living particle is updated. This is the core simulation step:

```
for each particle:
    velocity += acceleration * deltaTime    // apply forces
    position += velocity * deltaTime        // move
    age += deltaTime                        // age

    // Interpolate visual properties
    t = age / lifetime
    color = lerp(startColor, endColor, t)
    size = lerp(startSize, endSize, t)
    rotation += rotationSpeed * deltaTime
```

This loop runs for every living particle every frame. With 500 particles, that is 500 iterations of simple arithmetic -- trivial for a modern CPU.

### Death

When a particle's `age` meets or exceeds its `lifetime`, it dies. Dead particles are either:

1. **Removed** from an array (causes allocation/deallocation overhead)
2. **Recycled** by marking them as inactive and reusing their memory slot when a new particle needs to spawn (object pooling -- covered in detail later)

### The Continuous Cycle

The emitter keeps spawning, particles keep dying, and the system reaches a **steady state** where roughly the same number of particles exist at any time:

```
Timeline of particle count for a fire effect (rate = 50/sec, avg life = 1 sec):

Particles alive:
60 ─                                          ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌
50 ─                               ╱╌╌╌╌╌╌╌╌╌╌
40 ─                          ╱╌╌╌╌
30 ─                     ╱╌╌╌
20 ─                ╱╌╌╌
10 ─           ╱╌╌╌
 0 ─ ╌╌╌╌╌╌╌╌╌
     ├─────┤ ramp-up       ├──────────────────┤ steady state
     0s    0.5s             1s                  time →

During ramp-up: particles are being born but none have died yet.
At steady state: birth rate equals death rate. ~50 particles alive at any time.
  (50 particles/sec * 1 sec average lifetime = ~50 particles)
```

### Emitter Configuration

An emitter is defined by its configuration. Here is a complete list of parameters:

**Emission rate**: How many particles to spawn per second. A fire might be 50-100/sec. A light dust effect might be 5/sec. An explosion might be 200 in a single burst.

**Emission shape**: The spatial region where particles appear. Options:

```
Point:              Line:              Circle:             Rectangle:
    *               ─────────          ╭──────╮            ┌────────────┐
                    * * * * *         *        *           │ * * *  * * │
  All particles     Spawned along     *        *           │ * *  * *  │
  start at the      a line segment    ╰──────╯            │  * * * *  │
  same point                         Spawned on or        └────────────┘
                                     inside a circle      Spawned in area
```

**Cone / directional spread**: The angle range of initial velocity:

```
Narrow spread (10 degrees):    Wide spread (90 degrees):    Full spread (360 degrees):
         │                           ╱│╲                          * * *
         │                          ╱ │ ╲                       *       *
         │                         ╱  │  ╲                     *         *
         *                        * * * * *                     *       *
     [emitter]                    [emitter]                     [emitter]
                                                              * * *
  Tight stream                Wide fan                      Radial burst
  (fountain)                  (fire)                        (explosion)
```

**Initial velocity range**: min and max speed, combined with direction spread. For fire: speed 30-80, direction mostly upward. For an explosion: speed 100-300, direction 360 degrees.

**Initial lifetime range**: min and max seconds each particle lives. Randomized per particle.

**Initial size range**: min and max pixel size at birth.

**Start / end color**: The color at birth and color at death. Each particle interpolates between them over its lifetime.

**Acceleration / gravity**: A constant force applied every frame. For realistic effects, gravity is (0, +98) in screen coordinates (positive Y is down in your engine's coordinate system) or (0, -98) if positive Y is up. Wind can be represented as horizontal acceleration.

**One-shot vs. continuous**: An explosion is "one-shot" -- emit 100 particles instantly, then stop. Fire is "continuous" -- emit particles every frame forever (until turned off). The emitter needs a flag or mode for this.

---

## Particle Simulation -- The Update Loop

Here is the complete update logic. Compare it mentally to your `Enemy::update()` -- you will see it is the same core idea (velocity-based motion) but stripped of all the complex parts (targeting, AI, collision):

```cpp
// Pseudocode for updating all particles in the system

struct Particle {
    float x, y;           // position
    float vx, vy;         // velocity
    float ax, ay;         // acceleration (gravity, wind)
    float lifetime;       // total seconds to live
    float age;            // seconds alive so far
    float size;           // current rendered size
    float startSize;      // size at birth
    float endSize;        // size at death
    float r, g, b, a;     // current color
    float startR, startG, startB, startA;  // color at birth
    float endR, endG, endB, endA;          // color at death
    float rotation;       // current rotation angle
    float rotationSpeed;  // degrees per second
    bool active;          // is this particle alive?
};

void ParticleSystem::update(float deltaTime) {
    // --- Phase 1: Spawn new particles ---
    // Accumulate fractional particles from emission rate
    spawnAccumulator += emissionRate * deltaTime;

    // Spawn whole particles from the accumulator
    while (spawnAccumulator >= 1.0f) {
        spawnParticle();  // initialize one new particle from emitter config
        spawnAccumulator -= 1.0f;
    }

    // --- Phase 2: Update all living particles ---
    for (int i = 0; i < maxParticles; i++) {
        Particle& p = particles[i];

        // Skip dead particles
        if (!p.active) continue;

        // Apply acceleration to velocity (gravity, wind)
        p.vx += p.ax * deltaTime;
        p.vy += p.ay * deltaTime;

        // Apply velocity to position
        p.x += p.vx * deltaTime;
        p.y += p.vy * deltaTime;

        // Age the particle
        p.age += deltaTime;

        // Check for death
        if (p.age >= p.lifetime) {
            p.active = false;  // mark for reuse by the pool
            continue;
        }

        // Calculate life progress (0.0 = birth, 1.0 = death)
        float t = p.age / p.lifetime;

        // Interpolate size
        p.size = p.startSize + (p.endSize - p.startSize) * t;

        // Interpolate color
        p.r = p.startR + (p.endR - p.startR) * t;
        p.g = p.startG + (p.endG - p.startG) * t;
        p.b = p.startB + (p.endB - p.startB) * t;
        p.a = p.startA + (p.endA - p.startA) * t;

        // Update rotation
        p.rotation += p.rotationSpeed * deltaTime;
    }
}
```

### The Spawn Accumulator

Notice `spawnAccumulator`. Why not just spawn `emissionRate * deltaTime` particles each frame?

Because `emissionRate * deltaTime` is almost always a fractional number. If your emission rate is 50 particles/sec and your frame runs in 16ms (60 FPS), then each frame you should spawn 50 * 0.016 = 0.8 particles. You cannot spawn 0.8 particles.

The accumulator collects these fractions over time. After 1 frame: accumulator = 0.8. After 2 frames: accumulator = 1.6 -- spawn 1, remainder 0.6. After 3 frames: accumulator = 1.4 -- spawn 1, remainder 0.4. Over time, you get exactly 50 particles per second regardless of frame rate.

```
Frame:   1      2      3      4      5      6      7
Add:    0.8    0.8    0.8    0.8    0.8    0.8    0.8
Total:  0.8    1.6    1.4    2.2    2.0    1.8    2.6
Spawn:   0      1      1      2      2      1      2
After:  0.8    0.6    0.4    0.2    0.0    0.8    0.6

Total spawned after 7 frames: 0+1+1+2+2+1+2 = 9
Expected: 50 * 7 * 0.016 = 5.6 --> the accumulator handles fractions correctly
```

This is the same principle as your `elapsedTime -= frameDuration` pattern in animation timing -- subtract rather than reset, to preserve fractional remainders.

### How spawnParticle Works

```cpp
// Pseudocode for spawning a single particle from the emitter configuration

void ParticleSystem::spawnParticle() {
    // Find an inactive particle in the pool
    int index = findInactiveParticle();
    if (index == -1) return;  // pool is full, skip this particle

    Particle& p = particles[index];

    // === Position: based on emitter shape ===
    // For a point emitter:
    p.x = emitterX;
    p.y = emitterY;

    // For a circle emitter (radius R):
    // float angle = randomFloat(0, 2 * PI);
    // float dist = randomFloat(0, R);
    // p.x = emitterX + cos(angle) * dist;
    // p.y = emitterY + sin(angle) * dist;

    // === Velocity: direction + speed with randomization ===
    float speed = randomFloat(minSpeed, maxSpeed);
    float angle = randomFloat(minAngle, maxAngle);
    p.vx = cos(angle) * speed;
    p.vy = sin(angle) * speed;

    // === Acceleration: usually gravity ===
    p.ax = windX;      // horizontal wind force
    p.ay = gravity;    // vertical gravity (positive = downward in screen coords)

    // === Lifetime ===
    p.lifetime = randomFloat(minLifetime, maxLifetime);
    p.age = 0.0f;

    // === Size ===
    p.startSize = randomFloat(minStartSize, maxStartSize);
    p.endSize = randomFloat(minEndSize, maxEndSize);
    p.size = p.startSize;

    // === Color ===
    p.startR = startColor.r;  p.startG = startColor.g;
    p.startB = startColor.b;  p.startA = startColor.a;
    p.endR = endColor.r;      p.endG = endColor.g;
    p.endB = endColor.b;      p.endA = endColor.a;
    p.r = p.startR;  p.g = p.startG;
    p.b = p.startB;  p.a = p.startA;

    // === Rotation ===
    p.rotation = randomFloat(0, 360);
    p.rotationSpeed = randomFloat(minRotSpeed, maxRotSpeed);

    // === Activate ===
    p.active = true;
}
```

---

## Particle Rendering

### Simple Approach: One Quad per Particle

The most straightforward way to render particles is to draw each one as a textured quad, exactly like your existing `Sprite::draw()`. Each particle gets its own draw call with its own position, size, color, and rotation.

```
A particle quad (same geometry as your Sprite):

    (0,1)────────(1,1)
      │ ╲          │
      │   ╲        │       6 vertices (2 triangles)
      │     ╲      │       Same as Sprite::setupMesh()
      │       ╲    │
      │         ╲  │
    (0,0)────────(1,0)

Each particle uses this same quad, scaled/translated/rotated
to its current properties.
```

The rendering loop for the simple approach:

```cpp
// Pseudocode for simple particle rendering
// This is functionally identical to how your entities render,
// but repeated for each living particle.

void ParticleSystem::render(Shader& shader, const Camera& camera) {
    shader.use();

    // Set projection and view matrices once (same for all particles)
    glm::mat4 projection = glm::ortho(
        0.0f, (float)camera.getViewportWidth(),
        (float)camera.getViewportHeight(), 0.0f
    );
    shader.setMat4("projection", projection);
    shader.setMat4("view", camera.getViewMatrix());

    // Bind the particle texture once
    particleTexture->bind(0);
    shader.setInt("particleTexture", 0);

    // Set blending mode
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // standard alpha
    // or: glBlendFunc(GL_SRC_ALPHA, GL_ONE);            // additive

    glBindVertexArray(quadVAO);

    for (int i = 0; i < maxParticles; i++) {
        Particle& p = particles[i];
        if (!p.active) continue;

        // Build model matrix for this particle
        glm::mat4 model = glm::mat4(1.0f);

        // Move to particle position (center the quad on the position)
        model = glm::translate(model, glm::vec3(
            p.x - p.size * 0.5f,
            p.y - p.size * 0.5f,
            0.0f
        ));

        // Scale to particle size
        model = glm::scale(model, glm::vec3(p.size, p.size, 1.0f));

        // Apply rotation around center
        // (translate to center, rotate, translate back)
        // For simplicity, rotation can be applied before translation

        shader.setMat4("model", model);
        shader.setVec4("particleColor", glm::vec4(p.r, p.g, p.b, p.a));

        glDrawArrays(GL_TRIANGLES, 0, 6);  // One draw call per particle
    }

    glBindVertexArray(0);
}
```

This works correctly but has a problem: if you have 500 particles, that is 500 draw calls. Your `12-rendering-optimization.md` document explains why this is costly -- each draw call requires CPU-GPU synchronization, uniform updates, and driver overhead. For 500 particles, it is probably fine. For 5000, it will become a bottleneck.

### Blending Modes

How particles combine with the background and each other drastically changes the visual effect. There are two primary modes.

**Alpha Blending** (standard transparency):

```
OpenGL call: glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

Formula: result = source.rgb * source.a + destination.rgb * (1 - source.a)

This is "normal" transparency. A 50% transparent red particle over a blue
background produces purple. Particles obscure what is behind them partially.

Visual result:
┌──────────────────────────────────────┐
│                                      │
│  Background visible through          │
│  semi-transparent particles          │
│                                      │
│        ░░░░░                         │
│       ░░▒▒▒░░     ← darker center   │
│      ░░▒▓▓▓▒░░       (more opaque)  │
│       ░░▒▒▒░░     ← lighter edge    │
│        ░░░░░         (more trans.)   │
│                                      │
│  Used for: smoke, dust, fog,         │
│  anything that BLOCKS light          │
└──────────────────────────────────────┘
```

**Additive Blending** (light accumulation):

```
OpenGL call: glBlendFunc(GL_SRC_ALPHA, GL_ONE)

Formula: result = source.rgb * source.a + destination.rgb * 1

This ADDS the particle's color to whatever is already there.
Overlapping particles become BRIGHTER, never darker.
White areas "blow out" to pure white.

Visual result:
┌──────────────────────────────────────┐
│                                      │
│  Overlapping particles become        │
│  brighter, creating a glow effect    │
│                                      │
│        ░░░░░                         │
│       ░░▒▒▒░░                        │
│      ░░▒▓█▓▒░░    ← center is       │
│       ░░▒▒▒░░        BRIGHTER        │
│        ░░░░░         than any        │
│                      single particle │
│  Used for: fire, magic, sparks,      │
│  anything that EMITS light           │
└──────────────────────────────────────┘
```

The difference in practice:

```
Alpha blending (smoke):          Additive blending (fire):

Two overlapping particles:       Two overlapping particles:

  Particle A (gray, 50% alpha)    Particle A (orange, 50% alpha)
       +                              +
  Particle B (gray, 50% alpha)    Particle B (orange, 50% alpha)
       =                              =
  Darker gray where they overlap   BRIGHTER orange where they overlap
  (each one blocks light)          (each one adds light)

  Looks like: thick smoke          Looks like: glowing fire
```

You can also switch blending modes within the same frame. Render fire particles with additive blending, then switch to alpha blending for smoke particles above them. This requires sorting your render calls by blend mode.

### Optimized Approach: Instanced Rendering

Your `12-rendering-optimization.md` doc already covers the concept of instanced rendering for tiles. The same technique applies perfectly to particles -- arguably even better, because particles are the canonical use case for instancing.

The idea: instead of calling `glDrawArrays` once per particle (500 draw calls), you call `glDrawArraysInstanced` once, telling the GPU to draw the same quad 500 times, each with different per-instance data (position, size, color).

```
Without instancing:                  With instancing:

CPU:                                 CPU:
  for each particle:                   Upload all particle data to GPU
    set uniforms                       (one buffer upload)
    glDrawArrays()                     glDrawArraysInstanced(500)
  = 500 draw calls                     = 1 draw call

GPU:                                 GPU:
  Receives 500 separate commands       Receives 1 command
  Must synchronize 500 times           Processes 500 instances in parallel
```

Here is how instanced particle rendering works:

**Step 1: Define the base quad (one quad, shared by all particles)**

```cpp
// Same quad as your Sprite::setupMesh(), defined once
float quadVertices[] = {
    // position   // texcoord
    0.0f, 1.0f,   0.0f, 1.0f,    // top-left
    1.0f, 0.0f,   1.0f, 0.0f,    // bottom-right
    0.0f, 0.0f,   0.0f, 0.0f,    // bottom-left
    0.0f, 1.0f,   0.0f, 1.0f,    // top-left
    1.0f, 1.0f,   1.0f, 1.0f,    // top-right
    1.0f, 0.0f,   1.0f, 0.0f,    // bottom-right
};
```

**Step 2: Create a per-instance data buffer**

```cpp
// Per-instance data: what makes each particle different
struct ParticleInstanceData {
    float x, y;         // position (2 floats)
    float size;         // uniform scale (1 float)
    float rotation;     // rotation angle (1 float)
    float r, g, b, a;   // color (4 floats)
};
// Total: 8 floats = 32 bytes per instance

// Create a VBO for instance data
GLuint instanceVBO;
glGenBuffers(1, &instanceVBO);
glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
// Allocate space for MAX_PARTICLES instances
glBufferData(GL_ARRAY_BUFFER,
             MAX_PARTICLES * sizeof(ParticleInstanceData),
             nullptr,             // no data yet, will fill each frame
             GL_DYNAMIC_DRAW);    // updated every frame
```

**Step 3: Set up vertex attributes with divisors**

```cpp
// Bind the VAO that has the quad vertices
glBindVertexArray(particleVAO);

// Attributes 0 and 1 are already set (quad position and texcoord)
// from the base quad VBO.

// Now bind the instance VBO and set up per-instance attributes
glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

// Attribute 2: instance position (vec2)
glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                      sizeof(ParticleInstanceData),
                      (void*)0);
glEnableVertexAttribArray(2);
glVertexAttribDivisor(2, 1);  // advance once per INSTANCE, not per vertex
//                       ^
//                       THIS IS THE KEY LINE.
//                       Without it, the attribute advances per vertex.
//                       With divisor=1, it advances per instance.

// Attribute 3: instance size (float)
glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE,
                      sizeof(ParticleInstanceData),
                      (void*)(2 * sizeof(float)));
glEnableVertexAttribArray(3);
glVertexAttribDivisor(3, 1);

// Attribute 4: instance rotation (float)
glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE,
                      sizeof(ParticleInstanceData),
                      (void*)(3 * sizeof(float)));
glEnableVertexAttribArray(4);
glVertexAttribDivisor(4, 1);

// Attribute 5: instance color (vec4)
glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE,
                      sizeof(ParticleInstanceData),
                      (void*)(4 * sizeof(float)));
glEnableVertexAttribArray(5);
glVertexAttribDivisor(5, 1);

glBindVertexArray(0);
```

The `glVertexAttribDivisor(attrib, 1)` call is the magic. It tells OpenGL: "Do not advance this attribute for every vertex. Instead, advance it once per instance." Without this, all 6 vertices of every quad would get the same instance data. With it, all 6 vertices of quad N get instance data N, and all 6 vertices of quad N+1 get instance data N+1.

**Step 4: Each frame, upload instance data and draw**

```cpp
// Pseudocode for instanced particle rendering

void ParticleSystem::render(Shader& shader, const Camera& camera) {
    // Collect data for all active particles into a contiguous array
    int activeCount = 0;
    for (int i = 0; i < maxParticles; i++) {
        if (!particles[i].active) continue;

        instanceData[activeCount].x = particles[i].x;
        instanceData[activeCount].y = particles[i].y;
        instanceData[activeCount].size = particles[i].size;
        instanceData[activeCount].rotation = particles[i].rotation;
        instanceData[activeCount].r = particles[i].r;
        instanceData[activeCount].g = particles[i].g;
        instanceData[activeCount].b = particles[i].b;
        instanceData[activeCount].a = particles[i].a;
        activeCount++;
    }

    if (activeCount == 0) return;  // nothing to draw

    // Upload instance data to GPU
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    activeCount * sizeof(ParticleInstanceData),
                    instanceData);

    // Set shader uniforms (once for all particles)
    shader.use();
    shader.setMat4("projection", projection);
    shader.setMat4("view", camera.getViewMatrix());
    particleTexture->bind(0);

    // Set blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive for fire

    // ONE draw call for ALL particles
    glBindVertexArray(particleVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, activeCount);
    //                                         ^
    //                              "Draw 6 vertices, activeCount times"
    glBindVertexArray(0);
}
```

**Step 5: The instanced vertex shader**

```glsl
#version 330 core

// Per-vertex attributes (from the base quad VBO)
layout (location = 0) in vec2 aPos;        // quad vertex position (0-1)
layout (location = 1) in vec2 aTexCoord;   // quad texture coordinate

// Per-instance attributes (from the instance VBO, advances per instance)
layout (location = 2) in vec2 aParticlePos;    // world position
layout (location = 3) in float aParticleSize;  // scale factor
layout (location = 4) in float aParticleRot;   // rotation in radians
layout (location = 5) in vec4 aParticleColor;  // RGBA color

out vec2 TexCoord;
out vec4 ParticleColor;

uniform mat4 projection;
uniform mat4 view;

void main() {
    // Center the quad on origin for rotation
    vec2 centered = aPos - vec2(0.5, 0.5);

    // Apply rotation
    float cosR = cos(aParticleRot);
    float sinR = sin(aParticleRot);
    vec2 rotated = vec2(
        centered.x * cosR - centered.y * sinR,
        centered.x * sinR + centered.y * cosR
    );

    // Scale by particle size
    vec2 scaled = rotated * aParticleSize;

    // Translate to particle world position
    vec2 worldPos = scaled + aParticlePos;

    gl_Position = projection * view * vec4(worldPos, 0.0, 1.0);

    TexCoord = aTexCoord;
    ParticleColor = aParticleColor;
}
```

```glsl
#version 330 core

in vec2 TexCoord;
in vec4 ParticleColor;

uniform sampler2D particleTexture;

out vec4 FragColor;

void main() {
    vec4 texel = texture(particleTexture, TexCoord);

    // Multiply texture by particle color (tint + fade)
    FragColor = texel * ParticleColor;

    // Discard fully transparent fragments
    if (FragColor.a < 0.01)
        discard;
}
```

### Batch Rendering

An alternative to instancing is **batch rendering**, which you already know from `12-rendering-optimization.md`. Instead of using `glDrawArraysInstanced`, you build one large VBO containing ALL particle quads with their final vertex positions and colors, then draw it in a single `glDrawArrays` call.

```
Instancing:                         Batching:
  VBO: [one quad, 6 verts]           VBO: [500 quads, 3000 verts]
  Instance VBO: [500 instances]      (positions already baked into vertices)
  Draw: glDrawArraysInstanced(500)   Draw: glDrawArrays(3000)

Both are 1 draw call.
```

The difference:

```
Instancing:
  + Less memory (quad defined once)
  + GPU handles the duplication (parallel)
  + Instance data is compact (8 floats per particle)
  - Requires OpenGL 3.3+ (you have this)
  - Slightly more complex setup

Batching:
  + Simpler to understand
  + Works on older OpenGL versions
  + Easier to add per-vertex variation (not just per-quad)
  - More memory (6 vertices * 8 floats = 48 floats per particle vs 8)
  - CPU must build all vertices each frame
  - More data uploaded to GPU per frame
```

For particle systems, **instancing is generally preferred** because:
1. You already have OpenGL 3.3 Core
2. Particles all use the same quad geometry
3. Instance data is much smaller than full vertex data
4. The GPU is better at duplicating geometry than the CPU

However, batching is simpler to implement as a first pass if you want to get something working quickly.

---

## Memory Management: Object Pooling

### The Problem

A fire effect spawns 50 particles per second. Each particle lives about 1 second. Over a 10-minute play session, that is 50 * 60 * 10 = 30,000 particles created and destroyed. If each particle is dynamically allocated with `new` and deallocated with `delete`, you have 30,000 allocations and 30,000 deallocations for just one fire effect.

Dynamic memory allocation is expensive:
- The allocator must search for a free block of the right size
- It may need to request memory from the operating system
- It causes memory fragmentation (scattered small holes in memory)
- It can trigger garbage collection pauses in some languages (not C++, but the principle of overhead still applies)

### The Solution: Object Pooling

Pre-allocate a fixed-size array of particles at startup. Never allocate or deallocate again. When a particle "dies," mark it as inactive. When you need to spawn a new particle, find an inactive slot and reinitialize it.

```
Object pool lifecycle:

Startup: allocate MAX_PARTICLES particles in a contiguous array
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│ dead │ dead │ dead │ dead │ dead │ dead │ dead │ dead │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
  All slots available. No particles alive yet.

After spawning 3 particles:
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│ LIVE │ LIVE │ LIVE │ dead │ dead │ dead │ dead │ dead │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
  Slots 0, 1, 2 are active.

Particle 1 dies:
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│ LIVE │ dead │ LIVE │ dead │ dead │ dead │ dead │ dead │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
  Slot 1 is now available for reuse.

Spawn a new particle -- reuses slot 1:
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│ LIVE │ LIVE │ LIVE │ dead │ dead │ dead │ dead │ dead │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
  Slot 1 is alive again with new data. No allocation happened.

The array never grows or shrinks. Memory is stable.
```

### The Web Development Parallel

This is exactly like **connection pooling** in web servers or **database connection pools**:

```javascript
// Web server connection pool (conceptual)
// Same principle as particle pooling

const pool = new Array(MAX_CONNECTIONS);  // pre-allocate
const available = [0, 1, 2, 3, 4];       // indices of free connections

function getConnection() {
    const index = available.pop();        // grab a free slot
    pool[index].active = true;
    return pool[index];
}

function releaseConnection(conn) {
    conn.active = false;
    available.push(conn.index);           // return slot to pool
}

// No new/delete ever happens after initialization.
// Same principle, different domain.
```

### Complete Pool Implementation

```cpp
// Pseudocode for a complete particle pool

class ParticlePool {
private:
    static const int MAX_PARTICLES = 1000;

    // The pool: a flat array of particles, allocated once
    Particle particles[MAX_PARTICLES];

    // Track the index of the last particle we checked when looking for
    // a free slot. This avoids scanning from 0 every time.
    int searchStart = 0;

public:
    ParticlePool() {
        // Mark all particles as dead at startup
        for (int i = 0; i < MAX_PARTICLES; i++) {
            particles[i].active = false;
        }
    }

    // Find an inactive particle slot
    // Returns index, or -1 if pool is full
    int findInactive() {
        // Search forward from where we last left off
        for (int i = 0; i < MAX_PARTICLES; i++) {
            int index = (searchStart + i) % MAX_PARTICLES;
            if (!particles[index].active) {
                searchStart = (index + 1) % MAX_PARTICLES;
                return index;
            }
        }
        return -1;  // pool is completely full
    }

    // Access a particle by index
    Particle& get(int index) {
        return particles[index];
    }

    // Iterate over all particles (for update and render)
    Particle* begin() { return particles; }
    Particle* end() { return particles + MAX_PARTICLES; }

    int capacity() const { return MAX_PARTICLES; }
};
```

The circular search (`searchStart`) is an optimization. Without it, `findInactive()` would always start scanning from index 0, and if the first N particles are always alive, it wastes time checking them every single spawn. By remembering where we left off, the next search starts near where we last found a free slot, which is statistically likely to be near other free slots.

### The Free List Pattern

An even faster approach uses an intrusive linked list of free slots. Dead particles form a chain where each dead particle stores the index of the next dead particle. Finding a free particle is O(1) -- just pop the head of the list.

```
Free list (indices stored inside the dead particles themselves):

particles:  [LIVE] [dead→4] [LIVE] [LIVE] [dead→7] [LIVE] [LIVE] [dead→-1]
indices:      0       1       2      3       4        5      6       7

freeHead = 1
  particles[1].nextFree = 4
  particles[4].nextFree = 7
  particles[7].nextFree = -1  (end of list)

To spawn: take index 1, set freeHead = 4. O(1).
To kill particle 3: set particles[3].nextFree = freeHead (4), set freeHead = 3. O(1).
```

This avoids scanning entirely. However, the simple scanning approach is perfectly adequate for particle counts under 10,000 in practice. The scan is over a contiguous array in cache-friendly memory, which modern CPUs handle extremely fast.

---

## Common Particle Effects -- Recipes

Each recipe below specifies the emitter configuration and particle behavior needed to produce a specific visual effect. You can mix and modify these to create your own effects.

### Fire

Fire is the quintessential particle effect. Particles are born at the base, rise upward, shrink, and change color from bright yellow through orange to red to transparent.

```
Emitter Configuration:
  Shape:          Circle (radius 6-8 pixels, at base of fire)
  Rate:           60-80 particles/sec
  Mode:           Continuous

Particle Properties:
  Velocity:       Mostly upward: angle = random(-100, -80) degrees
                   (nearly straight up, slight spread)
                  Speed: random(40, 90) px/sec
  Acceleration:   (0, -20)  -- slight upward push (negative = up)
                   ("hot air rises" -- countering gravity)
  Lifetime:       random(0.4, 0.9) seconds
  Start Size:     random(8, 14) pixels
  End Size:       random(1, 3) pixels
  Start Color:    (1.0, 1.0, 0.3, 0.9)  -- bright yellow
  End Color:      (1.0, 0.1, 0.0, 0.0)  -- dark red, fully transparent
  Rotation Speed: random(-90, 90) degrees/sec

Blending:         ADDITIVE (GL_SRC_ALPHA, GL_ONE)

Texture:          Soft circle (white, fading to transparent at edges)
                  Additive blending + this texture = natural glow
```

```
ASCII visualization of fire:

              .
             * .
            . * .
           *  .  *
          .  * . *
         * .  *  .
        . *  . * .
       *  . *  .  *
      . *  .  * .
     ════════════════
      [emitter base]

  Particles are largest and brightest at the bottom.
  They shrink and fade as they rise.
  Additive blending makes overlapping areas glow brighter.
```

### Smoke

Smoke typically appears above fire. Particles rise slowly, EXPAND (opposite of fire), and fade from gray to transparent.

```
Emitter Configuration:
  Shape:          Circle (radius 4 pixels, positioned above fire)
  Rate:           15-25 particles/sec (much less than fire)
  Mode:           Continuous

Particle Properties:
  Velocity:       Upward with drift: angle = random(-110, -70) degrees
                  Speed: random(15, 35) px/sec (slower than fire)
  Acceleration:   (random(-5, 5), -8)  -- slight upward + random horizontal drift
  Lifetime:       random(1.5, 3.0) seconds (longer than fire)
  Start Size:     random(6, 10) pixels
  End Size:       random(20, 35) pixels (GROWS -- smoke expands)
  Start Color:    (0.4, 0.4, 0.4, 0.5)  -- medium gray, semi-transparent
  End Color:      (0.6, 0.6, 0.6, 0.0)  -- light gray, fully transparent
  Rotation Speed: random(-30, 30) degrees/sec

Blending:         ALPHA (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
                  (Smoke blocks light, does not emit it)

Texture:          Soft, fuzzy circle (same as fire, or a cloudier texture)
```

```
ASCII visualization of smoke:

            ░░░
          ░░░░░░░
         ░░░░░░░░░        ← large, faded, almost invisible
          ░░░░░░░
           ░░░░░
            ▒▒▒
           ▒▒▒▒▒          ← medium size, partly transparent
            ▒▒▒
             ▓▓
            ▓▓▓▓           ← small, denser (just born)
             ▓▓
         ════════════
          [fire below]

  Smoke particles are small and opaque near the fire,
  then expand and fade as they rise.
  Alpha blending (not additive) because smoke blocks light.
```

### Rain

Rain particles span the full width of the screen (or the visible area), falling fast with slight horizontal drift for wind.

```
Emitter Configuration:
  Shape:          Rectangle (screen width + margin, positioned above
                   the visible area so rain enters from the top)
                  Width: camera viewport width + 100 pixels
                  Height: 1 pixel (thin line)
                  Position: above the camera's top edge
  Rate:           200-500 particles/sec (heavy rain = more)
  Mode:           Continuous

Particle Properties:
  Velocity:       Mostly downward with wind:
                  vx = random(20, 40)  -- slight wind to the right
                  vy = random(400, 600) -- fast downward
  Acceleration:   (0, 100)  -- gravity (accelerate further)
  Lifetime:       random(0.8, 1.5) seconds (enough to cross the screen)
  Start Size:     Width: 1-2 pixels, Height: 8-16 pixels
                  (rain is ELONGATED, not square)
  End Size:       Same as start (rain does not shrink)
  Start Color:    (0.6, 0.7, 0.9, 0.6)  -- blue-white, semi-transparent
  End Color:      (0.6, 0.7, 0.9, 0.3)  -- slightly more transparent
  Rotation:       Angled to match velocity direction
                  atan2(vy, vx) gives the angle of travel

Blending:         ALPHA (rain streaks are semi-transparent lines)

Texture:          A 1x4 white rectangle (or just a colored quad, no texture)
```

```
ASCII visualization of rain:

  ╱  ╱  ╱  ╱  ╱  ╱  ╱  ╱  ╱  ╱
    ╱  ╱  ╱  ╱  ╱  ╱  ╱  ╱  ╱
  ╱  ╱  ╱  ╱  ╱  ╱  ╱  ╱  ╱  ╱
    ╱  ╱  ╱  ╱  ╱  ╱  ╱  ╱  ╱
  ╱  ╱  ╱  ╱  ╱  ╱  ╱  ╱  ╱  ╱
  ═══════════════════════════════
           [ground]

  Each "/" is one rain particle -- an elongated, angled quad.
  They all fall at similar angles (wind direction).
  Dense coverage from high emission rate.

  Optional: spawn "splash" particles at the ground line.
  When a rain particle's Y passes the ground height,
  spawn 2-4 tiny splash particles at that position
  with upward + outward velocity and very short lifetime (0.1s).

  Splash sub-emitter (one-shot per impact):
        .  .  .
       *  │  *
       ───●───
      [impact point]
```

### Snow

Snow is slower than rain, with a gentle horizontal oscillation that makes flakes wander as they fall.

```
Emitter Configuration:
  Shape:          Rectangle (same as rain -- full screen width above camera)
  Rate:           30-80 particles/sec (light snow = 30, blizzard = 200)
  Mode:           Continuous

Particle Properties:
  Velocity:       Slow downward with gentle drift:
                  vx = random(-15, 15)  -- slight random horizontal
                  vy = random(20, 50)   -- slow fall
  Acceleration:   (0, 5)  -- very light gravity
  Lifetime:       random(4.0, 8.0) seconds (long -- snow falls slowly)
  Start Size:     random(2, 5) pixels (small flakes)
  End Size:       Same as start
  Start Color:    (1.0, 1.0, 1.0, 0.8)  -- white, mostly opaque
  End Color:      (1.0, 1.0, 1.0, 0.0)  -- white, faded (melting)
  Rotation Speed: random(-60, 60) degrees/sec

  SPECIAL: Horizontal sine-wave drift
  Each frame, add a horizontal offset:
    p.x += sin(p.age * driftFrequency + p.phase) * driftAmplitude * deltaTime

  driftFrequency: random(2.0, 4.0) per particle (how fast it sways)
  driftAmplitude: random(10, 30) pixels (how far it sways)
  phase: random(0, 2*PI) per particle (so they do not all sway in sync)

Blending:         ALPHA (snow is opaque-ish, blocks background slightly)

Texture:          Small soft circle (white, feathered edges)
                  Or an actual snowflake sprite for larger flakes
```

```
ASCII visualization of snow:

       *         .    *
    .       *       .       *
         .      *        .
    *        .       *
       .          .       .
    *      *          .      *
              .    *
       .            .     *
    ═══════════════════════════
             [ground]

  Each * or . is a snowflake at a slightly different phase
  of its horizontal sine-wave drift.
  They wander left and right as they fall.
  Long lifetimes mean many are visible at once.
```

The sine-wave drift is what makes snow look like snow instead of slow rain. Without it, particles just fall straight and look unnatural. The key is giving each particle its own random `phase` so they do not all oscillate in unison:

```
Particle A (phase = 0):     Particle B (phase = PI):
  ╲                             ╱
   ╲                           ╱
    ╱                         ╲
   ╱                           ╲
  ╲                             ╱
   ╲                           ╱

They sway in opposite directions -- looks organic and varied.
```

### Dust and Dirt Puffs

Small bursts of particles when the player lands from a jump, starts running, or slides to a stop. These are **one-shot** effects, not continuous.

```
Emitter Configuration:
  Shape:          Point (at the character's feet)
  Rate:           N/A (one-shot burst)
  Burst Count:    8-15 particles
  Mode:           One-shot (emit all at once, then stop)
  Trigger:        Player lands, starts running, stops suddenly

Particle Properties:
  Velocity:       Spread outward and slightly upward:
                  angle = random(-150, -30) degrees (upward fan)
                  speed = random(30, 80) px/sec
  Acceleration:   (0, 120)  -- gravity pulls them down quickly
  DRAG:           0.95 per frame (multiply velocity by 0.95 each frame)
                  This makes particles slow down quickly (air resistance)
  Lifetime:       random(0.2, 0.5) seconds (very short)
  Start Size:     random(3, 6) pixels
  End Size:       random(1, 2) pixels (shrink)
  Start Color:    (0.7, 0.6, 0.4, 0.7)  -- brown-tan, semi-transparent
  End Color:      (0.7, 0.6, 0.4, 0.0)  -- same color, fully transparent
  Rotation Speed: random(-180, 180) degrees/sec

Blending:         ALPHA

Texture:          Very small, soft circle
```

```
ASCII visualization of dust puff:

  Frame 1 (just spawned):       Frame 5 (0.1s later):        Frame 15 (0.3s):
       . . .                         .       .                     .
      . * * .                     .     .   .
       . . .                                                   (almost gone)
     ───●───                      ───●───                      ───●───
      [feet]                       [feet]                       [feet]

  Particles burst outward, slow down quickly due to drag,
  then fade and vanish in under half a second.
```

**Drag** is a new concept here. It is a simple way to simulate air resistance:

```
// Drag: multiply velocity by a factor < 1.0 each frame
p.vx *= drag;  // drag = 0.95 means lose 5% speed each frame
p.vy *= drag;

// At 60 FPS with drag 0.95:
// After 1 frame: speed *= 0.95
// After 10 frames: speed *= 0.95^10 = 0.60 (40% speed lost)
// After 30 frames: speed *= 0.95^30 = 0.21 (79% speed lost)
// After 60 frames: speed *= 0.95^60 = 0.05 (nearly stopped)
```

Note: drag should technically be frame-rate-independent (`drag = pow(baseDrag, deltaTime)`), but for a learning project, the simple multiplication approach is fine.

### Sparks and Impact

Bright particles that burst from a weapon strike, a pickaxe hitting rock, or a collision with metal. Similar to dust puffs but brighter, faster, and with additive blending for glow.

```
Emitter Configuration:
  Shape:          Point (at impact location)
  Rate:           N/A (one-shot burst)
  Burst Count:    15-30 particles
  Mode:           One-shot
  Trigger:        Weapon impact, tool use, collision

Particle Properties:
  Velocity:       Radial burst in all directions:
                  angle = random(0, 360) degrees
                  speed = random(80, 250) px/sec (fast!)
  Acceleration:   (0, 200)  -- gravity creates arcing trajectories
  Lifetime:       random(0.15, 0.4) seconds (very brief)
  Start Size:     random(2, 4) pixels (tiny)
  End Size:       random(1, 1) pixel
  Start Color:    (1.0, 0.9, 0.5, 1.0)  -- bright yellow-white
  End Color:      (1.0, 0.3, 0.0, 0.0)  -- red-orange, faded
  Rotation Speed: 0 (sparks are too small for rotation to matter)

Blending:         ADDITIVE (GL_SRC_ALPHA, GL_ONE)
                  (sparks emit light -- overlapping sparks should glow)

Texture:          Tiny bright dot (1x1 to 3x3 white circle)
```

```
ASCII visualization of sparks:

  Frame 1 (impact):          Frame 3 (0.05s):           Frame 8 (0.15s):

                                   *                        .
                                *     .                  .
       * * *               *           *              .        .
      * ═══ *                  ═══                       ═══
       * * *               *           *              .        .
                                *     .                  .
                                   *                        .

  [impact at ═══]       Sparks fly outward           Gravity bends
                        at high speed                trajectories downward
                                                     Fading quickly
```

### Magic and Spell Effects

Magic effects typically use spiraling or orbiting particles with vivid colors and additive blending for a supernatural glow.

```
Emitter Configuration:
  Shape:          Circle (radius = spell radius, centered on caster/target)
  Rate:           40-80 particles/sec
  Mode:           Continuous while spell is active

Particle Properties:
  SPECIAL: Velocity tangent to circle (particles orbit the center)

  For a clockwise orbit:
    angle = random(0, 2*PI)  -- spawn position on circle
    spawnX = centerX + cos(angle) * radius
    spawnY = centerY + sin(angle) * radius

    tangentAngle = angle + PI/2  -- perpendicular to radius = tangent
    speed = random(40, 80)
    vx = cos(tangentAngle) * speed
    vy = sin(tangentAngle) * speed

  Acceleration:   Inward pull toward center:
                  ax = (centerX - p.x) * attractionStrength
                  ay = (centerY - p.y) * attractionStrength
                  (This keeps particles orbiting rather than flying away)
  Lifetime:       random(0.6, 1.5) seconds
  Start Size:     random(3, 8) pixels
  End Size:       random(1, 2) pixels
  Start Color:    (0.5, 0.2, 1.0, 0.9)  -- purple (or blue, gold, etc.)
  End Color:      (0.8, 0.4, 1.0, 0.0)  -- lighter purple, faded
  Rotation Speed: random(-120, 120) degrees/sec

  OPTIONAL: Sine-wave oscillation on the radius
    Each frame: radius += sin(p.age * 6.0) * 3.0
    This makes particles pulse in and out, creating a "breathing" effect

Blending:         ADDITIVE (magic glows)

Texture:          Soft glowing circle, or a small star shape
```

```
ASCII visualization of magic spiral:

        .  *  .
      *    .    *
    .   ╭─────╮   .
    *   │     │   *
    .   │  @  │   .       @ = caster / center
    *   │     │   *
    .   ╰─────╯   .
      *    .    *
        .  *  .

  Particles orbit the center in a ring.
  Additive blending makes the ring glow brighter
  where particles cluster together.
  Sine-wave oscillation makes the ring "breathe."
```

### Leaves

Leaves drifting from trees or blown by wind. These use actual leaf sprites and have long lifetimes with gentle, wandering motion.

```
Emitter Configuration:
  Shape:          Rectangle (positioned in tree canopy area, or off-screen
                   for wind-blown leaves)
  Rate:           3-10 particles/sec (sparse -- leaves are noticeable individually)
  Mode:           Continuous

Particle Properties:
  Velocity:       Wind direction with variation:
                  vx = random(10, 30) + windSpeed  -- horizontal drift
                  vy = random(5, 15)                -- slow downward
  Acceleration:   (0, 5)  -- very gentle gravity
  Lifetime:       random(5.0, 12.0) seconds (very long)
  Start Size:     random(4, 8) pixels (recognizable leaf shapes)
  End Size:       Same (leaves do not shrink)
  Start Color:    (0.3, 0.7, 0.2, 0.9)  -- green
                  or (0.8, 0.5, 0.1, 0.9)  -- autumn orange
                  or randomize between seasonal colors
  End Color:      Same color but alpha = 0.0 (fade out)
  Rotation Speed: random(-45, 45) degrees/sec (slow lazy spin)

  SPECIAL: Sine-wave oscillation (same as snow, but horizontal dominant)
    p.y += sin(p.age * driftFreq + p.phase) * amplitude * deltaTime

Blending:         ALPHA (leaves are opaque objects)

Texture:          An actual leaf sprite (small pixel art leaf)
                  Can have multiple leaf textures for variety
```

```
ASCII visualization of leaves:

     [tree canopy area]
    ╔══════════════════╗
    ║  ▓▓▓▓▓▓▓▓▓▓▓▓▓  ║
    ║ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ ║
    ╚══════════════════╝
              │
     ~        │    ~            ~ = leaves drifting
         ~    │         ~
              │    ~
    ~         │              ~
         ~         ~
                        ~
    ~              ~
    ─────────────────────────
           [ground]

  Leaves drift horizontally with wind, oscillating
  gently up and down as they travel.
  Slow rotation makes them tumble naturally.
```

---

## Advanced Techniques

### Particle Trails (Sub-Emitters)

A particle itself can act as an emitter, spawning smaller "child" particles behind it as it moves. This creates trail effects like comet tails, magic missile paths, or firework streamers.

```
Primary particle path:

   * ─ ─ → * ─ ─ → * ─ ─ → * (moving)
   │       │       │
   ▼       ▼       ▼
   .       .       .        ← child particles spawned at each position
    .       .       .          they have their own lifetime and fade
     .       .       .         independently

Result: a glowing trail behind the moving particle
```

Implementation: every N frames (or every M pixels of travel), the primary particle triggers a spawn from a secondary emitter at its current position. The child particles have very short lifetimes and no velocity (or slight outward drift), so they just hang in place and fade.

### Attraction and Repulsion

Instead of constant acceleration, particles can be attracted to or repelled from a point. This creates "force field" effects.

```
// Attraction (pull toward a point):
float dx = attractPoint.x - p.x;
float dy = attractPoint.y - p.y;
float distSq = dx * dx + dy * dy;
float force = attractionStrength / (distSq + 1.0f);  // +1 avoids divide-by-zero
p.vx += dx * force * deltaTime;
p.vy += dy * force * deltaTime;

// Repulsion (push away): same math, negate the force
p.vx -= dx * force * deltaTime;
p.vy -= dy * force * deltaTime;
```

Use case: a magic orb that gathers energy (particles from the surrounding area spiral inward toward the orb), or a shockwave (particles pushed outward from an explosion center).

### Collision with Tilemap

For most effects, particles pass through everything -- they are purely visual. But sometimes you want particles to interact with the world: sparks bouncing off the ground, rain splashing on rooftops, snow settling on ledges.

Simple tilemap collision for particles:

```
// After updating particle position:
int tileX = (int)(p.x / tileSize);
int tileY = (int)(p.y / tileSize);

if (tilemap.isSolid(tileX, tileY)) {
    // Option A: Kill the particle (rain hits ground, dies)
    p.active = false;
    // Optionally spawn splash particles here

    // Option B: Bounce (sparks reflect off surfaces)
    p.vy = -p.vy * 0.6f;  // reverse vertical, lose energy
    p.y = tileY * tileSize - 1;  // push out of tile
}
```

This is NOT full collision detection. It is a single-point check per particle (the particle's position, not its entire quad). It is cheap enough to do for every particle every frame because it is just an array lookup.

### Texture Animation

Each particle can play a mini sprite animation over its lifetime. Instead of a single texture, the particle has a sequence of frames (like a 4-frame smoke puff animation).

```
Smoke puff animation (4 frames in a texture atlas):
┌──────┬──────┬──────┬──────┐
│  .   │  ..  │ .... │......│
│  ..  │ .... │......│......│
│  .   │  ..  │ .... │......│
└──────┴──────┴──────┴──────┘
  Frame 0  Frame 1  Frame 2  Frame 3
  Birth                       Death

The particle interpolates through these frames over its lifetime:
  currentFrame = (int)(t * totalFrames)

  At t=0.0 → frame 0 (small puff)
  At t=0.5 → frame 2 (expanded)
  At t=1.0 → frame 3 (fully dissipated)
```

This is the same UV math from your tilemap and sprite animation systems -- index into a texture atlas based on a frame number. The only difference is the frame advances based on life progress (`t = age / lifetime`) rather than elapsed time.

### Noise-Based Movement

For organic, wind-like particle movement, you can use Perlin noise (or simplex noise) to offset particle velocities. Instead of a constant wind acceleration, the wind direction and strength varies smoothly through space and time.

```
// Instead of constant wind:
p.vx += windX * deltaTime;

// Use noise-based wind:
float noiseValue = perlinNoise(p.x * 0.01, p.y * 0.01, time * 0.5);
p.vx += noiseValue * windStrength * deltaTime;
p.vy += perlinNoise(p.x * 0.01 + 100, p.y * 0.01, time * 0.5) * windStrength * deltaTime;
```

This creates swirling, eddying motion that looks like real air currents. The particles move coherently (nearby particles move similarly because they sample nearby noise values) but not identically.

Perlin noise is a big topic on its own. For particle systems, you just need to know that it provides smooth random values that vary continuously over space and time, unlike `random()` which jumps erratically.

### GPU Particle Systems (Compute Shaders)

In advanced engines, particles are updated entirely on the GPU using compute shaders. The CPU never touches particle data after initial setup. This allows millions of particles because:

1. The GPU has thousands of cores for parallel math
2. Particle data never crosses the CPU-GPU bus (no upload each frame)
3. The GPU reads and writes particle data in the same memory it renders from

This requires OpenGL 4.3+ (compute shaders) or Vulkan. Your engine uses OpenGL 3.3, so this is out of scope. But it is worth knowing the concept exists. If you ever need 100,000+ particles, this is how you get there.

### Sorting for Correct Alpha Blending

Alpha-blended particles need to be drawn back-to-front for correct transparency. If a particle behind another is drawn after it, the blending equation produces incorrect colors.

```
Correct (back to front):     Incorrect (random order):

  Background                   Background
     ↓                            ↓
  Particle A (far)             Particle B (near) -- drawn first
     ↓                            ↓
  Particle B (near)            Particle A (far) -- drawn second, incorrectly
     ↓                            ↓
  Screen                       Screen: Particle A "shows through" B

  A is blended onto background,    B is blended onto background,
  then B is blended on top.         then A is blended on top of B.
  Correct layering.                 A appears IN FRONT of B. Wrong!
```

Solutions:

1. **Use additive blending**: Additive blending is order-independent. The result of A+B equals B+A. This is why fire, sparks, and magic effects almost always use additive blending -- no sorting needed.

2. **Sort particles by depth**: Sort the particle array by Y position (or Z if you use depth layers) each frame before rendering. This costs O(N log N) per frame but guarantees correct order.

3. **Disable depth writing**: `glDepthMask(GL_FALSE)` prevents particles from writing to the depth buffer, so they do not occlude each other at the hardware level. Combined with sorting, this gives correct results.

For a 2D game, most particle effects use additive blending (fire, magic, sparks) or are sparse enough that overlap artifacts are not noticeable (leaves, dust). Sorting is rarely necessary in practice.

---

## Performance Considerations

### How Many Particles Can You Have?

A rough budget for a 2D game on modern hardware:

```
Approach:                 Comfortable Budget:    Maximum (before problems):
Per-particle draw calls   100-500 particles       ~1000 before FPS drops
Instanced rendering       5,000-10,000            ~50,000+ (GPU dependent)
Batch rendering           2,000-5,000             ~20,000
GPU compute shaders       100,000+                Millions (not in your GL version)
```

For your HD-2D Stardew Valley clone, you are extremely unlikely to need more than a few hundred particles at a time. Environmental effects (rain, snow, dust motes) plus a few active effects (fire in a fireplace, magic spell) might total 200-500 particles. Even the simplest approach (one draw call per particle) handles this comfortably at 60 FPS.

### CPU vs. GPU Bottleneck

The particle update loop (position, velocity, age, interpolation) runs on the CPU. This is the bottleneck for large particle counts with simple rendering approaches. The GPU is almost never the bottleneck for 2D particles because:

- 2D quads are trivially simple geometry
- Particle textures are usually tiny (8x8 to 32x32 pixels)
- Fill rate (pixels drawn) is low compared to 3D scenes

```
Typical frame time breakdown for a 2D game with particles:

CPU:
  Game logic:        0.5ms
  Particle update:   0.1ms (500 particles * trivial math)
  Build render data: 0.2ms (prepare instance/batch data)
  Total CPU:         0.8ms

GPU:
  Tilemap rendering: 1.0ms
  Entity rendering:  0.3ms
  Particle rendering: 0.2ms (500 textured quads)
  Total GPU:         1.5ms

Frame total: ~2.3ms = ~430 FPS
Plenty of headroom. You would need 10x more work to hit 60 FPS limit.
```

### When to Optimize

Follow the same principle from your `12-rendering-optimization.md`:

```
Is your game running below 60 FPS?
├── No  → Do not optimize. Keep the simple approach.
└── Yes → Is the particle system the bottleneck?
    ├── No  → Optimize whatever IS the bottleneck.
    └── Yes → How many particles do you have?
        ├── < 500   → Something else is wrong. Profile more carefully.
        ├── 500-2000 → Switch to instanced rendering.
        └── 2000+    → You probably have too many effects active.
                       Reduce emission rates or particle lifetimes.
```

For a learning project, start with the simple approach (one draw call per particle). Only move to instancing if you actually need more particles than the simple approach can handle. The conceptual understanding is more valuable than premature optimization.

---

## Particle Systems in HD-2D Games

### How Octopath Traveler Uses Particles

Octopath Traveler and its sequel use particle systems extensively as a core part of the HD-2D aesthetic:

**Battle effects**: Every spell, every attack has accompanying particles. Fire spells have flame particles, ice spells have frost and snow particles, lightning has spark trails. These effects are layered with additive blending to create the intense visual spectacle the series is known for.

**Environmental atmosphere**: Dust motes floating in shafts of sunlight, snow drifting through mountain passes, rain in coastal towns, firefly-like particles in forests at night. These are subtle, low-density continuous emitters that add life to every scene without demanding attention.

**Transition effects**: Scene transitions and dramatic moments use particle bursts -- energy gathering before a powerful attack, petals scattering during a story beat.

### How Particles Contribute to the "HD" in HD-2D

The defining visual contrast of HD-2D is:

```
Sprites and tiles:    LOW resolution (pixel art, 16x16, 32x32)
Effects and lighting: HIGH resolution (smooth gradients, many particles)

This contrast is what makes HD-2D feel special:

┌───────────────────────────────────────────┐
│                                           │
│  [pixel art tree]    *  .  *              │
│  ████████            .  *  .  *           │
│  ██    ██        *  .  *  .               │
│  ████████           .  *  .               │
│    ████            *  .                   │
│    ████              .                    │
│                                           │
│  Crunchy, nostalgic   Smooth, modern      │
│  pixel art            particle effects    │
│                                           │
│  The COMBINATION creates the HD-2D look.  │
│  Neither alone achieves the effect.       │
└───────────────────────────────────────────┘
```

Particles bridge the gap between retro and modern. The sprites say "SNES nostalgia" while the particles say "running on modern hardware." This duality is the entire point.

### Bloom + Particles

Bloom is a post-processing effect that makes bright areas bleed light into surrounding pixels. When applied to additive-blended particles, the result is stunning:

```
Without bloom:                    With bloom:

   *  *  *                          *  . * .  *
  *  * *  *                       . * . * . * . *
   *  *  *                         * . * * * . *
                                    . * . * . * .
  Particles are bright               *  . * .  *
  dots with hard edges.
                                  Particles have soft glowing
                                  halos that bleed outward.
                                  Fire looks like it actually
                                  emits light into the scene.
```

The workflow:
1. Render the scene to a framebuffer
2. Render particles (additive) to the same framebuffer
3. Extract bright pixels (above a threshold) into a separate texture
4. Blur the bright texture (Gaussian blur)
5. Add the blurred bright texture back to the original scene
6. The result: particles glow

This is covered more in your `13-hd2d-aesthetics.md` document. The key point here is that particles are the primary SOURCE of bloom in an HD-2D game. Without bright particles, bloom has nothing to bloom.

### Depth-of-Field + Particles

In HD-2D, different visual layers exist at different "depths" even though the game is 2D. Particles at different depths can be blurred differently:

```
Layer layout (near to far):

  Near (foreground)  ─── particles are BLURRED (close to camera, out of focus)
  Focus plane        ─── player, main game action (sharp)
  Mid-background     ─── background particles SHARP (in focus range)
  Far background     ─── distant particles BLURRED (far from camera)

Example: a forest scene
  Foreground:  Large, blurred leaves drifting past the camera (particle system)
  Focus:       Player walking on a path
  Background:  Small, blurred dust motes in distant sunbeams (particle system)

The particles at different depths create a sense of volume
that flat 2D sprites alone cannot achieve.
```

Implementing this requires rendering particles to different layers (framebuffers) and applying different blur amounts during compositing. This is an advanced technique that combines particle systems with the post-processing pipeline.

---

## Implementation Architecture

Here is how a particle system integrates with your existing engine architecture.

### The Particle Struct

```cpp
// Particle.h -- a plain data struct, no methods needed
// This is NOT an Entity. It is a lightweight data container.

struct Particle {
    // Physics
    float x, y;           // world position
    float vx, vy;         // velocity (pixels per second)
    float ax, ay;         // acceleration (gravity, wind)

    // Lifetime
    float lifetime;       // total seconds this particle lives
    float age;            // seconds alive so far

    // Visual: size
    float size;           // current rendered size (pixels)
    float startSize;
    float endSize;

    // Visual: color
    float r, g, b, a;     // current RGBA (0.0 to 1.0)
    float startR, startG, startB, startA;
    float endR, endG, endB, endA;

    // Visual: rotation
    float rotation;       // current angle (radians)
    float rotationSpeed;  // radians per second

    // Pool management
    bool active;          // is this particle alive?

    // Optional: per-particle drag
    float drag;           // multiplied against velocity each frame (0.95 = light drag)

    // Optional: sine-wave drift (for snow/leaves)
    float driftFreq;      // oscillation frequency
    float driftAmp;       // oscillation amplitude
    float driftPhase;     // phase offset (randomized per particle)
};
```

### The ParticleEmitter Configuration

```cpp
// ParticleEmitterConfig.h -- describes HOW to spawn particles
// This is a configuration struct, not a live object.

enum class EmitterShape {
    POINT,
    CIRCLE,
    RECTANGLE,
    LINE
};

enum class BlendMode {
    ALPHA,      // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
    ADDITIVE    // GL_SRC_ALPHA, GL_ONE
};

struct ParticleEmitterConfig {
    // Emission
    EmitterShape shape;
    float shapeRadius;       // for CIRCLE
    float shapeWidth;        // for RECTANGLE
    float shapeHeight;       // for RECTANGLE
    float emissionRate;      // particles per second (0 = one-shot only)
    int burstCount;          // for one-shot: how many to emit at once

    // Velocity
    float minSpeed;
    float maxSpeed;
    float minAngle;          // radians -- direction spread minimum
    float maxAngle;          // radians -- direction spread maximum

    // Acceleration
    float gravityX;
    float gravityY;

    // Lifetime
    float minLifetime;
    float maxLifetime;

    // Size
    float minStartSize;
    float maxStartSize;
    float minEndSize;
    float maxEndSize;

    // Color
    float startR, startG, startB, startA;
    float endR, endG, endB, endA;

    // Rotation
    float minRotationSpeed;
    float maxRotationSpeed;

    // Drag
    float drag;              // 1.0 = no drag, 0.95 = light drag

    // Drift (sine-wave oscillation)
    bool useDrift;
    float minDriftFreq;
    float maxDriftFreq;
    float minDriftAmp;
    float maxDriftAmp;

    // Rendering
    BlendMode blendMode;
    // Texture* texture;     // the texture to use for particles
};
```

### The ParticleSystem Class

```cpp
// ParticleSystem.h -- owns a pool of particles, updates and renders them

class ParticleSystem {
private:
    static const int MAX_PARTICLES = 1000;

    // The particle pool (fixed-size, pre-allocated)
    Particle particles[MAX_PARTICLES];

    // Emitter state
    ParticleEmitterConfig config;
    float emitterX, emitterY;        // emitter world position
    float spawnAccumulator;          // fractional particle accumulator
    bool emitting;                   // is the emitter active?

    // Rendering resources (shared by all particles in this system)
    GLuint quadVAO;
    GLuint quadVBO;
    GLuint instanceVBO;

    // Instance data buffer (rebuilt each frame)
    struct InstanceData {
        float x, y;
        float size;
        float rotation;
        float r, g, b, a;
    };
    InstanceData instanceBuffer[MAX_PARTICLES];

    int searchStart;  // for pool scanning

    // Internal methods
    int findInactiveParticle();
    void spawnParticle();
    void setupRendering();

public:
    ParticleSystem(const ParticleEmitterConfig& config);
    ~ParticleSystem();

    // Called by Game::update()
    void update(float deltaTime);

    // Called by Game::render()
    void render(Shader& shader, const Camera& camera);

    // Control
    void setPosition(float x, float y);
    void start();           // begin emitting
    void stop();            // stop emitting (existing particles finish their lives)
    void burst(int count);  // one-shot: emit N particles immediately

    bool isFinished() const;  // true if not emitting AND all particles dead
};
```

### Complete Update Method

```cpp
// ParticleSystem.cpp

void ParticleSystem::update(float deltaTime) {
    // --- Phase 1: Spawn new particles if emitting ---
    if (emitting && config.emissionRate > 0) {
        spawnAccumulator += config.emissionRate * deltaTime;

        while (spawnAccumulator >= 1.0f) {
            spawnParticle();
            spawnAccumulator -= 1.0f;
        }
    }

    // --- Phase 2: Update all living particles ---
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = particles[i];
        if (!p.active) continue;

        // Apply acceleration to velocity
        p.vx += p.ax * deltaTime;
        p.vy += p.ay * deltaTime;

        // Apply drag (air resistance)
        if (p.drag < 1.0f) {
            p.vx *= p.drag;
            p.vy *= p.drag;
        }

        // Apply velocity to position
        p.x += p.vx * deltaTime;
        p.y += p.vy * deltaTime;

        // Apply sine-wave drift (for snow, leaves)
        if (config.useDrift) {
            p.x += sin(p.age * p.driftFreq + p.driftPhase) * p.driftAmp * deltaTime;
        }

        // Age the particle
        p.age += deltaTime;

        // Check for death
        if (p.age >= p.lifetime) {
            p.active = false;
            continue;
        }

        // Calculate life progress (0.0 to 1.0)
        float t = p.age / p.lifetime;

        // Interpolate size
        p.size = p.startSize + (p.endSize - p.startSize) * t;

        // Interpolate color
        p.r = p.startR + (p.endR - p.startR) * t;
        p.g = p.startG + (p.endG - p.startG) * t;
        p.b = p.startB + (p.endB - p.startB) * t;
        p.a = p.startA + (p.endA - p.startA) * t;

        // Update rotation
        p.rotation += p.rotationSpeed * deltaTime;
    }
}
```

### Complete Render Method (Instanced)

```cpp
void ParticleSystem::render(Shader& shader, const Camera& camera) {
    // Collect active particles into the instance buffer
    int activeCount = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) continue;

        instanceBuffer[activeCount].x = particles[i].x;
        instanceBuffer[activeCount].y = particles[i].y;
        instanceBuffer[activeCount].size = particles[i].size;
        instanceBuffer[activeCount].rotation = particles[i].rotation;
        instanceBuffer[activeCount].r = particles[i].r;
        instanceBuffer[activeCount].g = particles[i].g;
        instanceBuffer[activeCount].b = particles[i].b;
        instanceBuffer[activeCount].a = particles[i].a;
        activeCount++;
    }

    if (activeCount == 0) return;

    // Upload instance data to GPU
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    activeCount * sizeof(InstanceData),
                    instanceBuffer);

    // Setup shader
    shader.use();
    int viewportWidth = camera.getViewportWidth();
    int viewportHeight = camera.getViewportHeight();
    glm::mat4 projection = glm::ortho(
        0.0f, (float)viewportWidth,
        (float)viewportHeight, 0.0f
    );
    shader.setMat4("projection", projection);
    shader.setMat4("view", camera.getViewMatrix());

    // Bind particle texture
    // particleTexture->bind(0);

    // Set blending mode
    glEnable(GL_BLEND);
    if (config.blendMode == BlendMode::ADDITIVE) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // Disable depth writing (particles should not occlude each other)
    glDepthMask(GL_FALSE);

    // Draw all particles in one instanced call
    glBindVertexArray(quadVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, activeCount);
    glBindVertexArray(0);

    // Restore state
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // restore default
}
```

### Integration with the Game

The particle system fits into your existing architecture like this:

```
Game class (owns everything):
┌──────────────────────────────────────────────┐
│                                              │
│  std::unique_ptr<Player> player;             │
│  std::vector<std::unique_ptr<Enemy>> enemies;│
│                                              │
│  // NEW: particle systems                    │
│  std::vector<std::unique_ptr<ParticleSystem>> particleSystems;  │
│                                              │
│  // Dedicated shader for particles           │
│  std::unique_ptr<Shader> particleShader;     │
│                                              │
│  void update(float deltaTime) {              │
│      player->update(deltaTime);              │
│      for (auto& enemy : enemies)             │
│          enemy->update(deltaTime);           │
│                                              │
│      // Update all particle systems          │
│      for (auto& ps : particleSystems)        │
│          ps->update(deltaTime);              │
│                                              │
│      // Clean up finished one-shot systems   │
│      particleSystems.erase(                  │
│          std::remove_if(                     │
│              particleSystems.begin(),        │
│              particleSystems.end(),          │
│              [](const auto& ps) {            │
│                  return ps->isFinished();     │
│              }                               │
│          ),                                  │
│          particleSystems.end()               │
│      );                                      │
│  }                                           │
│                                              │
│  void render() {                             │
│      // Render tilemap first                 │
│      // Render entities                      │
│                                              │
│      // Render particles AFTER entities      │
│      // (particles are visual effects on top)│
│      for (auto& ps : particleSystems)        │
│          ps->render(*particleShader, *camera);│
│  }                                           │
│                                              │
│  // Spawn a fire at a position               │
│  void spawnFire(float x, float y) {          │
│      ParticleEmitterConfig fireConfig;       │
│      // ... fill in fire recipe ...          │
│      auto fire = std::make_unique<           │
│          ParticleSystem>(fireConfig);        │
│      fire->setPosition(x, y);               │
│      fire->start();                          │
│      particleSystems.push_back(              │
│          std::move(fire));                   │
│  }                                           │
│                                              │
│  // Spawn a one-shot dust puff               │
│  void spawnDustPuff(float x, float y) {      │
│      ParticleEmitterConfig dustConfig;       │
│      // ... fill in dust recipe ...          │
│      auto dust = std::make_unique<           │
│          ParticleSystem>(dustConfig);        │
│      dust->setPosition(x, y);               │
│      dust->burst(12);  // emit 12 particles │
│      // Do NOT call start() -- one-shot only │
│      particleSystems.push_back(              │
│          std::move(dust));                   │
│      // isFinished() will return true once   │
│      // all 12 particles have died, and the  │
│      // cleanup in update() will remove it.  │
│  }                                           │
│                                              │
└──────────────────────────────────────────────┘
```

### Memory Ownership Diagram

```
Memory ownership (consistent with your existing pattern):

Game
 ├── owns: std::unique_ptr<ParticleSystem> (one per active effect)
 │         ParticleSystem
 │          ├── owns: Particle[MAX_PARTICLES] (the pool, stack-allocated)
 │          ├── owns: GLuint quadVAO, quadVBO (GPU resources)
 │          ├── owns: GLuint instanceVBO (GPU resources)
 │          └── references: Texture* (non-owning, same as Sprite)
 │
 ├── owns: std::unique_ptr<Shader> particleShader
 │         (shared by all particle systems)
 │
 └── owns: std::unique_ptr<Texture> particleTextures
           (particle textures, shared via raw pointers)

This follows the same ownership model as your entities:
  Game owns resources (unique_ptr<Texture>, unique_ptr<Shader>)
  ParticleSystems reference textures via raw pointers (non-owning)
  The pool is stack-allocated inside ParticleSystem (no heap allocation per particle)
```

### Render Order

```
Complete render order for an HD-2D scene with particles:

1. Clear screen
2. Render far background (parallax layer, blurred)
3. Render tilemap (ground, walls)
4. Render entities sorted by Y position (back to front)
5. Render alpha-blended particles (smoke, dust, leaves, rain, snow)
   └── These are behind/mixed with the scene
6. Render additive-blended particles (fire, sparks, magic)
   └── These glow on top of everything
7. Apply post-processing:
   a. Extract bright pixels (from additive particles)
   b. Blur bright pixels (Gaussian)
   c. Composite bloom back onto scene
   d. Apply depth-of-field blur to foreground/background layers
8. Render UI on top

Steps 5 and 6 are separate because they use different blend modes.
You cannot mix alpha and additive particles in the same draw call
(well, you can with per-particle blend mode in the shader, but
separating them is simpler and sufficient).
```

---

## Glossary

| Term | Definition |
|------|-----------|
| **Particle** | A small, short-lived visual element with position, velocity, and lifetime |
| **Emitter** | The source that spawns particles with configured properties |
| **Emission rate** | How many particles the emitter spawns per second |
| **Lifetime** | How many seconds a particle lives before dying |
| **Age** | How many seconds a particle has been alive |
| **Life progress (t)** | The ratio `age / lifetime`, from 0.0 (birth) to 1.0 (death) |
| **Lerp** | Linear interpolation: `a + (b - a) * t` -- blending between two values |
| **Object pool** | A pre-allocated array of objects that are reused instead of allocated/freed |
| **Spawn accumulator** | A float that tracks fractional particles to spawn, ensuring frame-rate-independent emission |
| **Alpha blending** | Standard transparency: particles partially obscure what is behind them |
| **Additive blending** | Light accumulation: particles add brightness, overlapping areas glow |
| **Instanced rendering** | Drawing one mesh many times with per-instance data, in a single draw call |
| **Batch rendering** | Combining all geometry into one large buffer and drawing in a single call |
| **Vertex attribute divisor** | OpenGL setting that controls whether an attribute advances per vertex or per instance |
| **Drag** | A velocity multiplier less than 1.0 that simulates air resistance |
| **Burst** | A one-shot emission of many particles at once (explosion, impact) |
| **Sub-emitter** | A particle that itself spawns child particles (for trails) |
| **Bloom** | A post-processing effect that makes bright areas bleed light outward |
| **Perlin noise** | A smooth random function used for organic, natural-looking movement patterns |
| **Free list** | A linked list of available pool slots, allowing O(1) allocation |
| **Depth-of-field** | A blur effect that simulates camera focus, blurring foreground/background |
