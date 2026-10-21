#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <leveldb/db.h>

namespace maprock {

struct BlockPos {
  int32_t x = 0, y = 0, z = 0;
  bool operator==(const BlockPos& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct BlockPosHash {
  size_t operator()(const BlockPos& p) const {
    uint64_t h = (uint64_t(uint32_t(p.x)) << 32) ^ uint32_t(p.y);
    h = h * 0x9e3779b97f4a7c15ULL ^ uint64_t(uint32_t(p.z));
    return size_t(h);
  }
};

// A block state as it appears in a subchunk palette entry. `name` keeps the
// "minecraft:" namespace, e.g. "minecraft:stone".
struct BlockState {
  std::string name;
  std::vector<std::pair<std::string, std::string>> props;  // sorted for dedup
};

// A loaded world region (sparse set of non-air blocks).
class World {
 public:
  World() {
    BlockState air;
    air.name = "minecraft:air";
    palette.push_back(air);
  }

  // palette[0] is always "minecraft:air".
  std::vector<BlockState> palette;
  std::unordered_map<BlockPos, uint16_t, BlockPosHash> blocks;

  bool hasBlocks = false;
  int32_t minX = 0, maxX = 0, minY = 0, maxY = 0, minZ = 0, maxZ = 0;

  // Returns the palette index at the given position, 0 (air) if absent.
  uint16_t blockAt(int32_t x, int32_t y, int32_t z) const;
  const BlockState& state(uint16_t id) const { return palette[id]; }
  // name without the "minecraft:" prefix.
  std::string shortName(uint16_t id) const;
  bool isAir(uint16_t id) const { return id == 0; }

  // Highest non-air block Y in [yMin, yMax] at column (x,z); yMin-1 if none.
  int32_t topBlockAt(int32_t x, int32_t z, int32_t yMin, int32_t yMax) const;

  // Interns a block state into the palette and returns its index.
  uint16_t internState(const BlockState& st);
  // Stores a block and updates the world bounds. Skipped for air (id 0).
  void setBlock(int32_t x, int32_t y, int32_t z, uint16_t id);

 private:
  std::unordered_map<std::string, uint16_t> stateIndex_;
};

struct LoadOptions {
  bool regionDefined = false;   // if false, load everything
  int32_t minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0;
  int dimension = 0;            // 0 = overworld, 1 = nether, 2 = end
};

// Opens a Bedrock LevelDB database. `createIfMissing` enables the write path
// (used by the synthetic-world generator). The returned DB must be deleted by
// the caller. Throws std::runtime_error on failure.
leveldb::DB* openBedrockDb(const std::string& path, bool createIfMissing);

// Loads a world. `worldPath` may be the world directory (containing "db") or
// the "db" directory itself. Throws std::runtime_error on failure.
void loadWorld(const std::string& worldPath, const LoadOptions& opts, World& out);

}  // namespace maprock
