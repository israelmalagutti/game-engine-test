#pragma once

#include "Common.h"

class Texture {
  private:
    GLuint textureID;
    int width;
    int height;

    int channels;

  public:
    Texture(const std::string& filepath);
    ~Texture();

    void bind(GLuint slot = 0);
    void unbind();

    // Getters
    int getWidth() const;
    int getHeight() const;
    GLuint getID() const;
};
