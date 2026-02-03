#include "Texture.h"
#include "Common.h"
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_surface.h>

Texture::Texture(const std::string& filepath) {
    textureID = 0;
    width = 0;
    height = 0;
    channels = 0;

    // Load image using SDL_image
    SDL_Surface* loadedSurface = IMG_Load(filepath.c_str());
    if (!loadedSurface) {
      std::cerr << "Failed to load texture: " << filepath << std::endl;
      std::cerr << "SDL_image error: " << IMG_GetError() << std::endl;
      return;
    }

    SDL_Surface* surface = SDL_ConvertSurfaceFormat(loadedSurface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(loadedSurface);

    if (!surface) {
      std::cerr << "Failed to convert surface format: " << SDL_GetError() << std::endl;
      return;
    }

    width   = surface->w;
    height  = surface->h;

    // Generate OpenGL texture
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Upload texture data
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        surface->pixels
        );

    // Free SDL surface
    SDL_FreeSurface(surface);

    glBindTexture(GL_TEXTURE_2D, 0);

    std::cout << "Texture loaded: " << filepath << " (" << width << "x" << height << ")" << std::endl;
}

Texture::~Texture() {
  if (textureID != 0) {
    glDeleteTextures(1, &textureID);
  }
}

void Texture::bind(GLuint slot) {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, textureID);
}

void Texture::unbind() {
  glBindTexture(GL_TEXTURE_2D, 0);
}

int Texture::getWidth() const {
  return width;
}

int Texture::getHeight() const {
  return height;
}

GLuint Texture::getID() const {
  return textureID;
}
