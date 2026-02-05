#include "Shader.h"
#include <filesystem>

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
  : programID(0),
    vertexPath(vertexPath),
    fragmentPath(fragmentPath)
{
  // Record initial modification times
  vertexLastModified = getFileModTime(vertexPath);
  fragmentLastModified = getFileModTime(fragmentPath);

  // Read shader source codes
  std::string vertexSource = readFile(vertexPath);
  std::string fragmentSource = readFile(fragmentPath);

  if (vertexSource.empty() || fragmentSource.empty()) {
    std::cerr << "Failed to read shader files!" << std::endl;
    return;
  }

  // Compile shaders
  GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
  GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

  if (vertexShader == 0 || fragmentShader == 0) {
    return;
  }

  // Link Program
  programID = glCreateProgram();
  glAttachShader(programID, vertexShader);
  glAttachShader(programID, fragmentShader);
  glLinkProgram(programID);

  // Check for linking errors
  GLint success;
  glGetProgramiv(programID, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(programID, 512, nullptr, infoLog);
    std::cerr << "Shader program linking failed:\n" << infoLog << std::endl;
  }

  // Clean up individual shaders (they're linked now)
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  std::cout << "Shader compiled successfully!" << std::endl;
}

Shader::~Shader() {
  if (programID != 0) {
    glDeleteProgram(programID);
  }
}

std::string Shader::readFile(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filepath << std::endl;
    return "";
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  return buffer.str();
}

GLuint Shader::compileShader(const std::string& source, GLenum type) {
  GLuint shader = glCreateShader(type);
  const char* src = source.c_str();
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  // Check for compilation errors
  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(shader, 512, nullptr, infoLog);
    std::cerr << "Shader compilation failed ("
              << (type == GL_VERTEX_SHADER ? "vertex" : "shader")
              << ")\n" << infoLog << std::endl;

    return 0;
  }

  return shader;
}

void Shader::use() {
  glUseProgram(programID);
}

void Shader::unuse() {
  glUseProgram(0);
}

GLuint Shader::getID() const {
  return programID;
}

void Shader::setInt(const std::string& name, int value) {
  glUniform1i(glGetUniformLocation(programID, name.c_str()), value);
}


void Shader::setFloat(const std::string& name, float value) {
  glUniform1f(glGetUniformLocation(programID, name.c_str()), value);
}

void Shader::setVec2(const std::string& name, float x, float y) {
  glUniform2f(glGetUniformLocation(programID, name.c_str()), x, y);
}

// Overload for new model using glm::vec2
void Shader::setVec2(const std::string& name, const glm::vec2& value) {
  glUniform2fv(glGetUniformLocation(programID, name.c_str()), 1, &value[0]);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) {
  glUniform3f(glGetUniformLocation(programID, name.c_str()), x, y, z);
}

void Shader::setVec4(const std::string& name, float x, float y, float z, float w) {
  glUniform4f(glGetUniformLocation(programID, name.c_str()), x, y, z, w);
}

void Shader::setMat4(const std::string& name, const glm::mat4& matrix) {
  glUniformMatrix4fv(glGetUniformLocation(programID, name.c_str()), 1, GL_FALSE, glm::value_ptr(matrix));
}

namespace fs = std::filesystem;

fs::file_time_type Shader::getFileModTime(const std::string& path) const {
  try {
    return fs::last_write_time(path);
  } catch (const fs::filesystem_error& e) {
    std::cerr << "Could not get mod time for: " << path << std::endl;
    return fs::file_time_type::min();
  }
}

bool Shader::hasFileChanged() const {
  auto currentVertexTime = getFileModTime(vertexPath);
  auto currentFragmentTime = getFileModTime(fragmentPath);

  return currentVertexTime != vertexLastModified ||
         currentFragmentTime != fragmentLastModified;
}

bool Shader::reload() {
  std::cout << "Reloading shader: " << vertexPath << ", " << fragmentPath << std::endl;

  std::string vertexSource = readFile(vertexPath);
  std::string fragmentSource = readFile(fragmentPath);

  if (vertexSource.empty() || fragmentSource.empty()) {
    std::cerr << "Hot-reload failed: Could not read shader files" << std::endl;
    return false;
  }

  GLuint newVertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
  GLuint newFragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

  if (newVertexShader == 0 || newFragmentShader == 0) {
    std::cerr << "Hot-reload failed: Shader compilation error" << std::endl;
    if (newVertexShader != 0) glDeleteShader(newVertexShader);
    if (newFragmentShader != 0) glDeleteShader(newFragmentShader);
    return false;
  }

  GLuint newProgram = glCreateProgram();
  glAttachShader(newProgram, newVertexShader);
  glAttachShader(newProgram, newFragmentShader);
  glLinkProgram(newProgram);

  GLint success;
  glGetProgramiv(newProgram, GL_LINK_STATUS, &success);

  glDeleteShader(newVertexShader);
  glDeleteShader(newFragmentShader);

  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(newProgram, 512, nullptr, infoLog);
    std::cerr << "Hot-reload failed: Linking error\n" << infoLog << std::endl;
    glDeleteProgram(newProgram);
    return false;
  }

  if (programID != 0) {
    glDeleteProgram(programID);
  }
  programID = newProgram;

  vertexLastModified = getFileModTime(vertexPath);
  fragmentLastModified = getFileModTime(fragmentPath);

  std::cout << "Shader hot-reloaded successfully!" << std::endl;
  return true;
}

bool Shader::checkReload() {
  if (hasFileChanged()) {
    return reload();
  }
  return false;
}
