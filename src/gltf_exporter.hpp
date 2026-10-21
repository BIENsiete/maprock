#pragma once

#include <string>

#include "mesher.hpp"
#include "texture_atlas.hpp"

namespace maprock {

// Writes a glTF 2.0 scene: <basePath>.gltf, <basePath>.bin and <basePath>.png.
bool exportGltf(const std::string& basePath, const Mesh& mesh, const TextureAtlas& atlas);

}  // namespace maprock
