#include "Location.h"
#include "Camera.h"
#include "Vector2.h"
#include <memory>

Location::Location(const std::string& id, int tilesX, int tilesY, int tileSize, Texture* tileset)
  : id(id) {
  tilemap = std::make_unique<Tilemap>(tilesX, tilesY, tileSize, tileset);  
}

Location::~Location() = default;

void Location::onEnter() {
  std::cout << "Entering location: " << id << std::endl;
}

void Location::onExit() {
  std::cout << "Leaving location: " << id << std::endl;
}

void Location::render(Shader& tileShader, const Camera& camera) {
  tilemap->render(tileShader, camera);
}

void Location::renderDebug(Shader& debugShader, const Camera& camera) {
  tilemap->renderDebug(debugShader, camera);
}

// void Location::update() {}

void Location::addWarp(float x, float y, float w, float h,
                       const std::string& destId, const Vector2& spawnPos
) {
  warpZones.push_back({x, y, w, h, destId, spawnPos});
}

const WarpZone* Location::checkWarpCollisions(const Vector2& position) const {
  for (const auto& warp : warpZones) {
    if (warp.contains(position)) {
      return &warp;
    }
  }

  return nullptr;
}

const std::string& Location::getId() const { return id; }

int Location::getWorldWidth() const {
  return tilemap->getTileCountX();
}

int Location::getWorldHeight() const {
  return tilemap->getTileCountY();
}
