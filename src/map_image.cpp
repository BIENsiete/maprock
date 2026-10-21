#include "map_image.hpp"

#include <algorithm>
#include <cmath>

namespace maprock {

void buildMapImage(const World& world, const std::vector<std::array<uint8_t, 3>>& colors,
                   int maxSide, MapImage& out) {
  out = MapImage{};
  if (!world.hasBlocks) return;

  const int regionW = world.maxX - world.minX + 1;
  const int regionH = world.maxZ - world.minZ + 1;
  if (regionW <= 0 || regionH <= 0) return;

  int scale = 1;
  if (maxSide > 0) {
    int biggest = std::max(regionW, regionH);
    scale = (int)std::ceil((double)biggest / maxSide);
    if (scale < 1) scale = 1;
  }

  const int imgW = (regionW + scale - 1) / scale;
  const int imgH = (regionH + scale - 1) / scale;

  struct Top {
    int32_t y;
    uint16_t id;
    bool set = false;
  };
  std::vector<Top> top(static_cast<size_t>(imgW) * imgH);

  for (const auto& kv : world.blocks) {
    if (kv.second == 0) continue;
    int px = (kv.first.x - world.minX) / scale;
    int pz = (kv.first.z - world.minZ) / scale;
    if (px < 0 || px >= imgW || pz < 0 || pz >= imgH) continue;
    Top& t = top[static_cast<size_t>(pz) * imgW + px];
    if (!t.set || kv.first.y > t.y) {
      t.y = kv.first.y;
      t.id = kv.second;
      t.set = true;
    }
  }

  // Height shading range.
  int32_t hMin = world.minY, hMax = std::max(world.minY, world.maxY);

  out.w = imgW;
  out.h = imgH;
  out.offsetX = world.minX;
  out.offsetZ = world.minZ;
  out.scale = scale;
  out.rgba.resize(static_cast<size_t>(imgW) * imgH * 4);

  for (int pz = 0; pz < imgH; ++pz) {
    for (int px = 0; px < imgW; ++px) {
      const Top& t = top[static_cast<size_t>(pz) * imgW + px];
      size_t o = (static_cast<size_t>(pz) * imgW + px) * 4;
      if (!t.set) {
        out.rgba[o] = out.rgba[o + 1] = out.rgba[o + 2] = 0;
        out.rgba[o + 3] = 0;
        continue;
      }
      uint8_t r = 128, g = 128, b = 128;
      if (t.id < colors.size()) {
        r = colors[t.id][0];
        g = colors[t.id][1];
        b = colors[t.id][2];
      }
      // Simple relief shading.
      float f = 0.0f;
      if (hMax > hMin) f = (float)(t.y - hMin) / (float)(hMax - hMin);
      float shade = 0.85f + 0.3f * f;
      out.rgba[o] = (uint8_t)std::min(255.0f, r * shade);
      out.rgba[o + 1] = (uint8_t)std::min(255.0f, g * shade);
      out.rgba[o + 2] = (uint8_t)std::min(255.0f, b * shade);
      out.rgba[o + 3] = 255;
    }
  }
}

}  // namespace maprock
