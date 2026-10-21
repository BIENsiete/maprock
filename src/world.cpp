#include "world.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>

#include <leveldb/cache.h>
#include <leveldb/decompress_allocator.h>
#include <leveldb/env.h>
#include <leveldb/filter_policy.h>
#include <leveldb/options.h>
#include <leveldb/zlib_compressor.h>

#include "nbt.hpp"

namespace maprock {

namespace {

uint32_t u32le(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}

int32_t i32le(const uint8_t* p) {
  uint32_t v = u32le(p);
  int32_t r;
  std::memcpy(&r, &v, 4);
  return r;
}

class NullLogger : public leveldb::Logger {
 public:
  void Logv(const char*, va_list) override {}
};

std::string propValueString(const Value& v) {
  switch (v.type) {
    case Tag::Byte: return std::to_string(v.i8);
    case Tag::Short: return std::to_string(v.i16);
    case Tag::Int: return std::to_string(v.i32);
    case Tag::Long: return std::to_string(v.i64);
    case Tag::Float: return std::to_string(v.f32);
    case Tag::Double: return std::to_string(v.f64);
    case Tag::String: return v.str;
    default: return "";
  }
}

std::string stateKey(const BlockState& st) {
  std::string s = st.name;
  if (!st.props.empty()) {
    s += '[';
    for (size_t i = 0; i < st.props.size(); ++i) {
      if (i) s += ',';
      s += st.props[i].first + '=' + st.props[i].second;
    }
    s += ']';
  }
  return s;
}

bool isWaterName(const std::string& name) {
  return name == "minecraft:water" || name == "minecraft:flowing_water";
}

// Decodes one storage layer into `idx` (4096 local palette indices) and
// `palIds` (world palette id for each local entry). Returns false if the layer
// could not be decoded.
bool decodeLayer(const uint8_t* p, size_t n, size_t& pos, World& w,
                 std::vector<uint16_t>& idx, std::vector<uint16_t>& palIds) {
  idx.assign(4096, 0);
  palIds.clear();
  if (pos >= n) return false;

  int bits = (p[pos] >> 1) & 0x7F;
  ++pos;
  if (bits > 16) return false;

  if (bits == 0) {
    // Uniform subchunk: a single palette entry and no index words.
    if (pos + 4 > n) return false;
    uint32_t count = u32le(p + pos);
    pos += 4;
    if (count < 1 || count > 4096) return false;
    for (uint32_t i = 0; i < count; ++i) {
      size_t consumed = 0;
      try {
        Value v = parseNbt(p + pos, n - pos, true, &consumed);
        pos += consumed;
        BlockState st;
        if (v.type == Tag::Compound) {
          if (const Value* nm = v.get("name")) st.name = nm->asString();
          if (const Value* stv = v.get("states")) {
            for (const auto& kv : stv->compound)
              st.props.emplace_back(kv.first, propValueString(kv.second));
          }
        }
        std::sort(st.props.begin(), st.props.end());
        palIds.push_back(w.internState(st));
      } catch (...) {
        return false;
      }
    }
    return !palIds.empty();
  }

  const size_t blocksPerWord = 32 / static_cast<size_t>(bits);
  const size_t wordCount = (4096 + blocksPerWord - 1) / blocksPerWord;

  if (pos + wordCount * 4 > n) return false;
  std::vector<uint32_t> words(wordCount);
  for (size_t i = 0; i < wordCount; ++i) {
    words[i] = u32le(p + pos);
    pos += 4;
  }

  const uint32_t mask = (1u << bits) - 1;
  for (int i = 0; i < 4096; ++i) {
    size_t word = i / blocksPerWord;
    size_t off = (i % blocksPerWord) * bits;
    idx[i] = static_cast<uint16_t>((words[word] >> off) & mask);
  }

  if (pos + 4 > n) return false;
  uint32_t count = u32le(p + pos);
  pos += 4;
  if (count < 1 || count > 4096) return false;

  for (uint32_t i = 0; i < count; ++i) {
    size_t consumed = 0;
    try {
      Value v = parseNbt(p + pos, n - pos, true, &consumed);
      pos += consumed;
      BlockState st;
      if (v.type == Tag::Compound) {
        if (const Value* nm = v.get("name")) st.name = nm->asString();
        if (const Value* stv = v.get("states")) {
          for (const auto& kv : stv->compound)
            st.props.emplace_back(kv.first, propValueString(kv.second));
        }
      }
      std::sort(st.props.begin(), st.props.end());
      palIds.push_back(w.internState(st));
    } catch (...) {
      return false;
    }
  }
  return true;
}

void parseSubchunk(const std::string& value, int chunkX, int chunkZ, int subchunkY,
                   World& w) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(value.data());
  const size_t n = value.size();
  size_t pos = 0;
  if (n < 2) return;

  int version = p[pos++];
  int layers = p[pos++];
  int yIndex = subchunkY;
  if (version == 9) {
    if (pos >= n) return;
    yIndex = static_cast<int8_t>(p[pos++]);
  } else if (version != 8) {
    return;  // unknown subchunk version
  }
  if (layers < 1 || layers > 2) layers = 1;

  const int baseY = yIndex * 16;

  std::vector<uint16_t> idx;
  std::vector<uint16_t> palIds;

  // Layer 0: solid blocks.
  if (!decodeLayer(p, n, pos, w, idx, palIds)) return;
  for (int i = 0; i < 4096; ++i) {
    uint16_t local = idx[i];
    if (local >= palIds.size()) continue;
    uint16_t id = palIds[local];
    if (id == 0) continue;  // air
    int x = chunkX * 16 + (i >> 8);
    int z = chunkZ * 16 + ((i >> 4) & 0xF);
    int y = baseY + (i & 0xF);
    w.setBlock(x, y, z, id);
  }

  // Layer 1: water layer (waterlogged blocks).
  if (layers >= 2 && pos < n) {
    std::vector<uint16_t> idx2;
    std::vector<uint16_t> palIds2;
    if (decodeLayer(p, n, pos, w, idx2, palIds2)) {
      for (int i = 0; i < 4096; ++i) {
        uint16_t local = idx2[i];
        if (local >= palIds2.size()) continue;
        uint16_t id = palIds2[local];
        const std::string& nm = w.state(id).name;
        if (!isWaterName(nm)) continue;
        int x = chunkX * 16 + (i >> 8);
        int z = chunkZ * 16 + ((i >> 4) & 0xF);
        int y = baseY + (i & 0xF);
        if (w.blockAt(x, y, z) != 0) continue;  // already a solid block
        w.setBlock(x, y, z, id);
      }
    }
  }
}

}  // namespace

uint16_t World::blockAt(int32_t x, int32_t y, int32_t z) const {
  auto it = blocks.find(BlockPos{x, y, z});
  return it == blocks.end() ? 0 : it->second;
}

std::string World::shortName(uint16_t id) const {
  const std::string& n = palette[id].name;
  if (n.rfind("minecraft:", 0) == 0) return n.substr(10);
  return n;
}

int32_t World::topBlockAt(int32_t x, int32_t z, int32_t yMin, int32_t yMax) const {
  for (int32_t y = yMax; y >= yMin; --y) {
    if (blockAt(x, y, z) != 0) return y;
  }
  return yMin - 1;
}

uint16_t World::internState(const BlockState& st) {
  if (st.name.empty() || st.name == "minecraft:air") return 0;
  std::string key = stateKey(st);
  auto it = stateIndex_.find(key);
  if (it != stateIndex_.end()) return it->second;
  palette.push_back(st);
  uint16_t id = static_cast<uint16_t>(palette.size() - 1);
  stateIndex_[key] = id;
  return id;
}

void World::setBlock(int32_t x, int32_t y, int32_t z, uint16_t id) {
  if (id == 0) return;
  blocks[BlockPos{x, y, z}] = id;
  if (!hasBlocks) {
    minX = maxX = x;
    minY = maxY = y;
    minZ = maxZ = z;
    hasBlocks = true;
  } else {
    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;
    if (z < minZ) minZ = z;
    if (z > maxZ) maxZ = z;
  }
}

leveldb::DB* openBedrockDb(const std::string& path, bool createIfMissing) {
  leveldb::Options options;
  options.filter_policy = leveldb::NewBloomFilterPolicy(10);
  options.block_cache = leveldb::NewLRUCache(40 * 1024 * 1024);
  options.write_buffer_size = 4 * 1024 * 1024;
  options.create_if_missing = createIfMissing;
  options.info_log = new NullLogger();
  options.compressors[0] = new leveldb::ZlibCompressorRaw(-1);
  options.compressors[1] = new leveldb::ZlibCompressor();

  leveldb::DB* db = nullptr;
  leveldb::Status s = leveldb::DB::Open(options, path, &db);
  if (!s.ok()) {
    throw std::runtime_error("Failed to open LevelDB at '" + path + "': " + s.ToString());
  }
  return db;
}

void loadWorld(const std::string& worldPath, const LoadOptions& opts, World& out) {
  // Accept either the world directory (containing "db") or the db/ directory.
  std::string dbPath = worldPath;
  {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::is_directory(worldPath + "/db", ec)) {
      dbPath = worldPath + "/db";
    }
  }

  leveldb::DB* db = openBedrockDb(dbPath, false);

  leveldb::ReadOptions readOptions;
  readOptions.decompress_allocator = new leveldb::DecompressAllocator();
  readOptions.fill_cache = false;

  leveldb::Iterator* it = db->NewIterator(readOptions);
  try {
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      leveldb::Slice k = it->key();
      leveldb::Slice v = it->value();
      const uint8_t* kb = reinterpret_cast<const uint8_t*>(k.data());
      const size_t ks = k.size();

      int32_t cx = 0, cz = 0;
      int dim = 0;
      int tagIdx = -1;
      if (ks >= 8) {
        cx = i32le(kb);
        cz = i32le(kb + 4);
        if (ks >= 13) {
          dim = i32le(kb + 8);
          tagIdx = 12;
        } else if (ks >= 9) {
          tagIdx = 8;
        }
      }
      if (tagIdx < 0) continue;

      const int tag = kb[tagIdx];

      // Only process the selected dimension.
      if (tag == 0x2F) {
        bool isSubchunk = false;
        int subY = 0;
        if (dim == 0 && ks == 10) {
          isSubchunk = true;
          subY = static_cast<int8_t>(kb[9]);
        } else if (ks == 14 && dim == opts.dimension && opts.dimension != 0) {
          isSubchunk = true;
          subY = static_cast<int8_t>(kb[13]);
        }
        if (!isSubchunk) continue;

        // Region filter (chunk-space).
        if (opts.regionDefined) {
          int cx0 = cx * 16, cx1 = cx * 16 + 15;
          int cz0 = cz * 16, cz1 = cz * 16 + 15;
          int y0 = subY * 16, y1 = subY * 16 + 15;
          if (cx1 < opts.minX || cx0 > opts.maxX) continue;
          if (cz1 < opts.minZ || cz0 > opts.maxZ) continue;
          if (y1 < opts.minY || y0 > opts.maxY) continue;
        }

        parseSubchunk(v.ToString(), cx, cz, subY, out);
      }
    }
  } catch (...) {
    delete it;
    delete readOptions.decompress_allocator;
    delete db;
    throw;
  }

  delete it;
  delete readOptions.decompress_allocator;
  delete db;
}

}  // namespace maprock
