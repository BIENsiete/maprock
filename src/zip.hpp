#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace maprock {

struct ZipEntry {
  std::string name;
  bool isDir = false;
  std::vector<uint8_t> data;
};

// Reads a ZIP archive (used for .mcworld files). Supports stored (0) and
// deflate (8) entries. Returns false on error.
bool readZip(const std::string& path, std::vector<ZipEntry>& out);

// Extracts a .mcworld archive's `db/` and `level.dat` into a fresh temporary
// directory and returns that directory. Returns "" on failure.
std::string extractMcworld(const std::string& mcworldPath);

}  // namespace maprock
