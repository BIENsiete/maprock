#include "texture_atlas.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

namespace maprock {

namespace {

std::string tintKey(const std::string& file, const Tint& t) {
  char buf[256];
  std::snprintf(buf, sizeof(buf), "%s|%d|%d|%d", file.c_str(),
                static_cast<int>(t.r * 255), static_cast<int>(t.g * 255),
                static_cast<int>(t.b * 255));
  return buf;
}

void fillMissing(Image& img) {
  img.w = img.h = 16;
  img.rgba.resize(16 * 16 * 4);
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      bool a = ((x / 4) + (y / 4)) % 2 == 0;
      uint8_t c = a ? 0xFF : 0x00;
      size_t i = (y * 16 + x) * 4;
      img.rgba[i + 0] = c;
      img.rgba[i + 1] = 0x00;
      img.rgba[i + 2] = c;
      img.rgba[i + 3] = 0xFF;
    }
  }
}

}  // namespace

bool loadImage(const std::string& file, Image& out) {
  int w = 0, h = 0, comp = 0;
  unsigned char* data = stbi_load(file.c_str(), &w, &h, &comp, 4);
  if (!data) return false;
  out.w = w;
  out.h = h;
  out.rgba.assign(data, data + size_t(w) * h * 4);
  stbi_image_free(data);
  return true;
}

bool resizeImage(const Image& src, int tw, int th, Image& dst) {
  if (src.w <= 0 || src.h <= 0 || tw <= 0 || th <= 0) return false;
  dst.w = tw;
  dst.h = th;
  dst.rgba.resize(size_t(tw) * th * 4);

  if (src.w == tw && src.h == th) {
    dst.rgba = src.rgba;
    return true;
  }

  // Box filter for downscaling, bilinear-ish for upscaling.
  const float sx = static_cast<float>(src.w) / tw;
  const float sy = static_cast<float>(src.h) / th;
  for (int y = 0; y < th; ++y) {
    for (int x = 0; x < tw; ++x) {
      float fx = (x + 0.5f) * sx - 0.5f;
      float fy = (y + 0.5f) * sy - 0.5f;
      int x0 = std::max(0, static_cast<int>(std::floor(fx)));
      int y0 = std::max(0, static_cast<int>(std::floor(fy)));
      int x1 = std::min(src.w - 1, x0 + static_cast<int>(std::ceil(sx)));
      int y1 = std::min(src.h - 1, y0 + static_cast<int>(std::ceil(sy)));
      float sr = 0, sg = 0, sb = 0, sa = 0;
      int cnt = 0;
      for (int yy = y0; yy <= y1; ++yy) {
        for (int xx = x0; xx <= x1; ++xx) {
          size_t i = (size_t(yy) * src.w + xx) * 4;
          sr += src.rgba[i];
          sg += src.rgba[i + 1];
          sb += src.rgba[i + 2];
          sa += src.rgba[i + 3];
          ++cnt;
        }
      }
      size_t o = (size_t(y) * tw + x) * 4;
      dst.rgba[o] = static_cast<uint8_t>(sr / cnt);
      dst.rgba[o + 1] = static_cast<uint8_t>(sg / cnt);
      dst.rgba[o + 2] = static_cast<uint8_t>(sb / cnt);
      dst.rgba[o + 3] = static_cast<uint8_t>(sa / cnt);
    }
  }
  return true;
}

bool averageColor(const Image& img, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (img.w <= 0 || img.h <= 0) return false;
  uint64_t sr = 0, sg = 0, sb = 0, cnt = 0;
  for (size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
    if (img.rgba[i + 3] < 128) continue;  // ignore transparent pixels
    sr += img.rgba[i];
    sg += img.rgba[i + 1];
    sb += img.rgba[i + 2];
    ++cnt;
  }
  if (cnt == 0) {
    r = g = b = 0;
    return false;
  }
  r = static_cast<uint8_t>(sr / cnt);
  g = static_cast<uint8_t>(sg / cnt);
  b = static_cast<uint8_t>(sb / cnt);
  return true;
}

int TextureAtlas::add(const std::string& file, const Tint& tint) {
  std::string key = file.empty() ? std::string("<missing>") : tintKey(file, tint);
  auto it = cache_.find(key);
  if (it != cache_.end()) return it->second;

  int id = static_cast<int>(entries_.size());
  cache_[key] = id;

  // Load the texture (empty path or failed load -> missing checkerboard).
  Image img;
  if (file.empty() || !loadImage(file, img)) fillMissing(img);

  Image tile;
  if (!resizeImage(img, kTile, kTile, tile)) fillMissing(tile);

  // Apply tint.
  if (tint.r != 1.0f || tint.g != 1.0f || tint.b != 1.0f) {
    for (size_t i = 0; i + 3 < tile.rgba.size(); i += 4) {
      tile.rgba[i] = static_cast<uint8_t>(std::min(255.0f, tile.rgba[i] * tint.r));
      tile.rgba[i + 1] = static_cast<uint8_t>(std::min(255.0f, tile.rgba[i + 1] * tint.g));
      tile.rgba[i + 2] = static_cast<uint8_t>(std::min(255.0f, tile.rgba[i + 2] * tint.b));
    }
  }

  entries_.push_back(Entry{});
  tiles_.push_back(std::move(tile));
  rebuild();
  return id;
}

void TextureAtlas::rebuild() {
  int count = static_cast<int>(entries_.size());
  int need = static_cast<int>(std::ceil(std::sqrt(std::max(1, count))));
  gridW_ = need;
  int side = gridW_ * kTile;
  atlas_.w = side;
  atlas_.h = side;
  atlas_.rgba.assign(size_t(side) * side * 4, 0);

  for (int i = 0; i < count; ++i) {
    Entry& e = entries_[i];
    e.x = (i % gridW_) * kTile;
    e.y = (i / gridW_) * kTile;
    const Image& t = tiles_[i];
    for (int y = 0; y < kTile; ++y) {
      size_t src = size_t(y) * kTile * 4;
      size_t dst = (size_t(e.y + y) * atlas_.w + e.x) * 4;
      std::memcpy(atlas_.rgba.data() + dst, t.rgba.data() + src, kTile * 4);
    }
  }
}

void TextureAtlas::uv(int id, float& u0, float& v0, float& u1, float& v1) const {
  if (id < 0 || id >= static_cast<int>(entries_.size()) || atlas_.w == 0) {
    u0 = v0 = 0;
    u1 = v1 = 1;
    return;
  }
  const Entry& e = entries_[id];
  const float W = static_cast<float>(atlas_.w);
  const float H = static_cast<float>(atlas_.h);
  // Inset by a small epsilon to avoid bleeding between tiles.
  const float ep = 0.02f;
  u0 = (e.x + ep) / W;
  v0 = (e.y + ep) / H;
  u1 = (e.x + kTile - ep) / W;
  v1 = (e.y + kTile - ep) / H;
}

bool TextureAtlas::averageOf(int id, uint8_t& r, uint8_t& g, uint8_t& b) const {
  if (id < 0 || id >= static_cast<int>(tiles_.size())) return false;
  return averageColor(tiles_[id], r, g, b);
}

bool TextureAtlas::writePng(const std::string& path) const {
  if (atlas_.w <= 0 || atlas_.h <= 0) return false;
  return stbi_write_png(path.c_str(), atlas_.w, atlas_.h, 4, atlas_.rgba.data(),
                        atlas_.w * 4) != 0;
}

}  // namespace maprock
