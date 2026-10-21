#include "mesher.hpp"

namespace maprock {

namespace {

struct V {
  float x, y, z, u, v;
};

void emitQuad(Mesh& m, float nx, float ny, float nz, const V& a, const V& b,
              const V& c, const V& d) {
  uint32_t base = static_cast<uint32_t>(m.positions.size() / 3);
  const V* vs[4] = {&a, &b, &c, &d};
  for (int i = 0; i < 4; ++i) {
    m.positions.insert(m.positions.end(), {vs[i]->x, vs[i]->y, vs[i]->z});
    m.normals.insert(m.normals.end(), {nx, ny, nz});
    m.uvs.insert(m.uvs.end(), {vs[i]->u, vs[i]->v});
  }
  m.indices.insert(m.indices.end(),
                   {base, base + 1, base + 2, base + 2, base + 3, base});
}

void uvRect(const TextureAtlas& atlas, int texId, float& u0, float& v0, float& u1,
            float& v1) {
  atlas.uv(texId, u0, v0, u1, v1);
}

void emitCubeFace(Mesh& m, const TextureAtlas& atlas, int texId, int x, int y, int z,
                  int face) {
  float u0, v0, u1, v1;
  uvRect(atlas, texId, u0, v0, u1, v1);
  const float X0 = static_cast<float>(x), X1 = static_cast<float>(x + 1);
  const float Y0 = static_cast<float>(y), Y1 = static_cast<float>(y + 1);
  const float Z0 = static_cast<float>(z), Z1 = static_cast<float>(z + 1);

  switch (face) {
    case 0:  // +Y top
      emitQuad(m, 0, 1, 0, {X0, Y1, Z0, u0, v0}, {X1, Y1, Z0, u1, v0},
               {X1, Y1, Z1, u1, v1}, {X0, Y1, Z1, u0, v1});
      break;
    case 1:  // -Y bottom
      emitQuad(m, 0, -1, 0, {X0, Y0, Z0, u0, v0}, {X0, Y0, Z1, u1, v0},
               {X1, Y0, Z1, u1, v1}, {X1, Y0, Z0, u0, v1});
      break;
    case 2:  // +X
      emitQuad(m, 1, 0, 0, {X1, Y0, Z0, u0, v1}, {X1, Y0, Z1, u1, v1},
               {X1, Y1, Z1, u1, v0}, {X1, Y1, Z0, u0, v0});
      break;
    case 3:  // -X
      emitQuad(m, -1, 0, 0, {X0, Y0, Z0, u0, v1}, {X0, Y1, Z0, u0, v0},
               {X0, Y1, Z1, u1, v0}, {X0, Y0, Z1, u1, v1});
      break;
    case 4:  // +Z
      emitQuad(m, 0, 0, 1, {X0, Y0, Z1, u0, v1}, {X1, Y0, Z1, u1, v1},
               {X1, Y1, Z1, u1, v0}, {X0, Y1, Z1, u0, v0});
      break;
    case 5:  // -Z
      emitQuad(m, 0, 0, -1, {X0, Y0, Z0, u0, v1}, {X0, Y1, Z0, u0, v0},
               {X1, Y1, Z0, u1, v0}, {X1, Y0, Z0, u1, v1});
      break;
  }
}

void emitCross(Mesh& m, const TextureAtlas& atlas, int texId, int x, int y, int z) {
  float u0, v0, u1, v1;
  uvRect(atlas, texId, u0, v0, u1, v1);
  const float X0 = static_cast<float>(x), X1 = static_cast<float>(x + 1);
  const float Y0 = static_cast<float>(y), Y1 = static_cast<float>(y + 1);
  const float Z0 = static_cast<float>(z), Z1 = static_cast<float>(z + 1);

  // Diagonal quad 1: (x,z) -> (x+1,z+1)
  emitQuad(m, 0, 0, 1, {X0, Y0, Z0, u0, v1}, {X1, Y0, Z1, u1, v1},
           {X1, Y1, Z1, u1, v0}, {X0, Y1, Z0, u0, v0});
  // Diagonal quad 2: (x,z+1) -> (x+1,z)
  emitQuad(m, 1, 0, 0, {X0, Y0, Z1, u0, v1}, {X1, Y0, Z0, u1, v1},
           {X1, Y1, Z0, u1, v0}, {X0, Y1, Z1, u0, v0});
}

bool isOpaque(const World& world, const std::vector<RenderBlock>& render, int x, int y,
              int z) {
  uint16_t id = world.blockAt(x, y, z);
  if (id == 0) return false;
  if (id >= render.size()) return false;
  return render[id].opaque;
}

bool inside(const Region* r, int x, int y, int z) {
  if (!r) return true;
  return x >= r->minX && x <= r->maxX && y >= r->minY && y <= r->maxY &&
         z >= r->minZ && z <= r->maxZ;
}

}  // namespace

void buildMesh(const World& world, const std::vector<RenderBlock>& render,
               const TextureAtlas& atlas, const Region* region, Mesh& out) {
  out = Mesh{};
  for (const auto& kv : world.blocks) {
    const BlockPos& p = kv.first;
    uint16_t id = kv.second;
    if (id == 0 || id >= render.size()) continue;
    if (!inside(region, p.x, p.y, p.z)) continue;
    const RenderBlock& rb = render[id];

    switch (rb.shape) {
      case Shape::None:
        break;
      case Shape::Cube:
      case Shape::Translucent: {
        int faceTex[6] = {rb.texUp, rb.texDown, rb.texSide, rb.texSide, rb.texSide,
                          rb.texSide};
        const int nx[6] = {0, 0, 1, -1, 0, 0};
        const int ny[6] = {1, -1, 0, 0, 0, 0};
        const int nz[6] = {0, 0, 0, 0, 1, -1};
        for (int f = 0; f < 6; ++f) {
          int nxv = p.x + nx[f], nyv = p.y + ny[f], nzv = p.z + nz[f];
          if (!inside(region, nxv, nyv, nzv)) {
            emitCubeFace(out, atlas, faceTex[f], p.x, p.y, p.z, f);  // open boundary
          } else if (!isOpaque(world, render, nxv, nyv, nzv)) {
            emitCubeFace(out, atlas, faceTex[f], p.x, p.y, p.z, f);
          }
        }
        break;
      }
      case Shape::Cross:
        emitCross(out, atlas, rb.texSide ? rb.texSide : rb.texUp, p.x, p.y, p.z);
        break;
      case Shape::Water:
      case Shape::Lava: {
        int topY = p.y + 1;
        if (!inside(region, p.x, topY, p.z) || world.blockAt(p.x, topY, p.z) == 0) {
          // Top of the water column, or open region boundary.
          emitCubeFace(out, atlas, rb.texUp ? rb.texUp : rb.texSide, p.x, p.y, p.z, 0);
        }
        break;
      }
    }
  }
}

}  // namespace maprock
