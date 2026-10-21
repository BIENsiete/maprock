#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace maprock {

enum class Shape {
  None,          // not rendered (air / unknown)
  Cube,          // full opaque cube
  Translucent,   // full cube, faces adjacent are not culled (glass, leaves, ...)
  Cross,         // two crossed quads (flowers, saplings, grass, ...)
  Water,         // water surface (top face, translucent)
  Lava,          // lava surface (top face, translucent)
};

// Face texture names as resolved from blocks.json. `side` applies to all four
// horizontal faces.
struct BlockDef {
  bool known = false;
  bool translucent = false;
  std::string texUp, texDown, texSide;
};

// Resolves Bedrock block identifiers to renderable faces using the resource
// pack's `blocks.json` and `textures/terrain_texture.json`.
class BlockRegistry {
 public:
  // Loads the resource pack. Returns false (and switches to the built-in
  // "missing" fallback) if the files cannot be found.
  bool load(const std::string& resourcePackDir);

  bool hasResourcePack() const { return hasPack_; }
  const std::string& resourcePackDir() const { return dir_; }

  // Resolves a block (short name, no namespace) to a shape and three relative
  // texture paths (up/down/side). Returns false if the block is not rendered.
  bool resolve(const std::string& shortName,
               const std::vector<std::pair<std::string, std::string>>& props,
               Shape& shape, std::string& texUp, std::string& texDown,
               std::string& texSide) const;

  // Returns the absolute file path (with extension) for a relative texture
  // path, or empty if the file does not exist.
  std::string textureFile(const std::string& relPath) const;

 private:
  struct Variant {
    std::string path;
    std::string overlay;  // hex "#rrggbb" or empty
  };

  bool loadBlocksJson(const std::string& path);
  bool loadTerrainTexture(const std::string& path);

  // Resolves a Bedrock texture name (+props) to a concrete relative file path.
  std::string resolveTexture(const std::string& name,
                             const std::vector<std::pair<std::string, std::string>>& props,
                             std::string* overlayOut) const;

  std::string dir_;
  bool hasPack_ = false;

  std::unordered_map<std::string, BlockDef> blocks_;      // short name -> def
  std::unordered_map<std::string, std::vector<Variant>> textures_;  // texture name -> variants

  static int colorVariantIndex(const std::string& colorValue);
  static Shape classifyShape(const std::string& shortName, const BlockDef& def);
};

}  // namespace maprock
