#include "zip.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include <zlib.h>

namespace maprock {

namespace {

uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t u32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}

bool inflateRaw(const uint8_t* src, size_t srcLen, size_t dstLen,
                std::vector<uint8_t>& out) {
  out.resize(dstLen);
  // ZIP entries use raw DEFLATE (RFC 1951); some writers emit zlib-wrapped
  // data, so try raw first and fall back to zlib.
  for (int wbits : {-15, 15}) {
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, wbits) != Z_OK) continue;
    zs.next_in = const_cast<Bytef*>(src);
    zs.avail_in = static_cast<uInt>(srcLen);
    zs.next_out = out.data();
    zs.avail_out = static_cast<uInt>(dstLen);
    int r = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (r == Z_STREAM_END) return true;
  }
  return false;
}

}  // namespace

bool readZip(const std::string& path, std::vector<ZipEntry>& out) {
  out.clear();
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
  if (buf.size() < 22) return false;

  // Locate the End Of Central Directory record (scanning backwards to tolerate
  // trailing data).
  size_t eocd = SIZE_MAX;
  for (size_t i = buf.size(); i >= 22; --i) {
    size_t off = i - 22;
    if (buf[off] == 0x50 && buf[off + 1] == 0x4B && buf[off + 2] == 0x05 &&
        buf[off + 3] == 0x06) {
      uint16_t commentLen = u16(&buf[off + 20]);
      if (off + 22 + commentLen == buf.size()) {
        eocd = off;
        break;
      }
    }
  }
  if (eocd == SIZE_MAX) return false;

  const size_t cdOffset = u32(&buf[eocd + 16]);
  const size_t cdCount = u16(&buf[eocd + 10]);
  if (cdOffset >= buf.size()) return false;

  size_t p = cdOffset;
  for (size_t i = 0; i < cdCount; ++i) {
    if (p + 46 > buf.size()) return false;
    if (u32(&buf[p]) != 0x02014b50) return false;
    const uint16_t method = u16(&buf[p + 10]);
    const uint32_t compSize = u32(&buf[p + 20]);
    const uint32_t uncompSize = u32(&buf[p + 24]);
    const uint16_t nameLen = u16(&buf[p + 28]);
    const uint16_t extraLen = u16(&buf[p + 30]);
    const uint16_t commentLen = u16(&buf[p + 32]);
    const uint32_t localOffset = u32(&buf[p + 42]);

    std::string name(reinterpret_cast<const char*>(&buf[p + 46]), nameLen);
    // Normalize Windows-style separators to the ZIP convention.
    for (char& c : name) {
      if (c == '\\') c = '/';
    }

    ZipEntry e;
    e.name = name;
    e.isDir = (name.empty() || name.back() == '/');
    if (!e.isDir) {
      if (localOffset + 30 > buf.size()) return false;
      if (u32(&buf[localOffset]) != 0x04034b50) return false;
      const uint16_t lnameLen = u16(&buf[localOffset + 26]);
      const uint16_t lextraLen = u16(&buf[localOffset + 28]);
      size_t dataOff = localOffset + 30 + lnameLen + lextraLen;
      if (dataOff + compSize > buf.size()) return false;

      if (method == 0) {
        e.data.assign(buf.begin() + dataOff, buf.begin() + dataOff + compSize);
      } else if (method == 8) {
        if (!inflateRaw(&buf[dataOff], compSize, uncompSize, e.data)) return false;
      } else {
        return false;  // unsupported compression method
      }
    }
    out.push_back(std::move(e));

    p += 46 + nameLen + extraLen + commentLen;
  }
  return true;
}

std::string extractMcworld(const std::string& mcworldPath) {
  std::vector<ZipEntry> entries;
  if (!readZip(mcworldPath, entries)) return "";

  std::error_code ec;
  std::filesystem::path tmp = std::filesystem::temp_directory_path(ec);
  if (ec) return "";

  // Unique directory name from the file name + a counter.
  static int counter = 0;
  std::string base = std::filesystem::path(mcworldPath).stem().string();
  std::filesystem::path dir = tmp / ("maprock_" + base + "_" + std::to_string(++counter));
  std::filesystem::create_directories(dir, ec);
  if (ec) return "";

  bool wroteDb = false;
  for (const ZipEntry& e : entries) {
    const std::string n = e.name;
    // Only extract the LevelDB database and level.dat.
    if (n.rfind("db/", 0) != 0 && n != "level.dat") continue;
    if (e.isDir) continue;

    std::filesystem::path out = dir / n;
    std::filesystem::create_directories(out.parent_path(), ec);
    std::ofstream of(out, std::ios::binary);
    if (!of) continue;
    of.write(reinterpret_cast<const char*>(e.data.data()),
             static_cast<std::streamsize>(e.data.size()));
    if (n.rfind("db/", 0) == 0) wroteDb = true;
  }

  if (!wroteDb) {
    std::filesystem::remove_all(dir, ec);
    return "";
  }
  return dir.string();
}

}  // namespace maprock
