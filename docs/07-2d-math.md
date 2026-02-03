# 2D Math Fundamentals

This document covers the mathematical concepts used for 2D game development.

## Table of Contents
- [Vectors](#vectors)
- [Vector Operations](#vector-operations)
- [Matrices](#matrices)
- [Transformations](#transformations)
- [Coordinate Systems](#coordinate-systems)
- [Common Game Math](#common-game-math)

---

## Vectors

A 2D vector represents a point or direction in 2D space.

### Vector2 Class

```Vector2.h
struct Vector2 {
    float x, y;

    Vector2() : x(0), y(0) {}
    Vector2(float x, float y) : x(x), y(y) {}

    void print() const {
        std::cout << "(" << x << ", " << y << ")";
    }
};
```

### Two Interpretations

**As a Position:**
```
   y▲
    │    • (3, 2)
    │
    └──────▶ x
```
Point at coordinates (3, 2).

**As a Direction/Velocity:**
```
   y▲
    │    ──▶
    │   /
    └──────▶ x
```
Movement of 3 units right, 2 units up.

---

## Vector Operations

### Addition

Combines two vectors (e.g., position + velocity):

```cpp
Vector2 operator+(const Vector2& a, const Vector2& b) {
  return Vector2(a.x + b.x, a.y + b.y);
}

// Usage: new position after movement
position = position + velocity * deltaTime;
```

```
   a + b:
        b
       ▲
      /
     / ───▶ result
    /
   ▲
   │ a
   └────
```

### Subtraction

Finds the vector from one point to another:

```cpp
Vector2 operator-(const Vector2& a, const Vector2& b) {
  return Vector2(a.x - b.x, a.y - b.y);
}

// Usage: direction from enemy to player
Vector2 toPlayer = playerPos - enemyPos;
```

### Scalar Multiplication

Scales a vector (changes magnitude, not direction):

```cpp
Vector2 operator*(const Vector2& v, float scalar) {
  return Vector2(v.x * scalar, v.y * scalar);
}

// Usage: apply speed to direction
velocity = direction * speed;

// Usage: apply delta time
position = position + velocity * deltaTime;
```

### Magnitude (Length)

The length of a vector:

```cpp
float magnitude(const Vector2& v) {
  return std::sqrt(v.x * v.x + v.y * v.y);
}
```

Formula: `|v| = √(x² + y²)` (Pythagorean theorem)

### Normalization

Creates a unit vector (length 1) pointing in the same direction:

```cpp
Vector2 normalize(const Vector2& v) {
  float mag = magnitude(v);
  if (mag > 0) {
    return Vector2(v.x / mag, v.y / mag);
  }
  return v;
}
```

**Why normalize?**
- Direction without magnitude
- Consistent movement speed regardless of diagonal

```cpp
// Without normalization: diagonal is faster (√2 ≈ 1.41x)
Vector2 input(1, 1);  // Length ≈ 1.41

// With normalization: all directions same speed
Vector2 input = normalize(Vector2(1, 1));  // Length = 1
```

### Dot Product

Measures how parallel two vectors are:

```cpp
float dot(const Vector2& a, const Vector2& b) {
  return a.x * b.x + a.y * b.y;
}
```

Results:
- `> 0`: Same general direction (< 90°)
- `= 0`: Perpendicular (90°)
- `< 0`: Opposite directions (> 90°)

**Uses:**
- Check if facing target
- Calculate angle between vectors
- Projection

---

## Matrices

A 4x4 matrix is used for transformations in graphics (even 2D).

### Why 4x4 for 2D?

OpenGL uses 4x4 matrices everywhere for consistency with 3D. The 2D case just ignores the Z component.

### Matrix Layout (Column-Major)

OpenGL uses column-major order:

```
Mathematical:         Memory (column-major):
[ m0  m4  m8  m12 ]   float m[16] = {
[ m1  m5  m9  m13 ]     m0, m1, m2, m3,    // Column 0
[ m2  m6  m10 m14 ]     m4, m5, m6, m7,    // Column 1
[ m3  m7  m11 m15 ]     m8, m9, m10, m11,  // Column 2
                        m12, m13, m14, m15 // Column 3
                      };
```

---

## Transformations

### Identity Matrix

Does nothing (like multiplying by 1):

```cpp
float identity[16] = {
  1, 0, 0, 0,
  0, 1, 0, 0,
  0, 0, 1, 0,
  0, 0, 0, 1
};
```

### Translation Matrix

Moves objects:

```cpp
float translation[16] = {
  1, 0, 0, 0,
  0, 1, 0, 0,
  0, 0, 1, 0,
  tx, ty, 0, 1  // Translation in last column
};
```

### Scale Matrix

Resizes objects:

```cpp
float scale[16] = {
  sx, 0, 0, 0,   // Scale X
  0, sy, 0, 0,   // Scale Y
  0, 0, 1, 0,
  0, 0, 0, 1
};
```

### Combined Model Matrix

In `Sprite::draw()`, scale and translation are combined:

```Sprite.cpp
void Sprite::draw(Shader& shader, int screenWidth, int screenHeight) {
    ...

    float model[16] = {
        size.x, 0,      0, 0,   // Scale X in column 0
        0,      size.y, 0, 0,   // Scale Y in column 1
        0,      0,      1, 0,   // Z unchanged
        position.x, position.y, 0, 1  // Translation in column 3
    };

    ...
}
```

This transforms the sprite's local coordinates (0-1) to world coordinates.

### Rotation Matrix (2D)

Rotates around the Z axis:

```cpp
float cosA = cos(angle);
float sinA = sin(angle);

float rotation[16] = {
  cosA, sinA, 0, 0,
  -sinA, cosA, 0, 0,
  0, 0, 1, 0,
  0, 0, 0, 1
};
```

### Matrix Multiplication Order

Transformations are applied right-to-left:

```glsl
// In vertex shader
gl_Position = projection * model * vec4(aPos, 0.0, 1.0);

// Order of application:
// 1. aPos (local vertex position)
// 2. model (scale, rotate, translate to world)
// 3. projection (world to screen)
```

**Order matters!**

Scale then translate ≠ Translate then scale:

```
Scale(2) then Translate(10):
  Point at 1 → Scale → 2 → Translate → 12

Translate(10) then Scale(2):
  Point at 1 → Translate → 11 → Scale → 22
```

---

## Coordinate Systems

### OpenGL Clip Space

OpenGL's native coordinate system:
- X: -1 (left) to 1 (right)
- Y: -1 (bottom) to 1 (top)
- Z: -1 (near) to 1 (far)

```
    (-1,1)────────(1,1)
       │            │
       │   (0,0)    │
       │            │
    (-1,-1)───────(1,-1)
```

### Screen Space (Pixels)

What we want for 2D games:
- X: 0 (left) to width (right)
- Y: 0 (top) to height (bottom)  ← Note: Y flipped!

```
    (0,0)─────────(800,0)
       │            │
       │            │
       │            │
    (0,600)───────(800,600)
```

### Orthographic Projection

Maps screen coordinates to clip space:

```Sprite.cpp
void Sprite::draw(Shader& shader, int screenWidth, int screenHeight) {
    ...

    float projection[16] = {
        2.0f / screenWidth,  0.0f,                  0.0f, 0.0f,
        0.0f,               -2.0f / screenHeight,   0.0f, 0.0f,
        0.0f,                0.0f,                 -1.0f, 0.0f,
       -1.0f,                1.0f,                  0.0f, 1.0f
    };

    ...
}
```

**Breaking it down:**

| Element | Purpose |
|---------|---------|
| `2/width` | Scale X from [0,width] to [0,2] |
| `-2/height` | Scale Y from [0,height] to [0,-2] and flip |
| `-1, 1` | Translate to center at (-1,-1) to (1,1) |

**Example transformation:**
```
Input: (400, 300) on 800x600 screen

X: 400 * (2/800) - 1 = 1 - 1 = 0
Y: 300 * (-2/600) + 1 = -1 + 1 = 0

Output: (0, 0) = center of screen ✓
```

---

## Common Game Math

### Distance Between Points

```cpp
float distance(const Vector2& a, const Vector2& b) {
  float dx = b.x - a.x;
  float dy = b.y - a.y;
  return std::sqrt(dx * dx + dy * dy);
}

// Usage: check if enemy is in attack range
if (distance(enemy.pos, player.pos) < attackRange) {
  attack();
}
```

### Direction Toward Target

```cpp
Vector2 directionTo(const Vector2& from, const Vector2& to) {
  Vector2 diff = to - from;
  return normalize(diff);
}

// Usage: enemy follows player
Vector2 moveDir = directionTo(enemy.pos, player.pos);
enemy.pos = enemy.pos + moveDir * speed * deltaTime;
```

### Linear Interpolation (Lerp)

Smoothly blend between two values:

```cpp
float lerp(float a, float b, float t) {
  return a + (b - a) * t;  // t=0 gives a, t=1 gives b
}

Vector2 lerp(const Vector2& a, const Vector2& b, float t) {
  return Vector2(
    lerp(a.x, b.x, t),
    lerp(a.y, b.y, t)
  );
}

// Usage: smooth camera follow
camera.pos = lerp(camera.pos, player.pos, 0.1f);
```

### Clamping

Restrict a value to a range:

```cpp
float clamp(float value, float min, float max) {
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

// Usage: keep player in bounds
player.x = clamp(player.x, 0, screenWidth);
player.y = clamp(player.y, 0, screenHeight);
```

### Angle from Direction

```cpp
float angleFromDirection(const Vector2& dir) {
  return std::atan2(dir.y, dir.x);  // Returns radians
}

// Convert to degrees
float degrees = radians * 180.0f / M_PI;
```

### Direction from Angle

```cpp
Vector2 directionFromAngle(float radians) {
  return Vector2(std::cos(radians), std::sin(radians));
}
```

---

## Frame-Rate Independent Movement

### The Problem

```cpp
// Fixed movement - faster on faster computers!
position.x += 5;
```

At 60 FPS: moves 300 units/second
At 30 FPS: moves 150 units/second

### The Solution: Delta Time

```cpp
// Movement scaled by time elapsed
position.x += 5 * deltaTime;  // 5 units per second, always
```

### Velocity-Based Movement

```Player.cpp
void Player::update(float deltaTime) {
    position.x += velocity.x * deltaTime;
    position.y += velocity.y * deltaTime;

    // Apply friction
    velocity = velocity * (1.0f - friction * deltaTime);
}

void Player::move(const Vector2& direction) {
    velocity.x = direction.x * speed;
    velocity.y = direction.y * speed;
}
```

This separates:
- **Input**: Sets desired velocity
- **Update**: Applies velocity over time
