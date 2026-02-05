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
    VBO(0),
    debugVAO(0),
    debugVBO(0),
    debugLineCount(0)
{
  this->tilesPerRow = tileset->getWidth() / tileSize;

  // Allocate tile data
  tiles.resize(width * height, 0);

  // Set OpenGL Buffers
  setupMesh();
  setupDebugMesh();
}

Tilemap::~Tilemap() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteVertexArrays(1, &debugVAO);
  glDeleteBuffers(1, &debugVBO);
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

int Tilemap::getTileCountX() const { return width; }
int Tilemap::getTileCountY() const { return height; }
int Tilemap::getTileSize()   const { return tileSize; }

void Tilemap::setTile(int x, int y, int tileID) {
  int tile = getTile(x, y);
  tiles[tile] = tileID;
}

void Tilemap::setupDebugMesh() {
  std::vector<float> lineVertices;

  float mapWidth = width * tileSize;
  float mapHeight = height * tileSize;

  // Horizontal lines (height + 1 lines)
  for (int y = 0; y <= height; y++) {
    float yPos = y * tileSize;
    // Start point
    lineVertices.push_back(0.0f);
    lineVertices.push_back(yPos);
    // End point
    lineVertices.push_back(mapWidth);
    lineVertices.push_back(yPos);
  }

  // Vertical lines (width + 1 lines)
  for (int x = 0; x <= width; x++) {
    float xPos = x * tileSize;
    // Start point
    lineVertices.push_back(xPos);
    lineVertices.push_back(0.0f);
    // End point
    lineVertices.push_back(xPos);
    lineVertices.push_back(mapHeight);
  }

  debugLineCount = (height + 1 + width + 1) * 2;  // 2 vertices per line

  glGenVertexArrays(1, &debugVAO);
  glGenBuffers(1, &debugVBO);

  glBindVertexArray(debugVAO);
  glBindBuffer(GL_ARRAY_BUFFER, debugVBO);
  glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(float), lineVertices.data(), GL_STATIC_DRAW);

  // Position attribute (vec2)
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void Tilemap::renderDebug(Shader& debugShader, const Camera& camera) {
  debugShader.use();

  // Projection & view
  glm::mat4 projection = glm::ortho(
      0.0f, static_cast<float>(camera.getViewportWidth()),
      static_cast<float>(camera.getViewportHeight()), 0.0f
  );
  glm::mat4 view = camera.getViewMatrix();

  debugShader.setMat4("projection", projection);
  debugShader.setMat4("view", view);
  debugShader.setVec3("lineColor", 1.0f, 1.0f, 1.0f);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBindVertexArray(debugVAO);
  glDrawArrays(GL_LINES, 0, debugLineCount);
  glBindVertexArray(0);

  glDisable(GL_BLEND);
}
