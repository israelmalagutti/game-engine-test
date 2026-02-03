#include "Sprite.h"
#include "Common.h"

Sprite::Sprite(Texture* texture) {
  this->texture = texture;
  this->position = Vector2(0, 0);
  this->size = Vector2(texture->getWidth(), texture->getHeight());
  this->rotation = 0.0f;

  VAO = 0;
  VBO = 0;
  EBO = 0;

  setupMesh();
}

Sprite::~Sprite() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
}

void Sprite::setupMesh() {
  float vertices[] = {
    // pos      // tex
    0.0f, 1.0f, 0.0f, 1.0f, // top-left
    1.0f, 0.0f, 1.0f, 0.0f, // bottom-right
    0.0f, 0.0f, 0.0f, 0.0f, // bottom-left
    //
    0.0f, 1.0f, 0.0f, 1.0f, // top-left
    1.0f, 1.0f, 1.0f, 1.0f, // top-right
    1.0f, 0.0f, 1.0f, 0.0f, // bottom-right
  };

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  
  // Position attr
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // Texture coordinate attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void Sprite::draw(Shader& shader, int screenWidth, int screenHeight) {
  shader.use();

  // Create othographic projection matrix (2D)
  // This maps pixel coordinates to OpenGL's -1 to 1 range
  float projection[16] = {
    2.0f / screenWidth, 0.0f, 0.0f, 0.0f,
    0.0f, -2.0f / screenHeight, 0.0f, 0.0f,
    0.0f, 0.0f, -1.0f, 0.0f,
    -1.0f, 1.0f, 0.0f, 1.0f
  };

  // Create model matrix (position and scale)
  // For simplicity, weŕe doing this manually whithout a matrix library
  float model[16] = {
    size.x, 0.0f, 0.0f, 0.0f,
    0.0f, size.y, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    position.x, position.y, 0.0f, 1.0f
  };

  shader.setMat4("projection", projection);
  shader.setMat4("model", model);
  shader.setInt("spriteTexture", 0);

  texture->bind(0);

  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);

  texture->unbind();
}

void Sprite::setPosition(const Vector2& pos) {
  position = pos;
}

void Sprite::setSize(const Vector2& newSize) {
  size = newSize;
}

void Sprite::setRotation(float degrees) {
  rotation = degrees;
}

Vector2 Sprite::getPosition() const {
  return position;
}

Vector2 Sprite::getSize() const {
  return size;
}
