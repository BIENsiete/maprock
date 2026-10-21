#pragma once

#include <cstdint>
#include <vector>

#include "block_registry.hpp"
#include "texture_atlas.hpp"
#include "world.hpp"

namespace maprock {

// Per-palette-id render info computed by the export pipeline.
struct RenderBlock {
  Shape shape = Shape::None;
  bool opaque = true;
  int texUp = 0, texDown = 0, texSide = 0;
};

// Indexed triangle mesh.
struct Mesh {
  std::vector<float> positions;  // xyz
  std::vector<float> normals;    // xyz
  std::vector<float> uvs;        // uv
  std::vector<uint32_t> indices;
  bool empty() const { return positions.empty(); }
};

// Inclusive block-coordinate region used to clip the mesh.
struct Region {
  int minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0;
};

// Builds the mesh for a loaded world. If `region` is non-null, only blocks
// inside it are emitted and faces at its boundary are left open.
void buildMesh(const World& world, const std::vector<RenderBlock>& render,
               const TextureAtlas& atlas, const Region* region, Mesh& out);

}  // namespace maprock
