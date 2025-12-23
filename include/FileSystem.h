#pragma once

#include <string>
#include <vector>

class FileSystem {
 public:
  static bool exists(const std::string& path);
  static std::vector<std::string> listFiles(const std::string& directory);
};
