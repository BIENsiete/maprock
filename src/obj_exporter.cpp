#include "obj_exporter.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

namespace maprock {

namespace {

std::string baseName(const std::string& p) {
  std::filesystem::path fp(p);
  return fp.filename().string();
}

}  // namespace

bool exportObj(const std::string& basePath, const Mesh& mesh, const TextureAtlas& atlas) {
  const std::string objPath = basePath + ".obj";
  const std::string mtlPath = basePath + ".mtl";
  const std::string pngPath = basePath + ".png";

  FILE* f = nullptr;
#ifdef _WIN32
  fopen_s(&f, objPath.c_str(), "wb");
#else
  f = fopen(objPath.c_str(), "wb");
#endif
  if (!f) return false;

  std::string pngName = baseName(pngPath);
  std::string mtlName = baseName(mtlPath);

  std::fprintf(f, "# maprock export\n");
  std::fprintf(f, "mtllib %s\n", mtlName.c_str());
  std::fprintf(f, "o maprock_world\n");

  const size_t vcount = mesh.positions.size() / 3;
  for (size_t i = 0; i < vcount; ++i) {
    std::fprintf(f, "v %.6f %.6f %.6f\n", mesh.positions[i * 3], mesh.positions[i * 3 + 1],
                 mesh.positions[i * 3 + 2]);
  }
  for (size_t i = 0; i < vcount; ++i) {
    // OBJ texture origin is bottom-left; our atlas is top-down, so flip v.
    std::fprintf(f, "vt %.6f %.6f\n", mesh.uvs[i * 2], 1.0f - mesh.uvs[i * 2 + 1]);
  }
  for (size_t i = 0; i < vcount; ++i) {
    std::fprintf(f, "vn %.6f %.6f %.6f\n", mesh.normals[i * 3], mesh.normals[i * 3 + 1],
                 mesh.normals[i * 3 + 2]);
  }

  std::fprintf(f, "usemtl maprock_atlas\n");
  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    uint32_t a = mesh.indices[i] + 1;
    uint32_t b = mesh.indices[i + 1] + 1;
    uint32_t c = mesh.indices[i + 2] + 1;
    std::fprintf(f, "f %u/%u/%u %u/%u/%u %u/%u/%u\n", a, a, a, b, b, b, c, c, c);
  }
  fclose(f);

  FILE* m = nullptr;
#ifdef _WIN32
  fopen_s(&m, mtlPath.c_str(), "wb");
#else
  m = fopen(mtlPath.c_str(), "wb");
#endif
  if (!m) return false;
  std::fprintf(m, "newmtl maprock_atlas\n");
  std::fprintf(m, "Ka 1.0 1.0 1.0\n");
  std::fprintf(m, "Kd 1.0 1.0 1.0\n");
  std::fprintf(m, "Ks 0.0 0.0 0.0\n");
  std::fprintf(m, "d 1.0\n");
  std::fprintf(m, "illum 2\n");
  std::fprintf(m, "map_Kd %s\n", pngName.c_str());
  fclose(m);

  return atlas.writePng(pngPath);
}

}  // namespace maprock
