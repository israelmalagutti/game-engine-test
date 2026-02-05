#pragma once

#include "Camera.h"
#include "Common.h"
#include "Enemy.h"
#include "Shader.h"
#include "Texture.h"
#include "Tilemap.h"
#include "Vector2.h"
#include "WarpZone.h"

class Location {
  private:
    std::string id;
    std::unique_ptr<Tilemap> tilemap;

    std::vector<std::unique_ptr<Enemy>> enemies;
 
    std::vector<WarpZone>
      warpZones;
    std::vector<EnemySpawn> enemySpawns;

    Texture* enemyTexture;

  public:
    Location(const std::string& id, int tilesX, int tilesY, int tileSize, Texture* tileset);
    ~Location();

    // Lifecycle
    void onEnter();
    void onExit();

    // Game loop
    void render(Shader& tileShader, const Camera& camera);
    void renderDebug(Shader& debugShader, const Camera& camera);
    // void update();

    // Warp zones
    void addWarp(float x, float y, float w, float h,
                 const std::string& destId, const Vector2& spawnPos);
    const WarpZone* checkWarpCollisions(const Vector2& position) const;

    // Enemies
    void removeDeadEnemies();
    void addEnemy(); //

    // Getters
    const std::string& getId() const;
    int getWorldWidth() const;
    int getWorldHeight() const;
};
