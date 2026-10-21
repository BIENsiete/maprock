#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace maprock {

struct Tint {
  float r = 1.0f, g = 1.0f, b = 1.0f;
};

struct Image {
  int w = 0, h = 0;
  std::vector<uint8_t> rgba;  // 4 bytes/pixel, top-down
};

// Packs square block textures into a single atlas texture. Id 0 is always the
// built-in "missing" checkerboard.
class TextureAtlas {
 public:
  // Adds a texture file (PNG/TGA) to the atlas, applying `tint`. Returns an id.
  int add(const std::string& file, const Tint& tint);
  // UV rectangle (0..1) for a texture id.
  void uv(int id, float& u0, float& v0, float& u1, float& v1) const;

  int width() const { return atlas_.w; }
  int height() const { return atlas_.h; }
  int count() const { return static_cast<int>(entries_.size()); }
  const std::vector<uint8_t>& rgba() const { return atlas_.rgba; }

  // Average color of a packed tile (tint already applied).
  bool averageOf(int id, uint8_t& r, uint8_t& g, uint8_t& b) const;

  // Writes the atlas as a PNG file.
  bool writePng(const std::string& path) const;

 private:
  static constexpr int kTile = 16;
  struct Entry { int x = 0, y = 0; };
  std::vector<Entry> entries_;
  std::vector<Image> tiles_;  // packed 16x16 tile per id
  std::unordered_map<std::string, int> cache_;
  Image atlas_;
  int gridW_ = 0;
  void rebuild();
};

// Loads a PNG/TGA image into an RGBA8 Image.
bool loadImage(const std::string& file, Image& out);

// Resizes an image into a fixed tile size (box filter when shrinking).
bool resizeImage(const Image& src, int tw, int th, Image& dst);

// Average color of an image, used for the top-down map view.
bool averageColor(const Image& img, uint8_t& r, uint8_t& g, uint8_t& b);

}  // namespace maprock
