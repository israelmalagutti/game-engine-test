#pragma once

#include <iostream>
#include <cmath>

struct Vector2 {
  float x = 0.0f;
  float y = 0.0f;

  // Constructors
  Vector2() : x(0.0f), y(0.0f) {}
  Vector2(float x, float y) : x(x), y(y) {}

  Vector2 operator+(const Vector2& other) const {
    return Vector2(x + other.x, y + other.y);
  }

  Vector2 operator-(const Vector2& other) const {
    return Vector2(x - other.x, y - other.y);
  }

  Vector2 operator*(float scalar) const {
    return Vector2(x * scalar, y * scalar);
  }

  Vector2 operator/(float scalar) const {
    return Vector2(x / scalar, y / scalar);
  }
  
  // Coumpound substraction  v1 -= v2
  Vector2& operator-=(const Vector2& other) {
    x -= other.x;
    y -= other.y;
    return *this;
  }

  // Coumpound multiply  v1 *= v2
  Vector2& operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    return *this;
  }

  bool operator==(const Vector2& other) const {
    return x == other.x && y == other.y;
  }

  // Coumpound Inequality  v1 != v2
  bool operator!=(const Vector2& other) const {
    return !(*this == other);
  }

  // Negation
  Vector2 operator-() const {
    return Vector2(-x , -y);
  }

  float length() const {
    return std::sqrt(x * x + y * y);
  }

  float lengthSquared() const {
    return x * x + y * y;
  }

  Vector2 normalized() const {
    float len = length();

    if (len == 0) return Vector2(0, 0);
    return Vector2(x / len, y / len);
  }

  float dot(const Vector2& other) const {
    return x * other.x + y * other.y;
  }

  float distanceTo(const Vector2& other) const {
    return (*this - other).length();
  }

  void print() const {
    std::cout << "(" << x << ", " << y << ")";
  }
};
