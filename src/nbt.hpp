#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace maprock {

enum class Tag : uint8_t {
  End = 0,
  Byte = 1,
  Short = 2,
  Int = 3,
  Long = 4,
  Float = 5,
  Double = 6,
  ByteArray = 7,
  String = 8,
  List = 9,
  Compound = 10,
  IntArray = 11,
  LongArray = 12,
};

// A generic NBT value. Bedrock stores chunk/entity NBT in *little-endian*
// (unlike Java Edition), so numeric reads are parameterised by byte order.
struct Value {
  Tag type = Tag::End;

  int8_t i8 = 0;
  int16_t i16 = 0;
  int32_t i32 = 0;
  int64_t i64 = 0;
  float f32 = 0;
  double f64 = 0;

  std::string str;
  std::vector<uint8_t> bytes;
  std::vector<int32_t> intArray;
  std::vector<int64_t> longArray;

  std::vector<Value> list;
  Tag listType = Tag::End;

  std::map<std::string, Value> compound;

  bool has(const std::string& key) const {
    return compound.find(key) != compound.end();
  }
  const Value& at(const std::string& key) const {
    auto it = compound.find(key);
    if (it == compound.end())
      throw std::runtime_error("NBT: missing key '" + key + "'");
    return it->second;
  }
  // Non-throwing lookup used by callers that tolerate absence.
  const Value* get(const std::string& key) const {
    auto it = compound.find(key);
    return it == compound.end() ? nullptr : &it->second;
  }

  std::string asString() const { return str; }
  int32_t asInt() const { return i32; }
  int64_t asLong() const { return i64; }
};

// Reads a single root tag (type byte + name + payload).
// Throws std::runtime_error on malformed input.
Value parseNbt(const uint8_t* data, size_t size, bool littleEndian);

// Same as above but also reports how many bytes were consumed. Useful for
// parsing sequences of concatenated tags (e.g. subchunk palettes).
Value parseNbt(const uint8_t* data, size_t size, bool littleEndian, size_t* consumed);

// Reads only a payload of the given type (no type byte / name prefix), used for
// list elements and palette entries that already know their type.
Value parseNbtPayload(const uint8_t* data, size_t size, Tag type, bool littleEndian);

}  // namespace maprock
