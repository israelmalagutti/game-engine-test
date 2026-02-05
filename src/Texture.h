#pragma once

#include "Common.h"
#include "FileWatcher.h"

class Texture {
  private:
    FileWatcher watcher;
    GLuint textureID;

    int width;
    int height;

    int channels;

  public:
    Texture(const std::string& filepath);
    ~Texture();

    void bind(GLuint slot = 0);
    void unbind();

    // Hot-reload
    bool reload();
    bool checkReload();

    // Getters
    int getWidth() const;
    int getHeight() const;
    GLuint getID() const;
};
