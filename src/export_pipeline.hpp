#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "block_registry.hpp"
#include "mesher.hpp"
#include "texture_atlas.hpp"
#include "world.hpp"

namespace maprock {

struct ExportOptions {
  std::string worldPath;
  std::string resourcePack;   // may be empty -> built-in fallback textures
  std::string outputBase;
  std::string format = "obj";  // "obj" or "gltf"
  bool whole = true;
  int minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0;
  std::function<void(float, const std::string&)> progress;
};

// A loaded world plus everything needed to render/export it. The GUI prepares
// this once and re-meshes different selections without reloading.
struct Prepared {
  World world;
  BlockRegistry registry;
  std::vector<RenderBlock> render;                  // indexed by palette id
  std::vector<std::array<uint8_t, 3>> colors;       // palette id -> RGB
  TextureAtlas atlas;
};

// Loads a world (whole or a region), the resource pack, render info, the atlas
// and per-block map colors. Returns an error string, or "" on success.
std::string prepare(Prepared& out, const std::string& worldPath,
                    const std::string& resourcePack, bool whole, int minX, int minY,
                    int minZ, int maxX, int maxY, int maxZ,
                    std::function<void(float, const std::string&)> progress);

// Meshes (optionally clipped to `region`) and writes the model files. Returns
// an error string, or "" on success.
std::string exportPrepared(const Prepared& p, const std::string& outputBase,
                           const std::string& format, const Region* region);

// One-shot helper used by the CLI.
std::string runExport(const ExportOptions& opts);

}  // namespace maprock
