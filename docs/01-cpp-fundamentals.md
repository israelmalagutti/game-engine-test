# C++ Fundamentals in This Project

This document covers the C++ language features and patterns used throughout the game engine.

## Table of Contents
- [Classes and Objects](#classes-and-objects)
- [Inheritance and Polymorphism](#inheritance-and-polymorphism)
- [Smart Pointers](#smart-pointers)
- [References vs Pointers](#references-vs-pointers)
- [Forward Declarations](#forward-declarations)
- [Header Guards](#header-guards)

---

## Classes and Objects

Classes encapsulate data (member variables) and behavior (member functions) into a single unit.

### Structure

```Entity.h
class Entity {
protected:           // Accessible by derived classes
    std::string name;
    Vector2 position;
    bool isActive;

public:              // Accessible by anyone
    Entity(const std::string& name, float x, float y);  // Constructor
    virtual ~Entity() = default;                         // Virtual destructor

    // Member functions
    void printInfo() const;  // const = doesn't modify object state

    // Getters (accessors)
    std::string getName() const;

    // Setters (mutators)
    void setPosition(const Vector2& newPosition);
};
```

### Access Specifiers

| Specifier | Class | Derived Class | Outside |
|-----------|-------|---------------|---------|
| `public` | Yes | Yes | Yes |
| `protected` | Yes | Yes | No |
| `private` | Yes | No | No |

### Constructor Initialization

```Entity.cpp
// Assignment in body (works but less efficient)
Entity::Entity(const std::string& name, float x, float y) {
    this->name = name;
    this->position = Vector2(x, y);
    this->isActive = true;
}
```

```Entity.cpp
// Alternative: initializer list (preferred, more efficient)
Entity::Entity(const std::string& name, float x, float y)
    : name(name), position(x, y), isActive(true) {}
```

The initializer list directly constructs members rather than default-constructing then assigning.

---

## Inheritance and Polymorphism

Inheritance allows classes to derive from a base class, inheriting its interface and implementation.

### Basic Inheritance

```Entity.h
class Entity {
public:
    virtual void update(float deltaTime);
    virtual void render(Shader& shader, int screenWidth, int screenHeight);

    ...
};
```

```Player.h
class Player : public Entity {
public:
    void update(float deltaTime) override;  // override = compiler checks signature
    void render(Shader& shader, int screenWidth, int screenHeight) override;

    ...
};
```

### Virtual Functions

`virtual` enables runtime polymorphism - the correct function is called based on the actual object type, not the pointer/reference type.

```cpp
Entity* entity = new Player(...);
entity->update(dt);  // Calls Player::update, not Entity::update
```

### The `override` Keyword

`override` (C++11) tells the compiler this function must override a base class virtual function. If signatures don't match, you get a compile error instead of silent bugs.

```cpp
// Without override - compiles but doesn't override (different signature)
void render(Shader& shader, int width, int height);  // Hides base method!

// With override - compile error if signature doesn't match
void render(Shader& shader, int width, int height) override;  // Safe
```

### Virtual Destructor

Always make base class destructors virtual when using polymorphism:

```Entity.h
class Entity {
public:
    virtual ~Entity() = default;  // Ensures derived destructors are called

    ...
};
```

Without this, `delete basePtr` won't call the derived destructor, causing memory leaks.

---

## Smart Pointers

Smart pointers (C++11) automatically manage memory, preventing leaks and dangling pointers.

### std::unique_ptr

Exclusive ownership - only one unique_ptr can own an object at a time.

```Game.h
#include <memory>

class Game {
private:
    std::unique_ptr<Window> window;
    std::unique_ptr<Player> player;
    std::vector<std::unique_ptr<Enemy>> enemies;

    ...
};
```

```Game.cpp
Game::Game() {
    // Creating unique_ptr
    window = std::make_unique<Window>("Game", 800, 600);

    // Transferring ownership
    enemies.push_back(std::move(enemy));  // enemy is now nullptr

    // Getting raw pointer (for APIs that need it)
    player = std::make_unique<Player>(400.0f, 300.0f, playerTexture.get());

    ...
}
```

### Key Operations

| Operation | Description |
|-----------|-------------|
| `std::make_unique<T>(args)` | Create a unique_ptr (preferred) |
| `ptr.get()` | Get raw pointer without releasing ownership |
| `std::move(ptr)` | Transfer ownership to another unique_ptr |
| `ptr.reset()` | Delete owned object and set to nullptr |
| `ptr.release()` | Return raw pointer and release ownership |

### Why Not Raw Pointers?

```cpp
// Raw pointer - must remember to delete, exception-unsafe
Window* window = new Window(...);
// ... if exception thrown here, memory leaks
delete window;

// unique_ptr - automatic cleanup, exception-safe
auto window = std::make_unique<Window>(...);
// ... if exception thrown, destructor still runs
// No delete needed
```

---

## References vs Pointers

Both provide indirection, but with different semantics.

### References

```cpp
void Sprite::setPosition(const Vector2& pos) {  // const reference
  position = pos;
}

// Must be initialized, cannot be null, cannot be reseated
Vector2& ref = someVector;  // ref always refers to someVector
```

### Pointers

```Sprite.h
class Sprite {
private:
    Texture* texture;  // Can be null, can point to different objects

    ...
};
```

```Sprite.cpp
Sprite::Sprite(Texture* texture) {
    this->texture = texture;  // Store pointer
    // Use -> to access members
    this->size = Vector2(texture->getWidth(), texture->getHeight());

    ...
}
```

### When to Use Which

| Use Case | Recommendation |
|----------|----------------|
| Optional parameter (can be null) | Pointer |
| Non-owning reference to existing object | Pointer or reference |
| Passing large objects efficiently | `const T&` |
| Output parameter | Pointer (or reference) |
| Ownership transfer | `std::unique_ptr` |
| Storing non-owning handle | Raw pointer |

In this project, `Texture*` is used because:
1. Textures are owned by `Game` (via `unique_ptr`)
2. Sprites just reference them (non-owning)
3. The pointer might be passed from `.get()` on a unique_ptr

---

## Forward Declarations

Declare a class exists without including its full definition. Reduces compilation dependencies.

```Entity.h
#pragma once

#include "Common.h"
#include "Vector2.h"

class Shader;  // Forward declaration - no #include "Shader.h" needed

class Entity {
public:
    virtual void render(Shader& shader, int w, int h);  // Only need declaration

    ...
};
```

### When You Can Forward Declare

- Pointers to the type: `Shader*`
- References to the type: `Shader&`
- Return types (in declarations)
- Parameter types (in declarations)

### When You Need Full Include

- Creating objects of the type
- Accessing members
- Inheriting from the type
- Using `sizeof(T)`

---

## Header Guards

Prevent multiple inclusion of the same header file.

### #pragma once (Modern)

```cpp
#pragma once  // Compiler-specific but universally supported

class Entity { ... };
```

### Include Guards (Traditional)

```cpp
#ifndef ENTITY_H
#define ENTITY_H

class Entity { ... };

#endif
```

### Why Needed?

Without guards, including the same header twice causes redefinition errors:

```cpp
// Game.h includes Entity.h
// Player.h includes Entity.h
// Game.cpp includes both -> Entity defined twice!
```

With guards, the second inclusion is skipped entirely.

---

## The `const` Keyword

### Const Member Functions

```Entity.h
class Entity {
public:
    std::string getName() const;  // Promises not to modify object
    Vector2 getPosition() const;
    bool getIsActive() const;

    ...
};
```

Const functions can be called on const objects/references:

```cpp
void printEntity(const Entity& e) {
  std::cout << e.getName();  // OK - getName is const
  e.setActive(false);        // ERROR - setActive is not const
}
```

### Const References

```cpp
void setPosition(const Vector2& newPosition);  // Won't modify argument
```

Allows passing temporaries and prevents accidental modification.

---

## The `this` Pointer

`this` is an implicit pointer to the current object instance.

```Entity.cpp
Entity::Entity(const std::string& name, float x, float y) {
    this->name = name;  // Disambiguate member from parameter
    this->position = Vector2(x, y);
}
```

When names don't conflict, `this->` is optional:

```Entity.cpp
void Entity::setActive(bool active) {
    isActive = active;  // Equivalent to: this->isActive = active;
}
```
