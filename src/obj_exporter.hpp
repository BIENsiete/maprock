#pragma once

#include <string>

#include "mesher.hpp"
#include "texture_atlas.hpp"

namespace maprock {

// Writes Wavefront OBJ + MTL + the atlas PNG next to `basePath`.
// Produces <basePath>.obj, <basePath>.mtl and <basePath>.png.
bool exportObj(const std::string& basePath, const Mesh& mesh, const TextureAtlas& atlas);

}  // namespace maprock
