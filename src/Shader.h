#pragma once

#include "Common.h"

class Shader {
  private:
    GLuint programID;

    GLuint compileShader(const std::string& source, GLenum type);
    std::string readFile(const std::string& filepath);

public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    void use();
    void unuse();

    GLuint getID() const;

    // Uniform setters
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec2(const std::string& name, float x, float y);
    void setVec3(const std::string& name, float x, float y, float z);
    void setVec4(const std::string& name, float x, float y, float z, float w);
    void setMat4(const std::string& name, const glm::mat4& matrix);
};
