#include "FileSystem.h"
#include <filesystem>
#include <vector>
#include <string>

namespace fs = std::filesystem;

bool FileSystem::exists(const std::string& path) {
    return fs::exists(path);
}

std::vector<std::string> FileSystem::listFiles(const std::string& directory) {
    std::vector<std::string> files;
    try {
        for (const auto& entry : fs::directory_iterator(directory)) {
            files.push_back(entry.path().string());
        }
    } catch (const fs::filesystem_error& e) {
        // Handle directory access errors
    }
    return files;
}

