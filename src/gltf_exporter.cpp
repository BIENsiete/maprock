#include "gltf_exporter.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include <nlohmann/json.hpp>

namespace maprock {

namespace {

using json = nlohmann::json;

std::string baseName(const std::string& p) {
  return std::filesystem::path(p).filename().string();
}

void writeUint32LE(std::vector<uint8_t>& b, uint32_t v) {
  b.push_back(static_cast<uint8_t>(v & 0xFF));
  b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
  b.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
  b.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void appendFloats(std::vector<uint8_t>& b, const std::vector<float>& f) {
  for (float v : f) {
    uint32_t u;
    std::memcpy(&u, &v, 4);
    writeUint32LE(b, u);
  }
}

}  // namespace

bool exportGltf(const std::string& basePath, const Mesh& mesh, const TextureAtlas& atlas) {
  const std::string gltfPath = basePath + ".gltf";
  const std::string binPath = basePath + ".bin";
  const std::string pngPath = basePath + ".png";

  const size_t vcount = mesh.positions.size() / 3;
  const size_t posBytes = mesh.positions.size() * 4;
  const size_t nrmBytes = mesh.normals.size() * 4;
  const size_t uvBytes = mesh.uvs.size() * 4;
  const size_t idxBytes = mesh.indices.size() * 4;

  std::vector<uint8_t> bin;
  bin.reserve(posBytes + nrmBytes + uvBytes + idxBytes);
  const size_t offPos = 0;
  appendFloats(bin, mesh.positions);
  const size_t offNrm = bin.size();
  appendFloats(bin, mesh.normals);
  const size_t offUv = bin.size();
  appendFloats(bin, mesh.uvs);
  const size_t offIdx = bin.size();
  for (uint32_t i : mesh.indices) writeUint32LE(bin, i);

  FILE* bf = nullptr;
#ifdef _WIN32
  fopen_s(&bf, binPath.c_str(), "wb");
#else
  bf = fopen(binPath.c_str(), "wb");
#endif
  if (!bf) return false;
  fwrite(bin.data(), 1, bin.size(), bf);
  fclose(bf);

  if (!atlas.writePng(pngPath)) return false;

  // Position min/max.
  float pmin[3] = {0, 0, 0}, pmax[3] = {0, 0, 0};
  if (vcount) {
    pmin[0] = pmax[0] = mesh.positions[0];
    pmin[1] = pmax[1] = mesh.positions[1];
    pmin[2] = pmax[2] = mesh.positions[2];
    for (size_t i = 0; i < vcount; ++i) {
      for (int k = 0; k < 3; ++k) {
        float v = mesh.positions[i * 3 + k];
        if (v < pmin[k]) pmin[k] = v;
        if (v > pmax[k]) pmax[k] = v;
      }
    }
  }

  const int POS = 0, NRM = 1, UV = 2, IDX = 3;

  json j;
  j["asset"]["version"] = "2.0";
  j["asset"]["generator"] = "maprock";

  j["buffers"] = json::array();
  j["buffers"].push_back({{"uri", baseName(binPath)}, {"byteLength", bin.size()}});

  j["bufferViews"] = json::array();
  j["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", offPos}, {"byteLength", posBytes}});
  j["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", offNrm}, {"byteLength", nrmBytes}});
  j["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", offUv}, {"byteLength", uvBytes}});
  j["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", offIdx}, {"byteLength", idxBytes}});

  j["accessors"] = json::array();
  j["accessors"].push_back({{"bufferView", POS},
                            {"componentType", 5126},
                            {"count", vcount},
                            {"type", "VEC3"},
                            {"min", {pmin[0], pmin[1], pmin[2]}},
                            {"max", {pmax[0], pmax[1], pmax[2]}}});
  j["accessors"].push_back(
      {{"bufferView", NRM}, {"componentType", 5126}, {"count", vcount}, {"type", "VEC3"}});
  j["accessors"].push_back(
      {{"bufferView", UV}, {"componentType", 5126}, {"count", vcount}, {"type", "VEC2"}});
  j["accessors"].push_back(
      {{"bufferView", IDX}, {"componentType", 5125}, {"count", mesh.indices.size()}, {"type", "SCALAR"}});

  j["images"] = json::array();
  j["images"].push_back({{"uri", baseName(pngPath)}});

  j["samplers"] = json::array();
  j["samplers"].push_back(
      {{"magFilter", 9729}, {"minFilter", 9729}, {"wrapS", 33071}, {"wrapT", 33071}});

  j["textures"] = json::array();
  j["textures"].push_back({{"source", 0}, {"sampler", 0}});

  j["materials"] = json::array();
  j["materials"].push_back({{"pbrMetallicRoughness",
                             {{"baseColorTexture", {{"index", 0}}},
                              {"metallicFactor", 0.0f},
                              {"roughnessFactor", 1.0f}}},
                            {"alphaMode", "BLEND"},
                            {"doubleSided", true}});

  j["meshes"] = json::array();
  j["meshes"].push_back({{"primitives",
                          {{"attributes",
                            {{"POSITION", POS}, {"NORMAL", NRM}, {"TEXCOORD_0", UV}}},
                           {"indices", IDX},
                           {"material", 0}}}});

  j["nodes"] = json::array();
  j["nodes"].push_back({{"mesh", 0}});
  j["scenes"] = json::array();
  j["scenes"].push_back({{"nodes", {0}}});
  j["scene"] = 0;

  std::ofstream o(gltfPath, std::ios::binary);
  if (!o) return false;
  o << j.dump(2);
  return true;
}

}  // namespace maprock
