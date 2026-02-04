#pragma once

#include "Common.h"
#include "Vector2.h"

class Camera {
  private:
    // Center of the view in world coordinates
    Vector2 position;

    // World boundries 
    Vector2 worldMin;
    Vector2 worldMax;

    // Visible area
    int viewportWidth;
    int viewportHeight;

  public:
    Camera(int viewportWidth, int viewportHeight);

    void setWorldBounds(float minX, float minY, float maxX, float maxY);
    void setViewportSize(int width, int height);
    void setPosition(const Vector2& pos);
    void centerOn(const Vector2& target);

    float clamp(float value, float x, float y);

    // Getters
    Vector2 getPosition()   const;
    int getViewportWidth()  const;
    int getViewportHeight() const;

    Vector2 worldToScreen(const Vector2& worldPos) const; // For rendering
    glm::mat4 getViewMatrix() const;
};
