# Tweening, Interpolation, and Easing Functions

This document covers everything you need to understand about smoothly animating values between states: from the mathematical foundations of interpolation, through the complete catalog of easing functions, to building a full tween system for your game engine. If you have used CSS transitions, you already understand the core concept -- this document explains what happens under the hood and how to build it yourself.

## Table of Contents
- [What Tweening Is](#what-tweening-is)
  - [The Word: In-Betweening](#the-word-in-betweening)
  - [The Web Parallel: CSS Transitions ARE Tweening](#the-web-parallel-css-transitions-are-tweening)
  - [Why Tweening Matters](#why-tweening-matters)
- [Linear Interpolation (Lerp) -- The Foundation](#linear-interpolation-lerp----the-foundation)
  - [The Formula](#the-formula)
  - [What t Represents](#what-t-represents)
  - [Computing t From Time](#computing-t-from-time)
  - [Lerp on Different Types](#lerp-on-different-types)
  - [Lerp vs MoveToward](#lerp-vs-movetoward)
  - [The Lerp Smoothing Pattern](#the-lerp-smoothing-pattern)
- [Easing Functions -- Complete Catalog](#easing-functions----complete-catalog)
  - [The Universal Pattern](#the-universal-pattern)
  - [Linear](#linear)
  - [Ease-In Quadratic](#ease-in-quadratic)
  - [Ease-Out Quadratic](#ease-out-quadratic)
  - [Ease-In-Out Quadratic](#ease-in-out-quadratic)
  - [Ease-In Cubic](#ease-in-cubic)
  - [Ease-Out Cubic](#ease-out-cubic)
  - [Ease-In-Out Cubic](#ease-in-out-cubic)
  - [Ease-In Quartic](#ease-in-quartic)
  - [Ease-Out Quartic](#ease-out-quartic)
  - [Ease-In Back](#ease-in-back)
  - [Ease-Out Back](#ease-out-back)
  - [Ease-In-Out Back](#ease-in-out-back)
  - [Ease-In Elastic](#ease-in-elastic)
  - [Ease-Out Elastic](#ease-out-elastic)
  - [Ease-In-Out Elastic](#ease-in-out-elastic)
  - [Ease-Out Bounce](#ease-out-bounce)
  - [Ease-In Bounce](#ease-in-bounce)
  - [Ease-In-Out Bounce](#ease-in-out-bounce)
  - [Custom Easing: Cubic Bezier Curves](#custom-easing-cubic-bezier-curves)
  - [Robert Penner's Easing Functions](#robert-penners-easing-functions)
- [Tween Systems -- Architecture](#tween-systems----architecture)
  - [What a Tween Object Is](#what-a-tween-object-is)
  - [Tween Lifecycle](#tween-lifecycle)
  - [Tween Manager](#tween-manager)
  - [Chaining and Grouping](#chaining-and-grouping)
  - [Complete Pseudocode](#complete-pseudocode)
- [Practical Game Uses](#practical-game-uses)
- [Bezier Curves for Motion Paths](#bezier-curves-for-motion-paths)
  - [Why Linear Motion Looks Wrong](#why-linear-motion-looks-wrong)
  - [Quadratic Bezier Curves](#quadratic-bezier-curves)
  - [Cubic Bezier Curves](#cubic-bezier-curves)
  - [De Casteljau's Algorithm](#de-casteljaus-algorithm)
  - [The Constant-Speed Problem](#the-constant-speed-problem)
- [Interpolation Beyond Position](#interpolation-beyond-position)
  - [Color Interpolation](#color-interpolation)
  - [Rotation Interpolation](#rotation-interpolation)
  - [Scale Interpolation](#scale-interpolation)
- [Frame-Rate Independence](#frame-rate-independence)
  - [Why Naive Tweening Breaks](#why-naive-tweening-breaks)
  - [Time-Based Tweening](#time-based-tweening)
  - [The Accumulator Pattern](#the-accumulator-pattern)
  - [The Lerp Smoothing Frame-Rate Problem](#the-lerp-smoothing-frame-rate-problem)
- [Advanced Concepts](#advanced-concepts)
  - [Tween Composition](#tween-composition)
  - [Tween Recycling and Pooling](#tween-recycling-and-pooling)
  - [Pausing and Resuming](#pausing-and-resuming)
  - [Time Scaling](#time-scaling)
  - [Reverse, Yoyo, Delay, Repeat](#reverse-yoyo-delay-repeat)
- [Connection to Other Animation Techniques](#connection-to-other-animation-techniques)

---

## What Tweening Is

### The Word: In-Betweening

The word "tween" is short for **in-betweening**. It comes directly from traditional hand-drawn animation at studios like Disney and Fleischer in the 1930s-1960s.

The production pipeline worked like this:

```
Traditional Animation Production Pipeline:

Senior Animator (the "Key Animator"):
  - Draws the KEY POSES (also called "keyframes")
  - These define the important moments of motion
  - Example: arm at rest (frame 1), arm fully extended (frame 12)

Junior Animator (the "In-Betweener"):
  - Draws all the FRAMES BETWEEN the key poses
  - These are the "in-between" frames, or "tweens"
  - Example: frames 2-11, showing the arm gradually moving

                Key Pose A                                Key Pose B
Frame:  1       2     3     4     5     6     7     8     9    10    11    12
        |       |     |     |     |     |     |     |     |     |     |     |
      [ARM   [arm  [arm  [arm  [arm  [arm  [arm  [arm  [arm  [arm  [arm  [ARM
       DOWN]  low]  low]  mid]  mid]  mid]  high] high] high] ext]  ext]  UP!]
        ^                                                                  ^
    KEY POSE                      in-betweens                          KEY POSE
  (senior draws)              (junior draws these)                 (senior draws)
```

In games, the computer is the junior animator. **You define the key states (start and end values), and the engine generates all the intermediate values.** Every frame of your game loop that falls between those key states is an in-between -- a tween.

### The Web Parallel: CSS Transitions ARE Tweening

If you have ever written a CSS transition, you have already used a tween system. Consider this:

```css
.box {
  transform: translateX(0px);
  transition: transform 0.5s ease-out;
}

.box:hover {
  transform: translateX(200px);
}
```

This is tweening. Break it down:

```
CSS Transition                        Game Tween
-----------------------------------------
Start value:    translateX(0px)   →   startPosition = 0.0
End value:      translateX(200px) →   endPosition = 200.0
Duration:       0.5s              →   duration = 0.5
Easing:         ease-out          →   easingFunction = easeOutQuad
Trigger:        :hover            →   some game event (button press, collision)
Who computes    The browser       →   Your game engine
  in-betweens:
```

The browser does exactly what your game engine will do: every frame (every `requestAnimationFrame` tick), it calculates the current value based on how much time has elapsed, applies the easing function, and sets the element's position. Your game engine will do the same thing -- you just have to write the code that the browser gives you for free.

### Why Tweening Matters

Without tweening, every state change in your game is **instantaneous**. The health bar goes from full to empty in a single frame. The camera jumps to the new position. Menu items appear without ceremony. The screen cuts to black with no fade.

```
Without Tweening:                      With Tweening:

Frame 1: Health = 100%                Frame 1:  Health = 100%
Frame 2: Health = 30%   (SNAP!)       Frame 2:  Health = 95%
                                       Frame 3:  Health = 88%
                                       Frame 4:  Health = 78%
                                       Frame 5:  Health = 65%
                                       Frame 6:  Health = 52%
                                       Frame 7:  Health = 41%
                                       Frame 8:  Health = 34%
                                       Frame 9:  Health = 31%
                                       Frame 10: Health = 30%
                                       (smooth drain over ~0.15 seconds)
```

Tweening is the difference between a game that feels **mechanical and harsh** and one that feels **polished and alive**. In the game design community, this kind of polish is often called "juice" -- small details that make interactions feel satisfying. Tweening is probably the single most impactful source of juice available to you.

Think about the difference between a website where navigation changes happen with no transition versus one where elements slide, fade, and scale smoothly. The content is identical, but the experience is fundamentally different. The same applies to games, except even more so, because games are interactive at 60 frames per second and every visual hitch breaks immersion.

---

## Linear Interpolation (Lerp) -- The Foundation

### The Formula

Linear interpolation, universally called **lerp** in game development, is the mathematical heart of all tweening. Everything else is built on top of this single operation.

The formula:

```
result = a + (b - a) * t
```

Where:
- `a` is the start value
- `b` is the end value
- `t` is the progress, ranging from 0.0 to 1.0

There is an alternative form that is mathematically identical:

```
result = a * (1 - t) + b * t
```

This second form makes the "blending" interpretation more obvious: when `t = 0`, you get `a * 1 + b * 0 = a` (100% start). When `t = 1`, you get `a * 0 + b * 1 = b` (100% end). When `t = 0.5`, you get `a * 0.5 + b * 0.5` (50/50 blend). You are mixing two values based on a ratio.

In C++:

```cpp
// Lerp for a single float
float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// Usage examples:
float x = lerp(0.0f, 200.0f, 0.0f);   // Result: 0.0    (at start)
float x = lerp(0.0f, 200.0f, 0.25f);   // Result: 50.0   (25% through)
float x = lerp(0.0f, 200.0f, 0.5f);    // Result: 100.0  (halfway)
float x = lerp(0.0f, 200.0f, 0.75f);   // Result: 150.0  (75% through)
float x = lerp(0.0f, 200.0f, 1.0f);    // Result: 200.0  (at end)
```

Visualized:

```
t:      0.0       0.25       0.5       0.75       1.0
        |----------|----------|----------|----------|
value:  0         50         100        150        200
        a (start)                                  b (end)
```

### What t Represents

`t` is the **normalized progress** through the tween. "Normalized" means it is always in the range [0, 1], regardless of the actual start and end values or the duration.

Think of `t` as a percentage: 0.0 = 0%, 0.5 = 50%, 1.0 = 100%. It answers the question: "how far along are we?"

This normalization is what makes lerp so powerful. The same `t` value works whether you are interpolating from 0 to 100, from -500 to 500, or from 3.14159 to 2.71828. The start and end values can be anything. `t` is always 0 to 1.

### Computing t From Time

In a game, `t` is computed from the elapsed time since the tween started and the total duration of the tween:

```
t = elapsedTime / totalDuration
```

Each frame, you accumulate delta time:

```cpp
// In your tween's update method
void update(float deltaTime) {
    elapsed += deltaTime;

    // Compute normalized progress (clamped to 0-1)
    float t = elapsed / duration;
    if (t > 1.0f) t = 1.0f;

    // Compute the current interpolated value
    float currentValue = lerp(startValue, endValue, t);
}
```

Concrete example: Moving a sprite from X=100 to X=400 over 2 seconds:

```
Frame 1:  dt=0.016s  elapsed=0.016  t=0.016/2.0=0.008   x = lerp(100, 400, 0.008)  = 102.4
Frame 2:  dt=0.016s  elapsed=0.032  t=0.032/2.0=0.016   x = lerp(100, 400, 0.016)  = 104.8
Frame 3:  dt=0.016s  elapsed=0.048  t=0.048/2.0=0.024   x = lerp(100, 400, 0.024)  = 107.2
...
Frame 62: dt=0.016s  elapsed=1.000  t=1.000/2.0=0.500   x = lerp(100, 400, 0.500)  = 250.0
...
Frame 125:dt=0.016s  elapsed=2.000  t=2.000/2.0=1.000   x = lerp(100, 400, 1.000)  = 400.0
```

This is frame-rate independent because you are using elapsed time, not frame count. Whether the game runs at 30fps, 60fps, or 144fps, the tween completes in exactly 2 seconds and arrives at exactly 400.

### Lerp on Different Types

Lerp works on any type where addition, subtraction, and scalar multiplication are defined. This includes essentially every numeric type you will encounter in game development.

**Floats:** The simplest case. Position components, opacity, scale.

```cpp
float opacity = lerp(1.0f, 0.0f, t);    // Fade out: 1.0 → 0.0
float scale   = lerp(0.5f, 1.5f, t);    // Grow: half size → 1.5x size
float volume  = lerp(0.0f, 0.8f, t);    // Fade in audio
```

**Vec2 (position):** Lerp each component independently.

```cpp
glm::vec2 lerpVec2(glm::vec2 a, glm::vec2 b, float t) {
    return glm::vec2(
        lerp(a.x, b.x, t),
        lerp(a.y, b.y, t)
    );
}

// Move from (100, 200) to (500, 50) over time
glm::vec2 start(100.0f, 200.0f);
glm::vec2 end(500.0f, 50.0f);
glm::vec2 current = lerpVec2(start, end, t);
```

GLM actually provides this built-in: `glm::mix(a, b, t)` does exactly lerp.

```cpp
glm::vec2 current = glm::mix(start, end, t);  // Same as lerpVec2
```

**Colors (RGB):** Lerp each color channel.

```cpp
glm::vec3 lerpColor(glm::vec3 colorA, glm::vec3 colorB, float t) {
    return glm::vec3(
        lerp(colorA.r, colorB.r, t),
        lerp(colorA.g, colorB.g, t),
        lerp(colorA.b, colorB.b, t)
    );
}

// Transition from red to blue
glm::vec3 red(1.0f, 0.0f, 0.0f);
glm::vec3 blue(0.0f, 0.0f, 1.0f);
glm::vec3 current = lerpColor(red, blue, 0.5f);  // (0.5, 0.0, 0.5) = purple
```

(Note: RGB lerp has perceptual problems -- more on this in the "Interpolation Beyond Position" section.)

**Why lerping angles is tricky:** Angles have a wrap-around problem. If a character is facing 350 degrees and needs to turn to 10 degrees, the shortest rotation is 20 degrees clockwise (350 -> 360/0 -> 10). But naive lerp computes:

```
lerp(350, 10, 0.5) = 350 + (10 - 350) * 0.5 = 350 + (-340) * 0.5 = 350 - 170 = 180
```

That is 180 degrees -- the character rotated the LONG way around, passing through south instead of taking the short 20-degree turn through north. This is covered in detail in the "Rotation Interpolation" section below.

### Lerp vs MoveToward

These are two fundamentally different approaches that beginners often confuse.

**Lerp** is progress-based. You specify a normalized `t` from 0 to 1, and the object is placed at exactly that fraction of the way between start and end. The object reaches its target at exactly `t = 1.0`, which corresponds to exactly the specified duration.

**MoveToward** is speed-based. You specify a speed (units per second), and the object moves toward the target at that constant rate, regardless of how far away it is.

```cpp
// Lerp: time-based, progress is a ratio
float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// MoveToward: speed-based, progress is a fixed step
float moveToward(float current, float target, float maxStep) {
    if (current < target) {
        return std::min(current + maxStep, target);
    } else {
        return std::max(current - maxStep, target);
    }
}
```

The key behavioral difference:

```
Lerp (moving from 0 to 100 over 1 second):
  t=0.0: position = 0      (start)
  t=0.5: position = 50     (halfway, regardless of frame rate)
  t=1.0: position = 100    (arrives at exactly 1 second)

  If you double the distance (0 to 200), it still takes 1 second.
  The object moves faster to cover more ground.

MoveToward (moving at 100 units/second):
  After 0.5s: position = 50    (moved 50 units at 100/s)
  After 1.0s: position = 100   (arrives at 1 second)

  If you double the distance (0 to 200), it takes 2 seconds.
  The object moves at the same speed, taking longer.
```

Use lerp when you want something to take a specific amount of time (screen transitions, health bar drain, UI animations). Use moveToward when you want something to move at a constant speed regardless of distance (projectile, character walking toward a point, progress bar filling at fixed rate).

### The Lerp Smoothing Pattern

This is one of the most common patterns in game development and deserves special attention because it looks like lerp but behaves very differently.

The pattern:

```cpp
// Every frame:
value = lerp(value, target, factor * dt);
```

Notice: `value` is BOTH the start parameter AND the variable being updated. Each frame, you lerp from the current value toward the target by a small fraction. This creates **exponential decay** -- the value approaches the target quickly at first, then slows down as it gets closer, asymptotically approaching but technically never reaching the target.

```
Lerp smoothing over time (target = 100, factor = 5.0):

Frame 1:  value = lerp(0,    100, 0.083) = 8.3
Frame 2:  value = lerp(8.3,  100, 0.083) = 15.9
Frame 3:  value = lerp(15.9, 100, 0.083) = 22.9
Frame 4:  value = lerp(22.9, 100, 0.083) = 29.3
Frame 5:  value = lerp(29.3, 100, 0.083) = 35.2
...
Frame 20: value = lerp(80.1, 100, 0.083) = 81.7
Frame 30: value = lerp(93.2, 100, 0.083) = 93.8
Frame 60: value = lerp(99.3, 100, 0.083) = 99.4
...never quite reaches 100

Visualized:
  100 ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─  target
   90                  ╱─────────────────────────
   80              ╱───
   70           ╱──
   60         ╱─
   50       ╱─
   40      ╱
   30    ╱─
   20   ╱
   10  ╱
    0 ╱
      └──────────────────────────────────────── time
      Fast at first, then decelerates asymptotically
```

This is extremely useful for:
- **Camera following**: The camera chases the player, moving fast when far away, gently when close. This creates smooth, organic camera movement without any setup.
- **UI smoothing**: A health bar that drains quickly then settles smoothly.
- **Mouse smoothing**: Averaging out jittery input.
- **Value targeting**: Anything where you want to "ease toward" a target that might change at any time.

**Why multiplying by dt is necessary:** Without `dt`, the smoothing factor is applied per-frame, meaning it depends on frame rate. At 60fps, `factor = 0.1` is applied 60 times per second. At 30fps, it is applied only 30 times per second, resulting in slower convergence. Multiplying by `dt` compensates: at 60fps, `dt = 0.016`, so `factor * dt = 0.0016`. At 30fps, `dt = 0.033`, so `factor * dt = 0.0033`. Larger steps at lower frame rates balance out the fewer updates.

However, this simple `factor * dt` multiplication is only an approximation. For a truly frame-rate-independent version, see the "Frame-Rate Independence" section below.

In web development, this pattern is the equivalent of what libraries like `react-spring` or Framer Motion do when they use spring physics -- the value chases a target with decreasing velocity.

---

## Easing Functions -- Complete Catalog

### The Universal Pattern

Every easing function takes a normalized `t` (0 to 1) and returns a modified `t` (also usually 0 to 1, though some functions like "back" and "elastic" temporarily exceed this range). You then use the modified `t` in your lerp:

```cpp
// Without easing: linear motion
float value = lerp(start, end, t);

// With easing: shaped motion
float easedT = easeOutQuad(t);
float value = lerp(start, end, easedT);
```

The easing function does NOT know about start values, end values, or what property is being animated. It only reshapes the progress curve. This separation of concerns is key -- you can combine any easing function with any lerp operation.

In CSS terms:

```css
/* The easing function only controls the CURVE of progress */
transition-timing-function: ease-out;    /* This is the easing function */
transition-property: transform;          /* This is what gets lerped */
transition-duration: 0.5s;               /* This determines how t maps to time */
```

There are three categories of easing:
- **Ease-In**: Starts slow, accelerates. The motion "eases in" to full speed.
- **Ease-Out**: Starts fast, decelerates. The motion "eases out" of full speed.
- **Ease-In-Out**: Starts slow, accelerates in the middle, decelerates at the end.

A powerful mathematical relationship connects ease-in and ease-out: if `f(t)` is an ease-in function, then `1 - f(1 - t)` is the corresponding ease-out function. This means you only need to memorize the ease-in formulas, and you can derive every ease-out for free.

---

### Linear

**Formula:** `f(t) = t`

No easing at all. Progress maps directly to output. Constant speed from start to end.

```
1.0 |              ╱
    |            ╱
    |          ╱
    |        ╱
    |      ╱
    |    ╱
    |  ╱
0.0 |╱
    └────────────────
    0.0            1.0
```

**How it feels:** Mechanical, robotic. Like a conveyor belt. No acceleration, no deceleration. Things start at full speed and stop at full speed, which feels unnatural because nothing in the real world moves this way.

**When to use it:** Progress bars, scrolling text at constant speed, any situation where you explicitly want uniform motion. Also used as the baseline to compare other easing functions against.

```cpp
float easeLinear(float t) {
    return t;
}
```

---

### Ease-In Quadratic

**Formula:** `f(t) = t * t`

The value is squared, meaning small values of `t` produce even smaller output. The motion starts very slowly and accelerates.

```
1.0 |                ╱
    |               ╱
    |             ╱
    |           ╱
    |         ╱
    |       ╱
    |     ╱
    |   ╱
    | ╱─
0.0 |──
    └────────────────
    0.0            1.0
```

**How it feels:** Sluggish at first, then accelerating. Like a car pulling away from a stop light -- slow to get going, then picking up speed. Objects feel heavy, like they have inertia to overcome.

**When to use it:** Objects starting to fall (gravity accelerates), vehicles starting to move, anything that should feel like it is overcoming inertia. In UI, almost never used alone because the abrupt stop at `t=1.0` feels jarring.

```cpp
float easeInQuad(float t) {
    return t * t;
}
```

---

### Ease-Out Quadratic

**Formula:** `f(t) = 1 - (1 - t) * (1 - t)`  or equivalently: `f(t) = t * (2 - t)`

The mirror of ease-in. Starts fast, decelerates to a gentle stop.

```
1.0 |          ──╱
    |        ─╱
    |      ╱─
    |    ╱─
    |   ╱
    |  ╱
    | ╱
    |╱
0.0 |
    └────────────────
    0.0            1.0
```

**How it feels:** Responsive and satisfying. The object reacts immediately (fast start) and settles into place gently (slow end). This is the most "natural" feeling for UI animations because humans expect things to decelerate when arriving.

**When to use it:** Almost any UI animation (menus sliding in, tooltips appearing, elements repositioning). This is the CSS `ease-out` equivalent, and it is the single most useful easing function. If you only implement one easing function, make it this one.

```cpp
float easeOutQuad(float t) {
    return 1.0f - (1.0f - t) * (1.0f - t);
}

// Alternative form (mathematically identical):
float easeOutQuad(float t) {
    return t * (2.0f - t);
}
```

---

### Ease-In-Out Quadratic

**Formula:** Piecewise -- ease-in for the first half, ease-out for the second half.

```
if t < 0.5:  f(t) = 2 * t * t
else:        f(t) = 1 - (-2 * t + 2)^2 / 2
```

```
1.0 |           ──╱──
    |         ╱──
    |       ╱─
    |      ╱
    |    ╱
    |  ─╱
    | ─╱
    |──
0.0 |
    └────────────────
    0.0            1.0
```

**How it feels:** Smooth and balanced. Starts gently, moves quickly through the middle, and settles gently at the end. Like a car accelerating, cruising, then braking. This is the CSS `ease-in-out` equivalent.

**When to use it:** Screen transitions (fade to black and back), camera pans between fixed positions, any motion that should feel deliberate and controlled. Less responsive than ease-out for UI because of the slow start.

```cpp
float easeInOutQuad(float t) {
    if (t < 0.5f) {
        return 2.0f * t * t;
    } else {
        float adjusted = -2.0f * t + 2.0f;
        return 1.0f - (adjusted * adjusted) / 2.0f;
    }
}
```

---

### Ease-In Cubic

**Formula:** `f(t) = t * t * t`

Stronger version of ease-in quadratic. Even slower start, even faster finish.

```
1.0 |                ╱
    |               ╱
    |              ╱
    |            ╱
    |          ╱
    |        ╱
    |      ╱
    |    ╱
    |  ╱─
    |╱──
0.0 |───
    └────────────────
    0.0            1.0
```

**How it feels:** Very sluggish at the start. Like something massive beginning to move -- a boulder, a heavy door. More dramatic than quadratic, appropriate for heavier-feeling objects.

**When to use it:** Heavy objects beginning to move, dramatic reveals, building suspense. The stronger the power (cubic > quadratic > linear), the more dramatic the acceleration.

```cpp
float easeInCubic(float t) {
    return t * t * t;
}
```

---

### Ease-Out Cubic

**Formula:** `f(t) = 1 - (1 - t)^3`  or equivalently: `f(t) = 1 - pow(1 - t, 3)`

```
1.0 |        ────╱
    |      ──╱
    |    ─╱
    |   ╱
    |  ╱
    | ╱
    |╱
    |
0.0 |
    └────────────────
    0.0            1.0
```

**How it feels:** Snappy and decisive. The object launches fast and comes to a very gentle stop. More dramatic than ease-out quadratic. Great for things that should feel like they were "thrown" into position.

**When to use it:** Notifications sliding in, popup menus, damage numbers appearing, anything that should arrive with energy and settle confidently.

```cpp
float easeOutCubic(float t) {
    float invT = 1.0f - t;
    return 1.0f - invT * invT * invT;
}
```

---

### Ease-In-Out Cubic

**Formula:** Piecewise.

```
if t < 0.5:  f(t) = 4 * t * t * t
else:        f(t) = 1 - (-2 * t + 2)^3 / 2
```

```
1.0 |            ───╱──
    |          ╱──
    |        ╱─
    |       ╱
    |      ╱
    |    ╱
    |  ─╱
    | ─╱
    |──
0.0 |──
    └────────────────
    0.0            1.0
```

**How it feels:** More dramatic version of ease-in-out quadratic. The slow start and slow end are more pronounced, with a faster rush through the middle. Appropriate for cinematic camera movements and dramatic transitions.

**When to use it:** Cinematic camera sweeps, dramatic scene transitions, title card animations, anything that should feel "directed" like a film cut.

```cpp
float easeInOutCubic(float t) {
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    } else {
        float adjusted = -2.0f * t + 2.0f;
        return 1.0f - (adjusted * adjusted * adjusted) / 2.0f;
    }
}
```

---

### Ease-In Quartic

**Formula:** `f(t) = t * t * t * t`

Even more extreme ease-in. The object barely moves for the first half, then rockets to the end.

```
1.0 |                 ╱
    |                ╱
    |               ╱
    |             ╱
    |           ╱
    |         ╱
    |       ╱
    |     ╱
    |   ╱─
    | ╱───
0.0 |─────
    └────────────────
    0.0            1.0
```

**How it feels:** Extreme hesitation before explosive acceleration. Like a spring being compressed and then released, or a rocket igniting. Most of the perceived motion happens in the last quarter of the duration.

**When to use it:** Charge-up mechanics (a sword swing where the character pulls back slowly then strikes fast), explosive launches, anything where tension builds before release.

```cpp
float easeInQuartic(float t) {
    return t * t * t * t;
}
```

---

### Ease-Out Quartic

**Formula:** `f(t) = 1 - (1 - t)^4`

```
1.0 |      ──────╱
    |    ───╱
    |   ─╱
    |  ╱
    | ╱
    |╱
    |
    |
0.0 |
    └────────────────
    0.0            1.0
```

**How it feels:** Extremely fast start with a very long, gentle deceleration. The object arrives almost instantly and then takes a long time to fully settle. Like something skidding to a stop on ice.

**When to use it:** Powerful impacts that settle slowly, objects being flung and coming to rest, fast UI elements that gently lock into position.

```cpp
float easeOutQuartic(float t) {
    float invT = 1.0f - t;
    return 1.0f - invT * invT * invT * invT;
}
```

---

### Ease-In Back

**Formula:** `f(t) = c3 * t^3 - c1 * t^2`  where `c1 = 1.70158` and `c3 = c1 + 1`

This function goes BELOW zero before rising. The object pulls backward before moving forward, like a slingshot being drawn back.

```
1.0 |                  ╱
    |                ╱
    |              ╱
    |            ╱
    |          ╱
    |        ╱
    |      ╱
    |    ╱
0.0 |──╱─
    |╱
    |    ← overshoots below 0 (pulls back)
    └────────────────
    0.0            1.0
```

**How it feels:** Anticipation before action. Like pulling back a bowstring, winding up a punch, or coiling before a jump. The brief backward motion makes the forward motion feel more powerful by contrast.

**When to use it:** Menu items that pull back before sliding in, characters winding up for an action, buttons that depress before triggering, any animation where anticipation increases impact.

```cpp
float easeInBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return c3 * t * t * t - c1 * t * t;
}
```

The constant `1.70158` is not arbitrary -- it was chosen by Robert Penner so that the overshoot is exactly 10% of the total distance. You can adjust this value: larger values produce more dramatic pullback.

---

### Ease-Out Back

**Formula:** `f(t) = 1 + c3 * (t - 1)^3 + c1 * (t - 1)^2`  where `c1 = 1.70158` and `c3 = c1 + 1`

The object overshoots PAST the target, then settles back. Like a ball rolling past the hole and coming back.

```
    |       ╱╲
1.0 |──────╱──╲──  ← overshoots above 1.0
    |    ╱─
    |   ╱
    |  ╱
    | ╱
    |╱
0.0 |
    └────────────────
    0.0            1.0
```

**How it feels:** Energetic arrival with a playful overshoot. The object zooms past its destination, then gently pulls back to where it should be. This gives animations a bouncy, lively quality that is extremely common in polished game UI.

**When to use it:** Popup menus and dialogs appearing, notification toasts sliding in, scaling effects (element grows slightly past its target size then shrinks back), any UI element that should feel energetic and "alive."

```cpp
float easeOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float shifted = t - 1.0f;
    return 1.0f + c3 * shifted * shifted * shifted + c1 * shifted * shifted;
}
```

---

### Ease-In-Out Back

**Formula:** Uses an increased overshoot constant for both ends.

```
    |          ╱╲
1.0 |─────────╱──╲─  ← overshoots at end
    |       ╱─
    |      ╱
    |    ╱─
    |  ─╱
    | ─╱
0.0 |─╱──
    | ╲╱  ← overshoots at start
    └────────────────
    0.0            1.0
```

**How it feels:** The object pulls back, accelerates through the middle, overshoots the target, and settles. Very dramatic and attention-grabbing. Like a pendulum that swings past both extremes.

**When to use it:** Full-screen transitions, dramatic reveals, large elements that need to feel weighty and physical.

```cpp
float easeInOutBack(float t) {
    const float c1 = 1.70158f;
    const float c2 = c1 * 1.525f;  // Increased overshoot for in-out

    if (t < 0.5f) {
        float term = 2.0f * t;
        return (term * term * ((c2 + 1.0f) * term - c2)) / 2.0f;
    } else {
        float term = 2.0f * t - 2.0f;
        return (term * term * ((c2 + 1.0f) * term + c2) + 2.0f) / 2.0f;
    }
}
```

---

### Ease-In Elastic

**Formula:** `f(t) = -2^(10t - 10) * sin((10t - 10.75) * (2 * PI / 3))`

Combines exponential growth with a sine wave to create a wobbling, spring-like motion at the start.

```
1.0 |                       ╱
    |                     ╱
    |                   ╱
    |                 ╱
    |               ╱
    |             ╱
    |           ╱
    |         ╱
0.0 |──╱╲──╱╲╱
    |      ╲╱
    | ← wobbles around 0 at the start
    └────────────────────────
    0.0                   1.0
```

**How it feels:** Like pulling a spring and letting it vibrate before moving. Jittery and energetic at the start, then smooth acceleration. Suggests elastic energy being built up.

**When to use it:** Rarely used in isolation. Works for objects being "pulled" into position (like a rubber band stretching), or for comedic wind-up animations.

```cpp
float easeInElastic(float t) {
    const float c4 = (2.0f * M_PI) / 3.0f;

    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    return -powf(2.0f, 10.0f * t - 10.0f) * sinf((10.0f * t - 10.75f) * c4);
}
```

---

### Ease-Out Elastic

**Formula:** `f(t) = 2^(-10t) * sin((10t - 0.75) * (2 * PI / 3)) + 1`

The object arrives and then wobbles/bounces around the target before settling. Like a spring reaching its rest position.

```
         ╱╲
1.0 |──╱╲╱─╲──────
    | ╱
    |╱
    |
    |
    |
    |
    |
0.0 |
    └────────────────────────
    0.0                   1.0
```

**How it feels:** Springy, bouncy arrival. The object reaches the target quickly, overshoots, comes back, overshoots a bit less, comes back, and so on until it settles. Very playful and energetic.

**When to use it:** Item pickups (scale pops up and bounces), character landing effects, achievement popups, any element that should feel like it "springs" into place. This is the easing equivalent of the spring physics described in the procedural animation doc.

```cpp
float easeOutElastic(float t) {
    const float c4 = (2.0f * M_PI) / 3.0f;

    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    return powf(2.0f, -10.0f * t) * sinf((10.0f * t - 0.75f) * c4) + 1.0f;
}
```

---

### Ease-In-Out Elastic

**Formula:** Piecewise combination of ease-in and ease-out elastic.

```
           ╱╲
1.0 |──╱╲─╱─╲──────
    | ╱
    |╱
    |
    |
    |
    |    ╲
0.0 |─╱╲──╲╱───
    |  ╲╱
    └────────────────────────
    0.0                   1.0
```

**How it feels:** Wobbles at the start, rushes through the middle, wobbles at the end. Very dramatic and attention-grabbing. Can feel chaotic if the duration is too long.

**When to use it:** Rare in practice. Works for transformations between two states where both the departure and arrival should feel elastic (e.g., a morph effect, a shape-shifting animation).

```cpp
float easeInOutElastic(float t) {
    const float c5 = (2.0f * M_PI) / 4.5f;

    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    if (t < 0.5f) {
        return -(powf(2.0f, 20.0f * t - 10.0f) * sinf((20.0f * t - 11.125f) * c5)) / 2.0f;
    } else {
        return (powf(2.0f, -20.0f * t + 10.0f) * sinf((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
    }
}
```

---

### Ease-Out Bounce

**Formula:** Piecewise quadratic segments simulating a bouncing ball.

```
                       ╱╲
1.0 |─────────────────╱──╲──
    |            ╱╲  ╱
    |           ╱  ╲╱
    |      ╱╲  ╱
    |     ╱  ╲╱
    |   ╱╲╱
    |  ╱
    | ╱
0.0 |╱
    └────────────────────────
    0.0                   1.0
```

**How it feels:** Exactly like a ball bouncing on the ground. The object falls to the target, bounces up, falls again, bounces smaller, falls again, bounces tiny, settles. Very playful.

**When to use it:** Coins landing, items dropping, notification badges appearing, any object that should feel like it has weight and elasticity against a surface. Great for game "juice."

This is the most complex standard easing function because it uses multiple quadratic segments, each representing one "bounce":

```cpp
float easeOutBounce(float t) {
    const float n1 = 7.5625f;
    const float d1 = 2.75f;

    if (t < 1.0f / d1) {
        // First arc (main fall)
        return n1 * t * t;
    } else if (t < 2.0f / d1) {
        // Second arc (first bounce)
        float adjusted = t - 1.5f / d1;
        return n1 * adjusted * adjusted + 0.75f;
    } else if (t < 2.5f / d1) {
        // Third arc (second bounce)
        float adjusted = t - 2.25f / d1;
        return n1 * adjusted * adjusted + 0.9375f;
    } else {
        // Fourth arc (final settle)
        float adjusted = t - 2.625f / d1;
        return n1 * adjusted * adjusted + 0.984375f;
    }
}
```

---

### Ease-In Bounce

**Formula:** `f(t) = 1 - easeOutBounce(1 - t)`

The reverse of ease-out bounce. The bouncing happens at the START, with the object bouncing up to the target.

```
1.0 |                        ╱
    |                       ╱
    |                      ╱
    |                    ╱
    |                  ╱
    |        ╱╲      ╱
    |  ╱╲  ╱  ╲  ╱╲╱
    |╱╲╱ ╲╱    ╲╱
0.0 |
    └────────────────────────
    0.0                   1.0
```

**How it feels:** The object seems to struggle to get moving, bouncing at the bottom before finally launching. Less commonly used than ease-out bounce, but works for conveying effort or difficulty.

**When to use it:** An object trying to break free, a character struggling to climb, a UI element "trying" to appear. Rare in practice.

```cpp
float easeInBounce(float t) {
    return 1.0f - easeOutBounce(1.0f - t);
}
```

---

### Ease-In-Out Bounce

**Formula:** Piecewise combination of ease-in and ease-out bounce.

```
                           ╱╲
1.0 |─────────────────────╱──╲──
    |              ╱╲   ╱
    |        ╱╲  ╱  ╲ ╱
    |       ╱  ╲╱    ╲
    |   ╱╲╱
    |  ╱
0.0 |╱╲╱
    └────────────────────────
    0.0                   1.0
```

**How it feels:** Bounces at both the start and the end. Very energetic and playful, but can feel excessive. Use sparingly.

**When to use it:** Comedic animations, exaggerated effects, situations where you want maximum "bounce."

```cpp
float easeInOutBounce(float t) {
    if (t < 0.5f) {
        return (1.0f - easeOutBounce(1.0f - 2.0f * t)) / 2.0f;
    } else {
        return (1.0f + easeOutBounce(2.0f * t - 1.0f)) / 2.0f;
    }
}
```

---

### Custom Easing: Cubic Bezier Curves

CSS's `cubic-bezier()` function lets you define custom easing curves using two control points. This is the exact same mathematical construct available in games.

A cubic Bezier easing curve is defined by four points:
- P0 = (0, 0) -- always fixed (start)
- P1 = (x1, y1) -- first control point (you define this)
- P2 = (x2, y2) -- second control point (you define this)
- P3 = (1, 1) -- always fixed (end)

```
In CSS:
  transition-timing-function: cubic-bezier(0.25, 0.1, 0.25, 1.0);
                                           x1    y1    x2    y2

The same curve in your game:
  float easedT = cubicBezierEase(t, 0.25f, 0.1f, 0.25f, 1.0f);

This IS the CSS "ease" default.
```

The challenge: a Bezier curve is parameterized by its own internal `u` parameter, not by `t` directly. You need to find the `u` that produces the desired `t` (x-coordinate), then evaluate the y-coordinate at that `u`. This requires numerical solving (typically Newton-Raphson iteration or bisection):

```cpp
// Evaluate the x-coordinate of the cubic Bezier at parameter u
float bezierX(float u, float x1, float x2) {
    // Bezier formula with P0.x=0 and P3.x=1
    float invU = 1.0f - u;
    return 3.0f * invU * invU * u * x1
         + 3.0f * invU * u * u * x2
         + u * u * u;
}

// Evaluate the y-coordinate of the cubic Bezier at parameter u
float bezierY(float u, float y1, float y2) {
    // Bezier formula with P0.y=0 and P3.y=1
    float invU = 1.0f - u;
    return 3.0f * invU * invU * u * y1
         + 3.0f * invU * u * u * y2
         + u * u * u;
}

// Find the Bezier y-value for a given x-value (t) using bisection
float cubicBezierEase(float t, float x1, float y1, float x2, float y2) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    // Bisection: find the u where bezierX(u) == t
    float lo = 0.0f;
    float hi = 1.0f;
    float u = t;  // initial guess

    // 20 iterations of bisection gives ~6 decimal places of precision
    for (int i = 0; i < 20; i++) {
        float x = bezierX(u, x1, x2);
        if (fabsf(x - t) < 0.00001f) break;  // close enough

        if (x < t) {
            lo = u;
        } else {
            hi = u;
        }
        u = (lo + hi) / 2.0f;
    }

    return bezierY(u, y1, y2);
}
```

Common CSS timing functions expressed as cubic Bezier:

```
CSS keyword        cubic-bezier()             Behavior
--------------------------------------------------------------------
ease               (0.25, 0.1, 0.25, 1.0)    Gentle acceleration, smooth deceleration
ease-in            (0.42, 0.0, 1.0, 1.0)     Slow start, abrupt end
ease-out           (0.0, 0.0, 0.58, 1.0)     Instant start, gentle end
ease-in-out        (0.42, 0.0, 0.58, 1.0)    Gentle start and end
linear             (0.0, 0.0, 1.0, 1.0)      Straight line
```

If you have ever used Chrome DevTools' cubic-bezier editor to drag control points around and see how the curve changes, that is exactly what you are doing here -- defining a curve shape that maps input progress to output progress.

---

### Robert Penner's Easing Functions

Robert Penner's easing equations, published in his 2001 book "Programming Macromedia Flash MX," are the original reference implementation that virtually every easing library in existence derives from. Flash games, jQuery, GSAP, CSS transitions, Unity's animation curves, Godot's Tween node -- they all trace back to Penner's original functions.

Penner's original signature was:

```
function ease(t, b, c, d):
    t = current time
    b = beginning value
    c = change in value (end - start)
    d = duration
```

Modern implementations (like everything in this document) simplify this by normalizing to `t` in [0, 1] and only returning the easing curve value, letting the caller handle the lerp. This is cleaner because it separates the easing curve from the interpolation.

If you ever need to look up an easing function, the definitive reference is [easings.net](https://easings.net/), which visualizes every standard function with interactive curves and provides implementations in multiple languages.

---

## Tween Systems -- Architecture

### What a Tween Object Is

A tween is a data structure that encapsulates everything needed to smoothly animate a value from one state to another over time. Think of it as the game engine equivalent of a CSS `transition` declaration -- it packages together the start state, end state, duration, timing function, and what to do when it finishes.

```
A Tween object contains:

  startValue:     The value at t=0 (e.g., 0.0)
  endValue:       The value at t=1 (e.g., 200.0)
  duration:       How long the tween takes in seconds (e.g., 0.5)
  elapsed:        How much time has passed since the tween started (starts at 0)
  easingFunction: A function pointer/lambda that shapes the progress curve
  onUpdate:       A callback invoked each frame with the current interpolated value
  onComplete:     A callback invoked when the tween finishes
  isComplete:     Whether the tween has finished
  isActive:       Whether the tween is currently running (vs paused)
```

Compare this directly to CSS:

```css
.box {
  /* startValue is the current computed value */
  /* endValue is 200px */
  transform: translateX(200px);

  /* duration = 0.5s */
  transition-duration: 0.5s;

  /* easingFunction = ease-out */
  transition-timing-function: ease-out;

  /* onUpdate: the browser applies the value each frame (implicit) */
  /* onComplete: the 'transitionend' event fires */
}
```

### Tween Lifecycle

A tween moves through a clear sequence of states:

```
Tween Lifecycle:

  CREATED ──→ STARTED ──→ RUNNING ──→ COMPLETED
     │            │           │            │
     │            │           │            └─→ onComplete callback fires
     │            │           │                Tween is marked for removal
     │            │           │
     │            │           └─→ Each frame:
     │            │               1. elapsed += deltaTime
     │            │               2. t = elapsed / duration
     │            │               3. easedT = easingFunction(t)
     │            │               4. value = lerp(start, end, easedT)
     │            │               5. onUpdate(value)
     │            │
     │            └─→ elapsed = 0, isActive = true
     │
     └─→ All parameters set, but not yet running


  Optional states:
  RUNNING ──→ PAUSED ──→ RUNNING  (pause/resume)
  RUNNING ──→ CANCELLED           (abort without completing)
```

### Tween Manager

Individual tweens are simple, but a game will have dozens or hundreds running simultaneously. A **Tween Manager** (sometimes called a Tween Engine) is a central system that owns and updates all active tweens. It parallels how your `Game` class owns and updates all entities.

```
Tween Manager responsibilities:

  1. Store all active tweens (vector or list)
  2. Update all tweens each frame (iterate and call update)
  3. Remove completed tweens (cleanup)
  4. Provide a factory method to create new tweens
  5. Optionally: pause all, resume all, cancel all

Game loop integration:
  ┌──────────────────────────────────────────┐
  │  Game::update(float dt)                  │
  │  {                                       │
  │      player->update(dt);                 │
  │      enemies->update(dt);                │
  │      tweenManager->update(dt);  ← HERE   │
  │      camera->update(dt);                 │
  │  }                                       │
  └──────────────────────────────────────────┘
```

This is exactly how GSAP works in web development -- `gsap.to()` creates a tween, GSAP's internal ticker updates all active tweens each frame, and completed tweens are cleaned up automatically. Your tween manager is your own GSAP.

### Chaining and Grouping

Real animations are almost never a single tween. They are sequences and parallel groups:

**Sequence (chain):** Play A, then when A finishes, play B, then C.

```
Sequence: A then B then C

Time:  0s──────1s──────2s──────3s
       ├── A ──┤── B ──┤── C ──┤
       slide in  pause   fade out

Example: A notification slides in (0.3s), stays visible (2s), fades out (0.5s)
```

**Group (parallel):** Play A, B, and C simultaneously.

```
Group: A and B and C together

Time:  0s──────────1s
       ├──── A ────┤  (X position)
       ├──── B ────┤  (Y position)
       ├──── C ────┤  (opacity)

Example: A damage number moves up AND fades out at the same time
```

**Combined:** Groups within sequences, sequences within groups.

```
Complex animation: Slide in + scale up (parallel), wait, slide out + fade (parallel)

Time:  0s────0.3s────────2.3s────2.8s
       ├─ Group 1 ─┤─ Delay ─┤─ Group 2 ─┤
       │  slide X   │         │  slide X   │
       │  scale up  │         │  fade out  │
```

In CSS, this maps to `@keyframes` with percentage stops, or to animation libraries like GSAP's `timeline()` and Framer Motion's `AnimatePresence`. The concept is identical -- you are orchestrating multiple value changes across time.

### Complete Pseudocode

Here is a complete, practical tween system design. This is pseudocode that maps closely to C++ but omits some boilerplate for clarity:

```cpp
// Easing function type: takes t [0,1], returns eased t [0,1]
// In C++ this would be:  using EaseFunc = std::function<float(float)>;
using EaseFunc = float(*)(float);

// A single tween that interpolates one float value
struct Tween {
    float* target;         // Pointer to the value being animated
    float startValue;      // Value at t=0
    float endValue;        // Value at t=1
    float duration;        // Total duration in seconds
    float elapsed;         // Time elapsed since start
    float delay;           // Seconds to wait before starting
    EaseFunc easing;       // The easing function to apply
    bool isComplete;       // True when tween has finished
    bool isActive;         // True when tween is running (not paused)

    // Optional callbacks
    std::function<void()> onComplete;  // Called when tween finishes

    // Construct a new tween
    Tween(float* targetPtr, float end, float dur, EaseFunc ease)
        : target(targetPtr)
        , startValue(*targetPtr)       // Capture current value as start
        , endValue(end)
        , duration(dur)
        , elapsed(0.0f)
        , delay(0.0f)
        , easing(ease)
        , isComplete(false)
        , isActive(true)
        , onComplete(nullptr)
    {}

    // Update the tween by deltaTime seconds
    void update(float dt) {
        if (isComplete || !isActive) return;

        // Handle delay
        if (delay > 0.0f) {
            delay -= dt;
            return;
        }

        // Advance elapsed time
        elapsed += dt;

        // Compute normalized progress [0, 1]
        float t = elapsed / duration;
        if (t >= 1.0f) {
            t = 1.0f;
            isComplete = true;
        }

        // Apply easing function to reshape the progress curve
        float easedT = easing(t);

        // Interpolate and write the result directly to the target
        *target = startValue + (endValue - startValue) * easedT;

        // Fire completion callback
        if (isComplete && onComplete) {
            onComplete();
        }
    }
};

// The tween manager owns and updates all active tweens
class TweenManager {
public:
    std::vector<std::unique_ptr<Tween>> tweens;

    // Create a new tween and add it to the manager
    Tween* to(float* target, float endValue, float duration, EaseFunc easing) {
        auto tween = std::make_unique<Tween>(target, endValue, duration, easing);
        Tween* ptr = tween.get();
        tweens.push_back(std::move(tween));
        return ptr;  // Return raw pointer so caller can configure callbacks/delay
    }

    // Update all active tweens, remove completed ones
    void update(float dt) {
        for (auto& tween : tweens) {
            tween->update(dt);
        }

        // Remove completed tweens (erase-remove idiom)
        tweens.erase(
            std::remove_if(tweens.begin(), tweens.end(),
                [](const std::unique_ptr<Tween>& t) { return t->isComplete; }
            ),
            tweens.end()
        );
    }

    // Cancel all tweens targeting a specific variable
    void cancelTweensOf(float* target) {
        tweens.erase(
            std::remove_if(tweens.begin(), tweens.end(),
                [target](const std::unique_ptr<Tween>& t) { return t->target == target; }
            ),
            tweens.end()
        );
    }

    // Cancel all active tweens
    void cancelAll() {
        tweens.clear();
    }
};
```

Usage example -- fading in a screen overlay:

```cpp
// In Game or wherever you manage state
TweenManager tweenManager;
float overlayOpacity = 1.0f;  // Start fully opaque (black screen)

// Fade in: opacity goes from 1.0 to 0.0 over 0.5 seconds with ease-out
Tween* fadeIn = tweenManager.to(&overlayOpacity, 0.0f, 0.5f, easeOutQuad);
fadeIn->onComplete = []() {
    // Screen is now fully visible, enable player input
    inputEnabled = true;
};

// In game loop:
void Game::update(float dt) {
    tweenManager.update(dt);
    // overlayOpacity is being modified by the tween each frame
}

void Game::render() {
    // Draw the game world...
    drawWorld();

    // Draw overlay on top with tweened opacity
    drawFullScreenQuad(0.0f, 0.0f, 0.0f, overlayOpacity);
    // Black quad fades from fully opaque to fully transparent
}
```

Compare this to GSAP in web development:

```javascript
// GSAP equivalent
gsap.to(overlay, {
    opacity: 0,
    duration: 0.5,
    ease: "power2.out",
    onComplete: () => {
        inputEnabled = true;
    }
});
```

The API shape is strikingly similar because the underlying problem is identical.

---

## Practical Game Uses

Here are concrete applications of tweening in your game engine, organized by how commonly you will need them.

### Screen Transitions -- Fade In/Out

Tween the opacity of a full-screen colored quad from 0 to 1 (fade to black) or 1 to 0 (fade from black). This is the most basic transition and is used in almost every game.

```cpp
// Fade to black over 0.3 seconds, then switch scenes, then fade from black
float fadeOpacity = 0.0f;

// Step 1: Fade out (screen goes black)
Tween* fadeOut = tweenManager.to(&fadeOpacity, 1.0f, 0.3f, easeInQuad);
fadeOut->onComplete = [this]() {
    // Step 2: Switch the scene while screen is black
    loadNewScene();

    // Step 3: Fade back in
    tweenManager.to(&fadeOpacity, 0.0f, 0.3f, easeOutQuad);
};
```

### UI Animation -- Menu Items Sliding In

Each menu item tweens its X position with a staggered delay, creating a cascading slide-in effect.

```cpp
// Four menu items, each with its own X position
float menuItemX[4] = {-200.0f, -200.0f, -200.0f, -200.0f};  // Start off-screen left
float targetX = 100.0f;  // Final position

for (int i = 0; i < 4; i++) {
    Tween* t = tweenManager.to(&menuItemX[i], targetX, 0.4f, easeOutBack);
    t->delay = i * 0.08f;  // Each item starts 80ms after the previous one
    // easeOutBack gives each item a playful overshoot as it arrives
}
```

### Damage Numbers Floating Up and Fading

A damage number needs to move upward and fade out simultaneously (a parallel tween group).

```cpp
// Damage number state
float dmgY = enemy.y - 20.0f;       // Start above the enemy
float dmgOpacity = 1.0f;             // Start fully visible

// Tween Y upward (float up 40 pixels over 0.8 seconds)
tweenManager.to(&dmgY, enemy.y - 60.0f, 0.8f, easeOutQuad);

// Tween opacity to 0 (fade out over same duration)
Tween* fade = tweenManager.to(&dmgOpacity, 0.0f, 0.8f, easeInQuad);
fade->onComplete = [this]() {
    // Remove the damage number entity when it finishes fading
    removeDamageNumber();
};
```

### Item Pickup Pop Effect

When the player picks up an item, scale it up (pop!) then back down with elastic easing.

```cpp
// Item scale (uniform scale, so one float controls both X and Y)
float itemScale = 1.0f;

// Pop up to 1.5x size
Tween* pop = tweenManager.to(&itemScale, 1.5f, 0.15f, easeOutQuad);
pop->onComplete = [this]() {
    // Then shrink down with elastic bounce
    tweenManager.to(&itemScale, 0.0f, 0.3f, easeInQuad);
};
```

### Character Squash and Stretch on Landing

When a character lands after a jump, briefly compress the sprite vertically and expand horizontally, then spring back.

```cpp
// Called when the character touches the ground after falling
void onLand() {
    // Squash: scaleY goes to 0.7 (compressed), scaleX goes to 1.3 (widened)
    scaleY = 0.7f;
    scaleX = 1.3f;

    // Spring back to normal with elastic easing
    tweenManager.to(&scaleY, 1.0f, 0.4f, easeOutElastic);
    tweenManager.to(&scaleX, 1.0f, 0.4f, easeOutElastic);
}
```

### Health Bar Smooth Drain

When the player takes damage, the health bar does not snap to the new value -- it drains smoothly.

```cpp
// Health bar state
float displayedHealth = 100.0f;  // What the health bar currently shows
float actualHealth = 100.0f;     // The real health value

void takeDamage(float amount) {
    actualHealth -= amount;

    // Cancel any existing health tween (in case of rapid hits)
    tweenManager.cancelTweensOf(&displayedHealth);

    // Smoothly drain to new health over 0.5 seconds
    tweenManager.to(&displayedHealth, actualHealth, 0.5f, easeOutCubic);
}
```

### Camera Zoom on Area Transition

When entering a new area (like going from a forest to a town), smoothly adjust the camera zoom.

```cpp
void onEnterTown() {
    // Zoom in slightly (scale from 1.0 to 1.5) over 1 second
    tweenManager.to(&camera.zoom, 1.5f, 1.0f, easeInOutQuad);
}

void onExitTown() {
    // Zoom back out
    tweenManager.to(&camera.zoom, 1.0f, 1.0f, easeInOutQuad);
}
```

### Screen Shake

Rapid, decaying tweens of the camera offset create a shake effect for impacts, explosions, or earthquakes.

```cpp
void triggerScreenShake(float intensity, float duration) {
    // Shake X and Y offsets with decaying amplitude
    // One approach: tween the shake amplitude from full to zero
    shakeAmplitude = intensity;
    tweenManager.to(&shakeAmplitude, 0.0f, duration, easeOutQuad);

    // Each frame, while shakeAmplitude > 0:
    // cameraOffsetX = sin(time * shakeFrequency) * shakeAmplitude;
    // cameraOffsetY = cos(time * shakeFrequency * 1.3) * shakeAmplitude;
    // The tween handles the decay, the sine/cosine handle the oscillation
}
```

### Text Appearing Letter by Letter

Tween an integer (or float cast to int) that represents how many characters of a string to show.

```cpp
std::string dialogText = "Welcome to Pelican Town!";
float visibleChars = 0.0f;

// Reveal all characters over the length-proportional duration
float charsPerSecond = 30.0f;
float revealDuration = dialogText.length() / charsPerSecond;
tweenManager.to(&visibleChars, (float)dialogText.length(), revealDuration, easeLinear);

// When rendering:
// std::string visible = dialogText.substr(0, (int)visibleChars);
// drawText(visible);
```

---

## Bezier Curves for Motion Paths

### Why Linear Motion Looks Wrong

When you lerp a position from A to B, the object moves in a straight line. This works for many situations, but it looks wrong for anything that should have a natural arc: projectiles, jumping, throwing items, magic spells traveling through the air.

```
Linear motion (lerp):              Natural motion (curved path):

 A ─────────────── B               A
                                     ╲
                                      ╲    ← arcing upward
                                       ╲
                                        ●  ← apex
                                       ╱
                                      ╱    ← falling down
                                     ╱
                                    B

The straight line looks artificial.   The curve looks like real physics.
```

Bezier curves let you define curved paths that objects follow. You specify control points that "pull" the path into a curve, and the math generates smooth, continuous motion along that path.

### Quadratic Bezier Curves

A quadratic Bezier has three points: start (P0), one control point (P1), and end (P2). The control point "pulls" the path toward itself without the path necessarily passing through it.

```
Quadratic Bezier with one control point:

          P1 (control point -- path curves toward this)
          ●
         ╱ ╲
        ╱   ╲
       ╱     ╲
      ╱       ╲
 P0 ●          ● P2
 (start)      (end)

Actual path traced:

 P0 ●
      ╲
       ╲
        ╲──── curve pulled toward P1
         ╲
          ╲
           ● P2

The path curves toward P1 but does not touch it (in general).
```

The formula:

```
B(t) = (1-t)^2 * P0 + 2*(1-t)*t * P1 + t^2 * P2
```

In C++:

```cpp
glm::vec2 quadraticBezier(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, float t) {
    float invT = 1.0f - t;
    return invT * invT * p0
         + 2.0f * invT * t * p1
         + t * t * p2;
}

// Example: a thrown item arcing from player to target
glm::vec2 playerPos(100.0f, 300.0f);       // P0: start (player hand)
glm::vec2 controlPt(250.0f, 100.0f);       // P1: high above midpoint (arc apex)
glm::vec2 targetPos(400.0f, 300.0f);       // P2: end (landing spot)

// Each frame: compute position along the arc
float t = elapsed / throwDuration;
glm::vec2 itemPos = quadraticBezier(playerPos, controlPt, targetPos, t);
```

### Cubic Bezier Curves

A cubic Bezier has four points: start (P0), two control points (P1, P2), and end (P3). Two control points allow S-curves, loops, and more complex shapes.

```
Cubic Bezier with two control points:

     P1 ●
    ╱    ╲
   ╱      ╲
  ╱        ╲           ● P2
 ╱          ╲         ╱
P0 ●         ╲       ╱
              ╲     ╱
               ╲   ╱
                ╲ ╱
                 ● P3

Actual path:

P0 ●
    ╲
     ╲
      ╲───╲
           ╲──────╱
                 ╱
                ╱
               ● P3

The path is pulled toward P1 near the start and toward P2 near the end.
```

The formula:

```
B(t) = (1-t)^3 * P0 + 3*(1-t)^2*t * P1 + 3*(1-t)*t^2 * P2 + t^3 * P3
```

In C++:

```cpp
glm::vec2 cubicBezier(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, float t) {
    float invT = 1.0f - t;
    float invT2 = invT * invT;
    float invT3 = invT2 * invT;
    float t2 = t * t;
    float t3 = t2 * t;
    return invT3 * p0
         + 3.0f * invT2 * t * p1
         + 3.0f * invT * t2 * p2
         + t3 * p3;
}
```

### De Casteljau's Algorithm

De Casteljau's algorithm is an alternative, more intuitive way to evaluate Bezier curves. Instead of using the polynomial formula directly, it works by recursively subdividing the control points.

The idea: to find the point at parameter `t`, you lerp between each pair of adjacent control points, then lerp between those results, and repeat until you have a single point.

```
De Casteljau's for a cubic Bezier at t=0.4:

Level 0 (original points):
  P0 ●─────────── P1 ●──────────── P2 ●─────────── P3 ●

Level 1 (lerp adjacent pairs at t=0.4):
  A = lerp(P0, P1, 0.4) ●──────── B = lerp(P1, P2, 0.4) ●──────── C = lerp(P2, P3, 0.4) ●

Level 2 (lerp again):
  D = lerp(A, B, 0.4) ●────────── E = lerp(B, C, 0.4) ●

Level 3 (final lerp):
  Result = lerp(D, E, 0.4) ●   ← This is the point on the curve at t=0.4
```

```cpp
// De Casteljau's algorithm for cubic Bezier
glm::vec2 deCasteljau(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, float t) {
    // Level 1: lerp between adjacent pairs
    glm::vec2 a = glm::mix(p0, p1, t);
    glm::vec2 b = glm::mix(p1, p2, t);
    glm::vec2 c = glm::mix(p2, p3, t);

    // Level 2: lerp between the Level 1 results
    glm::vec2 d = glm::mix(a, b, t);
    glm::vec2 e = glm::mix(b, c, t);

    // Level 3: final lerp gives the point on the curve
    return glm::mix(d, e, t);
}
```

De Casteljau's is mathematically equivalent to the polynomial formula but is more numerically stable (less floating-point error) and easier to understand. It is also the basis for **subdivision** -- splitting a Bezier curve into two smaller Bezier curves at any point, which is useful for collision detection and adaptive rendering.

### Using Bezier Paths in Games

Practical applications for your Stardew Valley clone:

```
Projectile arcs (thrown items, arrows):
  P0 = player position
  P1 = high point above midpoint (controls arc height)
  P2 = target position

              P1 (apex)
              ●
           ╱     ╲
         ╱         ╲
       ╱             ╲
  P0 ●                 ● P2
  (throw)            (land)


Character jump paths:
  P0 = ground position at jump start
  P1 = high above the jump midpoint
  P2 = ground position at landing

  This gives a parabolic arc without needing physics simulation.


Magic spell trajectories (homing or curved):
  P0 = caster position
  P1 = control point offset to one side (creates a sweeping curve)
  P2 = control point near the target
  P3 = target position

  P1 ●
       ╲
        ╲
    P0 ● ╲
          ╲
           ╲       ● P2
            ╲     ╱
             ╲   ╱
              ╲ ╱
               ● P3 (target)

  This creates a spell that curves through the air rather than flying straight.
```

### The Constant-Speed Problem

There is a critical subtlety with Bezier curves: **the parameter `t` does NOT correspond to distance along the curve.** When you increment `t` by 0.01 each step, the object does NOT move 1% of the curve's total length. It moves faster in some sections and slower in others, depending on the curvature.

```
Bezier parameterization is NOT uniform:

Points at equal t intervals (0.0, 0.1, 0.2, 0.3, ... 1.0):

  ●●●●● ● ● ●   ●    ●    ●     ●      ●       ●     ●  ● ● ●●●●●
  ^^^^^                                                      ^^^^^
  Points    Points are spread far apart in the            Points
  cluster   middle where the curve is straighter          cluster
  where                                                   at the
  curve                                                   end
  bends

  The object SLOWS DOWN in curved regions and SPEEDS UP in straight regions.
```

For many game uses (projectile arcs, spell effects), this non-uniform speed is acceptable or even desirable -- the object naturally slows at the apex of an arc, which looks like gravity.

But if you need **constant speed** along a Bezier path (e.g., a character walking along a curved road at uniform walking speed), you need **arc-length parameterization**. The idea:

1. **Pre-compute** a lookup table that maps distance-along-curve to the `t` parameter
2. To find the position at distance `d`, look up the corresponding `t` in the table
3. Evaluate the Bezier at that `t`

```cpp
// Pre-compute an arc-length table for a cubic Bezier
// Sample many points, compute cumulative distance
struct ArcLengthTable {
    static const int SAMPLES = 100;
    float lengths[SAMPLES + 1];     // Cumulative arc length at each sample
    float totalLength;

    void build(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3) {
        lengths[0] = 0.0f;
        glm::vec2 prev = p0;

        for (int i = 1; i <= SAMPLES; i++) {
            float t = (float)i / (float)SAMPLES;
            glm::vec2 current = cubicBezier(p0, p1, p2, p3, t);
            lengths[i] = lengths[i - 1] + glm::length(current - prev);
            prev = current;
        }

        totalLength = lengths[SAMPLES];
    }

    // Given a distance along the curve, find the corresponding t parameter
    float distanceToT(float distance) {
        // Binary search through the length table
        int lo = 0, hi = SAMPLES;
        while (lo < hi - 1) {
            int mid = (lo + hi) / 2;
            if (lengths[mid] < distance) {
                lo = mid;
            } else {
                hi = mid;
            }
        }

        // Linear interpolation between the two surrounding samples
        float segmentLength = lengths[hi] - lengths[lo];
        if (segmentLength < 0.0001f) return (float)lo / (float)SAMPLES;

        float fraction = (distance - lengths[lo]) / segmentLength;
        return ((float)lo + fraction) / (float)SAMPLES;
    }
};

// Usage: constant-speed travel along a Bezier path
float distanceTraveled = speed * elapsedTime;
float t = arcTable.distanceToT(distanceTraveled);
glm::vec2 position = cubicBezier(p0, p1, p2, p3, t);
```

This is a standard technique used in racing games (cars on tracks), rail shooters (camera on a path), and any situation requiring precise speed control along a curve.

---

## Interpolation Beyond Position

### Color Interpolation

**RGB Lerp:** The simplest approach -- lerp each channel (R, G, B) independently. This works but has a perceptual problem:

```
RGB lerp from red to cyan (aqua):

  t=0.0:  (255,   0,   0) = Red
  t=0.25: (191,  64,  64) = Dark desaturated reddish
  t=0.5:  (128, 128, 128) = GRAY          ← Problem! Muddy middle
  t=0.75: ( 64, 191, 191) = Dark desaturated cyan
  t=1.0:  (  0, 255, 255) = Cyan

The midpoint is gray! Transitioning between two vivid colors passes
through an ugly, desaturated intermediate.

  Red ── dark red ── GRAY ── dark cyan ── Cyan
                      ↑
              This looks muddy and wrong
```

Why does this happen? In RGB space, the "shortest path" between red (1,0,0) and cyan (0,1,1) passes through the origin (0,0,0), which is dark gray/black. The colors lose saturation in the middle.

**HSL/HSV Lerp:** A perceptually better approach. Convert to Hue-Saturation-Lightness (or Hue-Saturation-Value), lerp in that space, then convert back to RGB.

```
HSL lerp from red to cyan:

  Red in HSL:  H=0,   S=100%, L=50%
  Cyan in HSL: H=180, S=100%, L=50%

  t=0.0:  H=0,   S=100%, L=50% = Red
  t=0.25: H=45,  S=100%, L=50% = Orange
  t=0.5:  H=90,  S=100%, L=50% = Green        ← Vivid!
  t=0.75: H=135, S=100%, L=50% = Spring Green
  t=1.0:  H=180, S=100%, L=50% = Cyan

  Red ── Orange ── Green ── Spring ── Cyan
                    ↑
          Stays vivid and saturated throughout!
```

The saturation stays at 100% the entire time because you are lerping the hue angle independently. The colors remain vivid.

**When to use which:**
- **RGB lerp**: Fine for colors that are similar (light blue to dark blue, one shade of green to another). Also fine for fading to black or white (lerp toward (0,0,0) or (1,1,1)).
- **HSL/HSV lerp**: Better for transitions between different hues (red to blue, green to purple). Preserves vibrancy. But note that hue itself is an angle and has the wrap-around problem (see below).

### Rotation Interpolation

**The angle wrap-around problem:** Angles wrap at 360 degrees (or 2*PI radians). If a character faces 350 degrees and needs to turn to 10 degrees, the shortest rotation is +20 degrees (clockwise through 360/0), but naive lerp computes -340 degrees (counter-clockwise the long way around).

```
The wrap-around problem:

                    0/360
                     |
              330    |    30
                 ╲   |   ╱
                  ╲  |  ╱
            300    ╲ | ╱    60
                ────●────
            270     |     90
                ────●────
            240    ╱ | ╲    120
                  ╱  |  ╲
                 ╱   |   ╲
              210    |    150
                    180

Current facing: 350 degrees (just left of north)
Target facing:  10 degrees (just right of north)

Shortest path: 350 → 360/0 → 10  (20 degrees clockwise)
Naive lerp:    350 → 180 → 10    (340 degrees counter-clockwise!)
```

The fix is to compute the shortest angular difference before lerping:

```cpp
// Shortest-arc angle lerp
float lerpAngle(float a, float b, float t) {
    // Compute the difference
    float diff = b - a;

    // Wrap the difference to [-180, 180] (shortest arc)
    while (diff > 180.0f)  diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;

    // Now lerp along the shortest path
    return a + diff * t;
}

// Example:
float result = lerpAngle(350.0f, 10.0f, 0.5f);
// diff = 10 - 350 = -340
// After wrapping: diff = -340 + 360 = 20
// result = 350 + 20 * 0.5 = 360 (= 0 degrees)
// Correct! Midway between 350 and 10, going through north.
```

If working in radians:

```cpp
float lerpAngleRadians(float a, float b, float t) {
    float diff = b - a;
    while (diff > M_PI)  diff -= 2.0f * M_PI;
    while (diff < -M_PI) diff += 2.0f * M_PI;
    return a + diff * t;
}
```

This same wrap-around problem applies to hue interpolation in HSL/HSV color space, since hue is an angle (0-360 degrees). To lerp between red (hue=0) and magenta (hue=300), the shortest path goes backward through 330, 300 -- not forward through 60, 120, 180, 240, 300.

**Why quaternions exist (3D context):** In 3D, rotation gets much more complicated. Euler angles (yaw, pitch, roll) suffer from gimbal lock -- a condition where two rotation axes align, causing a loss of a degree of freedom. Quaternions are a 4-component mathematical object that represents 3D rotations without gimbal lock and supports smooth interpolation (called "slerp" -- spherical linear interpolation). For your 2D game, you only need the angle lerp shown above. But knowing that quaternions exist explains why 3D engines like Unity expose rotations as `Quaternion` objects rather than three float angles.

### Scale Interpolation

For most purposes, linear interpolation of scale works fine. Tweening `scaleX` from 1.0 to 2.0 produces visually acceptable results.

However, for **zoom** (camera scale), linear interpolation can feel wrong because human perception of size is logarithmic. Zooming from 1x to 2x feels like a "bigger change" than zooming from 8x to 9x, even though both are +1x. This is because doubling (1x to 2x) is perceived as the same magnitude of change as doubling again (2x to 4x).

Logarithmic lerp for zoom:

```cpp
// Linear lerp for zoom: feels like it accelerates
float zoom = lerp(1.0f, 8.0f, t);
// At t=0.5: zoom = 4.5  (midpoint between 1 and 8)

// Logarithmic lerp for zoom: feels like constant perceived speed
float zoom = expf(lerp(logf(1.0f), logf(8.0f), t));
// At t=0.5: zoom = exp(lerp(0, 2.08, 0.5)) = exp(1.04) ≈ 2.83
// 2.83 is the geometric mean of 1 and 8 (sqrt(1*8) ≈ 2.83)
```

The geometric midpoint (2.83) feels like the perceptual halfway point when zooming, not the arithmetic midpoint (4.5). Try zooming in a map application -- the zoom speed feels constant because map apps use logarithmic interpolation.

---

## Frame-Rate Independence

### Why Naive Tweening Breaks

If you base your tween progress on frame count instead of elapsed time, the tween runs at different speeds on different machines:

```
Naive approach: advance t by a fixed amount each frame

At 60fps:
  Frame 1: t += 0.016  →  t = 0.016
  Frame 2: t += 0.016  →  t = 0.032
  ...
  Frame 60: t = 1.0     (tween completes in 1 second)

At 30fps:
  Frame 1: t += 0.016  →  t = 0.016
  Frame 2: t += 0.016  →  t = 0.032
  ...
  Frame 30: t = 0.5     (after 1 second, only halfway!)
  Frame 60: t = 1.0     (tween takes 2 seconds!)

The tween takes TWICE as long at 30fps because there are half as many frames.
```

### Time-Based Tweening

The fix is straightforward: base progress on real elapsed time, not frame count.

```cpp
void Tween::update(float deltaTime) {
    elapsed += deltaTime;
    float t = elapsed / duration;
    if (t > 1.0f) t = 1.0f;

    // This produces identical results regardless of frame rate:
    // At 60fps: elapsed accumulates in ~0.016s increments
    // At 30fps: elapsed accumulates in ~0.033s increments
    // But after 1 second of real time, elapsed = 1.0 in both cases
}
```

This is what all the tween code in this document already does. The critical insight: `deltaTime` (the real time between frames) naturally compensates for frame rate differences. Each frame advances the clock by the actual amount of time that passed, not by a fixed fictional amount.

In CSS, this is handled automatically -- the browser's animation system uses wall-clock time internally. In games, you must do it yourself.

### The Accumulator Pattern

What happens when `deltaTime` is very large? If the game hitches for 0.5 seconds (due to loading, garbage collection, or any spike), a single `deltaTime` of 0.5 seconds could skip most of a tween's duration in one frame. The tween would jump from near-start to near-end with no visible interpolation.

The accumulator pattern handles this by breaking large time steps into smaller fixed steps:

```cpp
void Tween::update(float deltaTime) {
    const float MAX_STEP = 1.0f / 60.0f;  // Never process more than 1/60s at once

    float remaining = deltaTime;
    while (remaining > 0.0f) {
        float step = std::min(remaining, MAX_STEP);
        elapsed += step;
        remaining -= step;
    }

    float t = std::min(elapsed / duration, 1.0f);
    float easedT = easing(t);
    *target = startValue + (endValue - startValue) * easedT;
}
```

In practice, for simple tweens (where you only care about the final position, not the path), you can skip the accumulator and just clamp `t` to 1.0. The accumulator matters more for physics simulations and for tweens where intermediate values trigger side effects (like a progress bar that fires events at certain thresholds).

### The Lerp Smoothing Frame-Rate Problem

The "lerp smoothing" pattern mentioned earlier has a more insidious frame-rate dependency. Simply multiplying by `dt` is an approximation that breaks at extreme frame rates:

```
value = lerp(value, target, factor * dt);

At 60fps (dt = 0.0167):  per-frame factor = 0.1 * 0.0167 = 0.00167
  After 1 second (60 frames): value ≈ 90.5% of the way to target

At 30fps (dt = 0.0333):  per-frame factor = 0.1 * 0.0333 = 0.00333
  After 1 second (30 frames): value ≈ 90.5% of the way to target

Looks the same! But actually, because lerp smoothing is EXPONENTIAL,
the simple factor*dt approximation only works when factor*dt is small.

At 10fps (dt = 0.1):     per-frame factor = 0.1 * 0.1 = 0.01
  After 1 second (10 frames): value ≈ 90.4%  ← slightly different!

At 5fps (dt = 0.2):      per-frame factor = 0.1 * 0.2 = 0.02
  After 1 second (5 frames):  value ≈ 90.4%  ← still close but diverging
```

The truly frame-rate independent version uses the exponential decay formula:

```cpp
// Frame-rate independent lerp smoothing
// 'halflife' is the time (in seconds) for the value to move halfway to the target
float smoothDamp(float current, float target, float halflife, float dt) {
    // The decay factor per second: how much of the remaining distance is covered
    // Using pow to correctly handle arbitrary dt values
    float factor = 1.0f - powf(0.5f, dt / halflife);
    return current + (target - current) * factor;
}

// Usage: camera follows player with a 0.1 second half-life
camera.x = smoothDamp(camera.x, player.x, 0.1f, deltaTime);
camera.y = smoothDamp(camera.y, player.y, 0.1f, deltaTime);
```

The `pow(0.5, dt / halflife)` formulation is mathematically exact for exponential decay regardless of frame rate. The "halflife" parameter is intuitive: it is the number of seconds for the value to close half the remaining gap. A halflife of 0.1 seconds means the value is always within 50% of the target after 0.1s, within 25% after 0.2s, within 12.5% after 0.3s, and so on.

An alternative formulation uses a "smoothing factor" from 0 to 1:

```cpp
// Alternative: factor-based (0 = no movement, 1 = instant snap)
float smoothDampFactor(float current, float target, float factor, float dt) {
    return current + (target - current) * (1.0f - powf(1.0f - factor, dt * 60.0f));
}

// This normalizes to 60fps: a factor of 0.1 at 60fps gives the same
// result as at 30fps or 144fps
```

The `dt * 60.0f` normalizes the exponent to a 60fps baseline, so `factor` values "feel" the same way they would if you were running at 60fps without the correction. This is a common convention in game engines.

---

## Advanced Concepts

### Tween Composition

A meta-tween controls other tweens. For example, a "camera shake" tween might internally create and destroy oscillation tweens. Or a "typewriter text" tween might spawn individual letter-appearance tweens.

```cpp
// A composite tween that orchestrates multiple sub-tweens
class CompositeAnimation {
    TweenManager& manager;
    std::vector<std::function<void()>> steps;
    int currentStep = 0;

    void runNextStep() {
        if (currentStep < steps.size()) {
            steps[currentStep]();
            currentStep++;
        }
    }

public:
    CompositeAnimation(TweenManager& mgr) : manager(mgr) {}

    // Add a step that creates tweens and calls runNextStep on completion
    void addStep(std::function<void()> step) {
        steps.push_back(step);
    }

    void start() {
        runNextStep();
    }
};
```

### Tween Recycling and Pooling

If your game creates and destroys many tweens per frame (e.g., particle effects where each particle has tweened opacity), the allocation overhead of `std::make_unique<Tween>` can add up.

An object pool pre-allocates a fixed number of tween objects and recycles them:

```cpp
class TweenPool {
    static const int POOL_SIZE = 256;
    Tween pool[POOL_SIZE];
    bool inUse[POOL_SIZE];

public:
    TweenPool() {
        // Mark all tweens as available
        for (int i = 0; i < POOL_SIZE; i++) {
            inUse[i] = false;
        }
    }

    Tween* acquire() {
        for (int i = 0; i < POOL_SIZE; i++) {
            if (!inUse[i]) {
                inUse[i] = true;
                return &pool[i];
            }
        }
        return nullptr;  // Pool exhausted
    }

    void release(Tween* tween) {
        int index = tween - pool;  // Pointer arithmetic to find index
        if (index >= 0 && index < POOL_SIZE) {
            inUse[index] = false;
        }
    }
};
```

This eliminates heap allocation entirely. Each tween reuse is just flipping a boolean and resetting the tween's fields.

In web dev, this is equivalent to how virtual DOM libraries reuse DOM nodes instead of creating/destroying them, or how connection pools reuse database connections.

### Pausing and Resuming

Sometimes you need to pause all animation -- when the game pauses, when a dialog opens, when the player enters a menu.

```cpp
class TweenManager {
    bool paused = false;

public:
    void pause() { paused = true; }
    void resume() { paused = false; }

    void update(float dt) {
        if (paused) return;  // Skip all tween updates

        for (auto& tween : tweens) {
            tween->update(dt);
        }
        // ... cleanup completed tweens
    }
};
```

You can also pause individual tweens:

```cpp
Tween* healthBarTween = tweenManager.to(&healthDisplay, newHealth, 0.5f, easeOutCubic);
healthBarTween->isActive = false;  // Paused
// ... later ...
healthBarTween->isActive = true;   // Resumed -- continues from where it left off
```

### Time Scaling

Time scaling lets you speed up or slow down all tweens simultaneously. This is how slow-motion effects work -- you multiply `deltaTime` by a scale factor before passing it to the tween system.

```cpp
class TweenManager {
    float timeScale = 1.0f;  // 1.0 = normal, 0.5 = half speed, 2.0 = double speed

public:
    void setTimeScale(float scale) { timeScale = scale; }

    void update(float dt) {
        float scaledDt = dt * timeScale;

        for (auto& tween : tweens) {
            tween->update(scaledDt);
        }
        // ...
    }
};

// Slow motion on big hit:
void onCriticalHit() {
    tweenManager.setTimeScale(0.2f);  // Everything at 20% speed

    // Restore normal speed after 0.5 real seconds
    // (Use a timer that ignores timeScale, or use wall-clock time)
    scheduleRestore(0.5f);
}
```

This naturally works because every tween uses `deltaTime` for progress. Scaling `deltaTime` scales all motion uniformly. Characters move slower, particles drift lazily, camera effects stretch out -- all from changing one number.

### Reverse, Yoyo, Delay, Repeat

These are standard tween modifiers found in every tween library:

```cpp
struct Tween {
    // ... existing fields ...

    float delay = 0.0f;          // Seconds to wait before starting
    int repeatCount = 0;         // 0 = play once, 1 = play twice, -1 = infinite
    int currentRepeat = 0;       // How many times we have repeated so far
    bool yoyo = false;           // If true, alternate forward and backward
    bool reversed = false;       // If true, play from end to start
    float speed = 1.0f;          // Playback speed multiplier

    void update(float dt) {
        if (!isActive || isComplete) return;

        // Handle delay
        if (delay > 0.0f) {
            delay -= dt;
            return;
        }

        // Advance time
        elapsed += dt * speed;

        // Compute raw t
        float t = elapsed / duration;

        // Check for completion of one cycle
        if (t >= 1.0f) {
            if (repeatCount == 0 || (repeatCount > 0 && currentRepeat >= repeatCount)) {
                // Truly finished
                t = 1.0f;
                isComplete = true;
            } else {
                // Repeat: reset elapsed, increment counter
                elapsed -= duration;
                currentRepeat++;

                if (yoyo) {
                    reversed = !reversed;  // Flip direction each cycle
                }

                t = elapsed / duration;
            }
        }

        // Apply reverse
        if (reversed) t = 1.0f - t;

        // Apply easing and update target
        float easedT = easing(t);
        *target = startValue + (endValue - startValue) * easedT;

        if (isComplete && onComplete) {
            onComplete();
        }
    }
};

// Usage examples:

// Pulse an element's opacity (yoyo between 0.5 and 1.0 forever)
Tween* pulse = tweenManager.to(&glowOpacity, 0.5f, 0.8f, easeInOutQuad);
pulse->yoyo = true;
pulse->repeatCount = -1;  // Infinite

// Bounce an icon with delay (wait 1 second, then play)
Tween* bounce = tweenManager.to(&iconY, iconY - 20.0f, 0.3f, easeOutBounce);
bounce->delay = 1.0f;

// Play an animation 3 times (initial + 3 repeats = 4 total)
Tween* flash = tweenManager.to(&spriteAlpha, 0.0f, 0.1f, easeLinear);
flash->yoyo = true;
flash->repeatCount = 3;
// This flashes: visible → invisible → visible → invisible → visible → invisible → visible → invisible
// Each cycle takes 0.1 seconds, total = 0.4 seconds
```

---

## Connection to Other Animation Techniques

Tweening is not an isolated system -- it is a fundamental building block that appears in nearly every other animation and rendering technique in game development.

### Skeletal Animation Keyframes

Skeletal animation (used in Spine, DragonBones, and 3D engines) defines key poses at specific timestamps. The runtime **tweens between keyframes** using exactly the interpolation described in this document. Each bone's rotation, translation, and scale are lerped (or slerped, for quaternion rotation) between keyframes, with optional easing curves per-keyframe.

```
Skeletal animation keyframes (bone rotation over time):

Time:     0.0s     0.2s     0.5s     0.8s     1.0s
          |--------|--------|--------|--------|
Angle:    0        45       90       45       0
          ↑        ↑        ↑        ↑        ↑
       keyframe keyframe keyframe keyframe keyframe

Between keyframes, the engine lerps:
  At t=0.1s:  lerp(0, 45, 0.1/0.2) = lerp(0, 45, 0.5) = 22.5 degrees
  At t=0.35s: lerp(45, 90, 0.15/0.3) = lerp(45, 90, 0.5) = 67.5 degrees
```

The easing functions from this document can be applied per-keyframe segment, allowing animators to control the feel of each transition independently.

### Procedural Animation Smoothing

Procedural animation (covered in the sprite animation doc) frequently uses lerp and smoothstep for smoothing. When a procedurally-positioned element needs to move to a new target, you use lerp smoothing rather than snapping. The `smoothDamp` function from the frame-rate independence section is the standard approach for procedural animation targets.

### UI Systems

Every polished UI system is built on tweening. Menus, tooltips, health bars, inventory grids, dialog boxes, notification banners -- all of these use tweened transitions for appearing, disappearing, repositioning, and resizing. The tween system you build for gameplay animation is the same one your UI system will use.

This is exactly the relationship between CSS transitions/animations and web UI libraries. React's `framer-motion`, Vue's `<Transition>`, Angular's animation module -- they all wrap CSS-level tweening into component-level APIs. In your game engine, your tween manager serves the same role.

### Camera Systems

Camera follow behavior almost always uses lerp smoothing (exponential decay toward the player position). Camera zoom transitions use standard time-based tweens with easing. Camera shake is a decaying oscillation -- a tween controlling the amplitude of a sine wave. Your camera system doc (08-camera-systems.md) describes offset-based camera positioning; adding lerp smoothing to that system makes the camera feel dramatically better with just a few lines of code.

### Particle Systems

Particles are short-lived entities whose properties (position, scale, opacity, color) change over their lifetime. These changes are typically defined as curves over the particle's normalized lifetime (0 to 1, just like `t`). The easing functions in this document are exactly the curves used to define how a particle fades, shrinks, or changes color.

```
Particle lifetime curves using easing:

Opacity:  easeInQuad(lifetimeT)        → starts visible, fades slowly then quickly
Scale:    easeOutElastic(lifetimeT)     → starts small, pops to full size with bounce
Color:    lerp(orange, red, lifetimeT)  → shifts from orange to red as particle ages
Speed:    easeOutQuad(lifetimeT)        → starts fast, decelerates (like a spark)
```

Each of these is just an easing function applied to a normalized lifetime value. The same easing functions you use for UI animation and gameplay tweens are reused directly in your particle system.

### Summary: Tweening Is Everywhere

```
Systems that depend on tweening/interpolation:

┌──────────────────────────────┬───────────────────────────────────┐
│ System                       │ How it uses tweening              │
├──────────────────────────────┼───────────────────────────────────┤
│ UI animations                │ Position, scale, opacity tweens   │
│ Screen transitions           │ Opacity tween on overlay quad     │
│ Camera follow                │ Lerp smoothing toward player      │
│ Camera zoom                  │ Time-based tween on zoom factor   │
│ Camera shake                 │ Decaying amplitude tween          │
│ Health/mana bars             │ Ease-out tween on displayed value │
│ Damage numbers               │ Parallel Y-position + opacity     │
│ Item pickups                 │ Scale pop with elastic easing     │
│ Character squash/stretch     │ Scale tweens on landing/jumping   │
│ Skeletal animation           │ Lerp between keyframe poses       │
│ Procedural animation         │ Smoothing of computed positions   │
│ Particle lifetime            │ Easing curves for particle props  │
│ Dialog text reveal           │ Linear tween on character count   │
│ Day/night cycle              │ Color lerp over game time         │
│ Music crossfade              │ Volume tweens on audio sources    │
│ Save/load indicators         │ Opacity pulse (yoyo tween)        │
│ Tile transitions             │ Lerp between tile appearances     │
│ Map scrolling                │ Ease-in-out position tween        │
└──────────────────────────────┴───────────────────────────────────┘
```

Tweening is one of those foundational techniques where a small amount of code unlocks a disproportionate amount of polish across every system in your game. The tween manager you build will likely be one of the most-used systems in your entire engine, second only to the renderer itself.
