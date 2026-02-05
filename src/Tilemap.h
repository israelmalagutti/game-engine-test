#pragma once

#include "Camera.h"
#include "Common.h"
#include "Shader.h"
#include "Texture.h"

class Tilemap {
  private:
  int width, height, tileSize;

  std::vector<int> tiles;    // Grounds layer
  // std::vector<int> objects; // Objects layer

  Texture* tileset;
  int tilesPerRow;

  GLuint VAO, VBO;

  void setupMesh();
  
  public:
    Tilemap(int width, int height, int tileSize, Texture* tileset);
    ~Tilemap();

    void render(Shader& shader, const  Camera& camera);
    
    // Collisions
    bool isWalkable(int tileX, int tileY) const;
    bool isSolid(int tileX, int tileY) const;

    // Getters
    int getTile(int x, int y) const;

    // Setters
    void setTile(int x, int y, int tileID);
};
