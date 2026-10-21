#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "world.hpp"

namespace maprock {

struct MapImage {
  int w = 0, h = 0;
  int offsetX = 0, offsetZ = 0;  // world block coordinate of pixel (0,0)
  int scale = 1;                 // blocks per pixel
  std::vector<uint8_t> rgba;     // RGBA8, top-down
};

// Builds a top-down "in-game map" style image. `colors` is indexed by palette
// id and holds 3 RGB bytes each. `maxSide` limits the largest image dimension.
void buildMapImage(const World& world, const std::vector<std::array<uint8_t, 3>>& colors,
                   int maxSide, MapImage& out);

}  // namespace maprock
