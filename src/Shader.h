#pragma once

#include "Common.h"
#include <filesystem>

class Shader {
  private:
    GLuint programID;

    // Store path for hot-reload
    std::string vertexPath;
    std::string fragmentPath;

    // Trac last modification times
    std::filesystem::file_time_type vertexLastModified;
    std::filesystem::file_time_type fragmentLastModified;

    GLuint compileShader(const std::string& source, GLenum type);
    std::string readFile(const std::string& filepath);

    // Helper to get file modification time
    std::filesystem::file_time_type getFileModTime(const std::string& path) const;

public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    void use();
    void unuse();

    GLuint getID() const;

    // Hot-reload methods
    bool reload();
    bool checkReload();
    bool hasFileChanged() const;

    // Uniform setters
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec2(const std::string& name, float x, float y);
    void setVec2(const std::string& name, const glm::vec2& value);
    void setVec3(const std::string& name, float x, float y, float z);
    void setVec4(const std::string& name, float x, float y, float z, float w);
    void setMat4(const std::string& name, const glm::mat4& matrix);
};
