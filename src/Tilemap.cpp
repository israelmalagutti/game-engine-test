#include "Tilemap.h"
#include "glad/gl.h"
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>

enum Tile {
  GRASS = 0,
  DIRT  = 1,
  WATER = 2,
};

Tilemap::Tilemap(int width, int height, int tileSize, Texture* tileset)
  : width(width),
    height(height),
    tileSize(tileSize),
    tileset(tileset),
    VAO(0),
    VBO(0)
{
  this->tilesPerRow = tileset->getWidth() / tileSize;

  // Allocate tile data
  tiles.resize(width * height, 0);
  
  // Set OpenGL Buffers
  setupMesh();
}

Tilemap::~Tilemap() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
}

void Tilemap::render(Shader& shader, const Camera& camera) {
  shader.use();

  // Projection & view
  glm::mat4 projection = glm::ortho(
      0.0f, static_cast<float>(camera.getViewportWidth()),
      static_cast<float>(camera.getViewportHeight()), 0.0f
  );
  glm::mat4 view = camera.getViewMatrix();

  shader.setMat4("projection", projection);
  shader.setMat4("view", view);
  shader.setInt("spriteTexture", 0);

  // UV size
  float uvWidth = static_cast<float>(tileSize) / tileset->getWidth();
  float uvHeight = static_cast<float>(tileSize) / tileset->getHeight();
  shader.setVec2("uvSize", glm::vec2(uvWidth, uvHeight));

  tileset->bind(0);
  glBindVertexArray(VAO);  

  for (int i = 0; i < width * height; i++) {
    int tileID = tiles[i];
    if (tileID < 0) continue;

    // Derive grid coords
    int x = i % width;
    int y = i / width;

    // Model matrix
    float worldX = x * tileSize;
    float worldY = y * tileSize;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(worldX, worldY, 0.0f));
    model = glm::scale(model, glm::vec3(tileSize, tileSize, 1.0f));
    shader.setMat4("model", model);

    // UV offset
    int tileCol = tileID % tilesPerRow;
    int tileRow = tileID / tilesPerRow;
    float uvX = tileCol * uvWidth;
    float uvY = tileRow * uvHeight;
    shader.setVec2("uvOffset", uvX, uvY);

    glDrawArrays(GL_TRIANGLES, 0, 6);
  }
}

void Tilemap::setupMesh() {
  std::array<glm::vec4, 6> vertices = {
    // pos      // tex
    glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), // top-left
    glm::vec4(1.0f, 0.0f, 1.0f, 0.0f), // bottom-right
    glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), // bottom-left

    glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), // top-left
    glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), // top-right
    glm::vec4(1.0f, 0.0f, 1.0f, 0.0f), // bottom-right
  };

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec4), vertices.data(), GL_STATIC_DRAW);

  // Position attr;
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
  glEnableVertexAttribArray(0);

  // Texture coordinate
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

bool Tilemap::isSolid(int tileX, int tileY) const {
  int tileID = getTile(tileX, tileY);
  return tileID != WATER;
}

bool Tilemap::isWalkable(int x, int y) const {
  int tileID = getTile(x, y);
  return tileID == GRASS || tileID == DIRT;
}

int Tilemap::getTile(int x, int y) const {
  return tiles[y * width + x];
}

void Tilemap::setTile(int x, int y, int tileID) {
  int tile = getTile(x, y);
  tiles[tile] = tileID;
}
