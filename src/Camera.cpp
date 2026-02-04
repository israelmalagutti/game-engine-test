#include "Camera.h"
#include "Vector2.h"
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>

Camera::Camera(int viewportWidth, const int viewportHeight) {
  this->viewportWidth =  viewportWidth;
  this->viewportHeight = viewportHeight;

  position = Vector2(0, 0);

  // Default until call setWorldBoundires. Setting min/max to allow camera to go anywhere initially
  worldMin = Vector2(0.0f, 0.0f);
  worldMax = Vector2(0.0f, 0.0f);
}

float Camera::clamp(float value, float min, float max) {
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

void Camera::centerOn(const Vector2& target) {
  position = target;

  // Calculate calid range for camera center
  float halfWidth = viewportWidth / 2.0f;
  float halfHeight = viewportHeight / 2.0f;

  float minX = worldMin.x + halfWidth;
  float maxX = worldMax.x - halfWidth;
  float minY = worldMin.y + halfHeight;
  float maxY = worldMax.y - halfHeight;

  // Clamp camera position
  position.x = clamp(position.x, minX, maxX);
  position.y = clamp(position.y, minY, maxY);

  // World narrower than viewport - center horizontally
  if (worldMax.x - worldMin.x < viewportWidth) {
    position.x = (worldMax.x + worldMin.x ) / 2.0f;
  }
  
  // World narrower than viewport - center vertically
  if (worldMax.y - worldMin.y < viewportHeight) {
    position.y = (worldMax.y + worldMin.y) / 2.0f;
  }
}

Vector2 Camera::worldToScreen(const Vector2& worldPos) const {
  Vector2 screenPos;

  screenPos.x = worldPos.x - position.x + (viewportWidth / 2.0f);
  screenPos.y = worldPos.y - position.y + (viewportHeight / 2.0f);
  return screenPos;
}

void Camera::setPosition(const Vector2& pos) {
  position = pos;
}

void Camera::setViewportSize(int width, int height) {
  viewportWidth = width;
  viewportHeight = height;
}

void Camera::setWorldBounds(float minX, float minY, float maxX, float maxY) {
  worldMin = Vector2(minX, minY);
  worldMax = Vector2(maxX, maxY);
}

Vector2 Camera::getPosition() const {
  return position;
}

int Camera::getViewportWidth() const {
  return viewportWidth;
}

int Camera::getViewportHeight() const {
  return viewportHeight;
}

glm::mat4 Camera::getViewMatrix() const {
  float tx = -position.x + viewportWidth / 2.0f;
  float ty = -position.y + viewportHeight / 2.0f;
  return glm::translate(glm::mat4(1.0f), glm::vec3(tx, ty, 0.0f));
}
