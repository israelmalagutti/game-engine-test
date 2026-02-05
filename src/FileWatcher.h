#pragma once

#include <filesystem>
#include "string"
#include <vector>

namespace fs = std::filesystem;

class FileWatcher {
  private: 
    std::vector<std::string> paths;
    std::vector<fs::file_time_type> lastModified;

    // Helper to get file modification time
    fs::file_time_type getModTime(const std::string& path) const {
      try {
        return fs::last_write_time(path);
      } catch (fs::filesystem_error& e) {
        return fs::file_time_type::min();
      }
    };

  public:
    // Watch single file
    FileWatcher(const std::string& path) {
      paths.push_back(path);
      lastModified.push_back(getModTime(path));
    };

    // Watch multiple files
    FileWatcher(const std::vector<std::string>& filePaths) {
      for (const auto& path : filePaths) {
        paths.push_back(path);
        lastModified.push_back(getModTime(path));
      }
    };

    bool hasChanged() const {
      for (size_t i = 0; i < paths.size(); i++) {
        if (getModTime(paths[i]) != lastModified[i]) {
          return true;
        }
      }
      return false;
    };

    void updateTimestamps() {
      for (size_t i = 0; i < paths.size(); i++) {
        getModTime(paths[i]) = lastModified[i];
      }
    }

    // Getters
    const std::vector<std::string>& getPaths() const {
      return paths;
    }
};
