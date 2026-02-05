#pragma once

#include "Vector2.h"

struct WarpZone {
  float x, y;
  float width, height;

  // Destination
  std::string destinationId;
  Vector2 spawnPosition;

  bool contains(const Vector2& point) const {
    return point.x >= x && point.x <= x + width &&
           point.y >= y && point.y <= y + height;
  }
};
