#include "nbt.hpp"

#include <cstring>

namespace maprock {

namespace {

class Reader {
 public:
  Reader(const uint8_t* data, size_t size, bool le) : p_(data), size_(size), le_(le) {}

  size_t position() const { return pos_; }

  Value readTag() {
    uint8_t t = u8();
    readName();
    return readPayload(static_cast<Tag>(t));
  }

  Value readPayload(Tag type) {
    switch (type) {
      case Tag::Byte: {
        Value v;
        v.type = Tag::Byte;
        v.i8 = static_cast<int8_t>(u8());
        return v;
      }
      case Tag::Short: {
        Value v;
        v.type = Tag::Short;
        v.i16 = static_cast<int16_t>(read16());
        return v;
      }
      case Tag::Int: {
        Value v;
        v.type = Tag::Int;
        v.i32 = static_cast<int32_t>(read32());
        return v;
      }
      case Tag::Long: {
        Value v;
        v.type = Tag::Long;
        v.i64 = static_cast<int64_t>(read64());
        return v;
      }
      case Tag::Float: {
        Value v;
        v.type = Tag::Float;
        uint32_t b = read32();
        std::memcpy(&v.f32, &b, 4);
        return v;
      }
      case Tag::Double: {
        Value v;
        v.type = Tag::Double;
        uint64_t b = read64();
        std::memcpy(&v.f64, &b, 8);
        return v;
      }
      case Tag::ByteArray: {
        Value v;
        v.type = Tag::ByteArray;
        uint32_t n = read32();
        v.bytes.resize(n);
        for (uint32_t i = 0; i < n; ++i) v.bytes[i] = u8();
        return v;
      }
      case Tag::String: {
        Value v;
        v.type = Tag::String;
        uint16_t n = read16();
        v.str.assign(reinterpret_cast<const char*>(p_ + pos_), n);
        pos_ += n;
        return v;
      }
      case Tag::List: {
        Value v;
        v.type = Tag::List;
        v.listType = static_cast<Tag>(u8());
        uint32_t n = read32();
        v.list.reserve(n);
        for (uint32_t i = 0; i < n; ++i) v.list.push_back(readPayload(v.listType));
        return v;
      }
      case Tag::Compound: {
        Value v;
        v.type = Tag::Compound;
        while (true) {
          uint8_t t = u8();
          if (t == 0) break;  // TAG_End
          std::string name = readName();
          v.compound[name] = readPayload(static_cast<Tag>(t));
        }
        return v;
      }
      case Tag::IntArray: {
        Value v;
        v.type = Tag::IntArray;
        uint32_t n = read32();
        v.intArray.resize(n);
        for (uint32_t i = 0; i < n; ++i) v.intArray[i] = static_cast<int32_t>(read32());
        return v;
      }
      case Tag::LongArray: {
        Value v;
        v.type = Tag::LongArray;
        uint32_t n = read32();
        v.longArray.resize(n);
        for (uint32_t i = 0; i < n; ++i) v.longArray[i] = static_cast<int64_t>(read64());
        return v;
      }
      case Tag::End:
      default:
        return Value{};
    }
  }

 private:
  std::string readName() {
    uint16_t n = read16();
    std::string s(reinterpret_cast<const char*>(p_ + pos_), n);
    pos_ += n;
    return s;
  }

  uint8_t u8() {
    need(1);
    return p_[pos_++];
  }
  uint8_t peek() {
    need(1);
    return p_[pos_];
  }
  uint16_t read16() {
    need(2);
    uint16_t v;
    if (le_) v = p_[pos_] | (uint16_t(p_[pos_ + 1]) << 8);
    else v = (uint16_t(p_[pos_]) << 8) | p_[pos_ + 1];
    pos_ += 2;
    return v;
  }
  uint32_t read32() {
    need(4);
    uint32_t v;
    if (le_)
      v = uint32_t(p_[pos_]) | (uint32_t(p_[pos_ + 1]) << 8) |
          (uint32_t(p_[pos_ + 2]) << 16) | (uint32_t(p_[pos_ + 3]) << 24);
    else
      v = (uint32_t(p_[pos_]) << 24) | (uint32_t(p_[pos_ + 1]) << 16) |
          (uint32_t(p_[pos_ + 2]) << 8) | uint32_t(p_[pos_ + 3]);
    pos_ += 4;
    return v;
  }
  uint64_t read64() {
    need(8);
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
      uint64_t b = p_[pos_ + i];
      if (le_) v |= b << (8 * i);
      else v = (v << 8) | b;
    }
    pos_ += 8;
    return v;
  }
  void need(size_t n) {
    if (pos_ + n > size_)
      throw std::runtime_error("NBT: unexpected end of data (pos=" + std::to_string(pos_) +
                               " size=" + std::to_string(size_) + " need=" +
                               std::to_string(n) + ")");
  }

  const uint8_t* p_;
  size_t size_;
  size_t pos_ = 0;
  bool le_;
};

}  // namespace

Value parseNbt(const uint8_t* data, size_t size, bool littleEndian) {
  Reader r(data, size, littleEndian);
  return r.readTag();
}

Value parseNbt(const uint8_t* data, size_t size, bool littleEndian, size_t* consumed) {
  Reader r(data, size, littleEndian);
  Value v = r.readTag();
  if (consumed) *consumed = r.position();
  return v;
}

Value parseNbtPayload(const uint8_t* data, size_t size, Tag type, bool littleEndian) {
  Reader r(data, size, littleEndian);
  return r.readPayload(type);
}

}  // namespace maprock
