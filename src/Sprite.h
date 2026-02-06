#pragma once

#include "Camera.h"
#include "Common.h"
#include "Texture.h"
#include "Shader.h"
#include "Vector2.h"

class Sprite{
  private:
    GLuint VAO; // Vertex Array Object
    GLuint VBO; // Vertex Buffer Object
    GLuint EBO; // Element Buffer Object

    Texture* texture;
    Vector2 position;
    Vector2 uvOffset;
    Vector2 uvSize;

    Vector2 size;
    float rotation;

    void setupMesh();

  public:
    Sprite(Texture* texture);
    ~Sprite();

    void draw(Shader& shader, const Camera& camera);

    void setUVRegion(const Vector2& offset, const Vector2& size);
    void setUVRegionPixels(float x, float y, float width, float height);

    void setPosition(const Vector2& pos);
    void setSize(const Vector2& size);
    void setRotation(float degrees);

    Vector2 getPosition() const;
    Vector2 getSize() const;
};
