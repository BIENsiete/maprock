// Generates a tiny synthetic Bedrock world (LevelDB + subchunks) used by the
// smoke test. Not part of the converter itself.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <leveldb/db.h>
#include <leveldb/options.h>
#include <leveldb/slice.h>
#include <leveldb/write_batch.h>

#include "world.hpp"

namespace {

void putLE16(std::vector<uint8_t>& b, uint16_t v) {
  b.push_back(static_cast<uint8_t>(v & 0xFF));
  b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void putLE32(std::vector<uint8_t>& b, uint32_t v) {
  b.push_back(static_cast<uint8_t>(v & 0xFF));
  b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
  b.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
  b.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

std::vector<uint8_t> keyI32(int32_t v) {
  std::vector<uint8_t> b;
  b.reserve(4);
  uint32_t u;
  std::memcpy(&u, &v, 4);
  putLE32(b, u);
  return b;
}

// Little-endian NBT compound: { name: <name>, states: {} }
std::vector<uint8_t> paletteEntry(const std::string& name) {
  std::vector<uint8_t> b;
  b.push_back(0x0A);          // TAG_Compound
  b.push_back(0); b.push_back(0);  // empty tag name
  b.push_back(0x08);          // "name": TAG_String
  putLE16(b, 4);
  for (char c : std::string("name")) b.push_back(static_cast<uint8_t>(c));
  putLE16(b, static_cast<uint16_t>(name.size()));
  for (char c : name) b.push_back(static_cast<uint8_t>(c));
  b.push_back(0x0A);          // "states": TAG_Compound (empty)
  putLE16(b, 6);
  for (char c : std::string("states")) b.push_back(static_cast<uint8_t>(c));
  b.push_back(0x00);          // end of states
  b.push_back(0x00);          // end of outer compound
  return b;
}

std::vector<uint8_t> encodeSubchunk(const std::vector<uint8_t>& cells,
                                    const std::vector<std::string>& palette) {
  std::vector<uint8_t> b;
  b.push_back(8);         // version
  b.push_back(1);         // one layer
  b.push_back(4 << 1);    // 4 bits per block

  const int blocksPerWord = 8;
  const int wordCount = 512;
  for (int w = 0; w < wordCount; ++w) {
    uint32_t word = 0;
    for (int j = 0; j < blocksPerWord; ++j) {
      uint8_t idx = cells[w * blocksPerWord + j];
      word |= static_cast<uint32_t>(idx) << (j * 4);
    }
    putLE32(b, word);
  }

  putLE32(b, static_cast<uint32_t>(palette.size()));
  for (const auto& name : palette) {
    std::vector<uint8_t> e = paletteEntry(name);
    b.insert(b.end(), e.begin(), e.end());
  }
  return b;
}

}  // namespace

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : "test_world";
  int radius = argc > 2 ? std::atoi(argv[2]) : 1;
  if (radius < 0) radius = 0;
  if (radius > 8) radius = 8;

  std::error_code ec;
  std::filesystem::create_directories(dir + "/db", ec);

  leveldb::DB* db = maprock::openBedrockDb(dir + "/db", true);

  // Palette: air, stone, dirt, grass, water
  const std::vector<std::string> palette = {
      "minecraft:air", "minecraft:stone", "minecraft:dirt",
      "minecraft:grass_block", "minecraft:water"};

  std::vector<uint8_t> cells(4096, 0);
  for (int cx = -radius; cx <= radius; ++cx) {
    for (int cz = -radius; cz <= radius; ++cz) {
      // Version record.
      {
        std::vector<uint8_t> key = keyI32(cx);
        std::vector<uint8_t> kz = keyI32(cz);
        key.insert(key.end(), kz.begin(), kz.end());
        key.push_back(0x2C);
        std::vector<uint8_t> val = {42};
        leveldb::Slice k(reinterpret_cast<const char*>(key.data()), key.size());
        leveldb::Slice v(reinterpret_cast<const char*>(val.data()), val.size());
        db->Put(leveldb::WriteOptions(), k, v);
      }

      // Subchunk 0: ground layers with a small water pool.
      for (int i = 0; i < 4096; ++i) {
        int x = i >> 8;
        int z = (i >> 4) & 0xF;
        int y = i & 0xF;
        uint8_t idx = 0;
        if (y == 0) idx = 1;        // stone
        else if (y == 1) idx = 2;   // dirt
        else if (y == 2) idx = 3;   // grass
        // water pool near the chunk center
        if (y == 2 && x >= 6 && x <= 9 && z >= 6 && z <= 9) idx = 4;
        cells[i] = idx;
      }
      std::vector<uint8_t> sub = encodeSubchunk(cells, palette);
      std::vector<uint8_t> key = keyI32(cx);
      std::vector<uint8_t> kz = keyI32(cz);
      key.insert(key.end(), kz.begin(), kz.end());
      key.push_back(0x2F);
      key.push_back(0);  // subchunk y index
      leveldb::Slice k(reinterpret_cast<const char*>(key.data()), key.size());
      leveldb::Slice v(reinterpret_cast<const char*>(sub.data()), sub.size());
      db->Put(leveldb::WriteOptions(), k, v);
    }
  }

  delete db;
  std::printf("Wrote synthetic world to %s\n", dir.c_str());
  return 0;
}
