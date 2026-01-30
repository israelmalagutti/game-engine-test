#pragma once

#include <iostream>

struct Vector2 {
  float x = 0.0f;
  float y = 0.0f;

  // Default constructor
  Vector2() : x(0.0f), y(0.0f) {}

  // Parameterized constructor
  Vector2(float x, float y) : x(x), y(y) {}

  // Methods defined inline (in the header) for simples structs
  Vector2 operator+(const Vector2& other) const {
    return Vector2(x + other.x, y + other.y);
  }

  Vector2 operator*(float scalar) const {
    return Vector2(x * scalar, y * scalar);
  }

  void print() const {
    std::cout << "(" << x << ", " << y << ")";
  }
};
