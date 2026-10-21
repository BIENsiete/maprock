#include "export_pipeline.hpp"

#include <filesystem>

#include "gltf_exporter.hpp"
#include "obj_exporter.hpp"
#include "zip.hpp"

namespace maprock {

namespace {

void report(const std::function<void(float, const std::string&)>& p, float f,
            const std::string& s) {
  if (p) p(f, s);
}

int addTexture(TextureAtlas& atlas, const BlockRegistry& reg, const std::string& rel,
               const Tint& tint) {
  if (rel.empty()) return 0;
  std::string file = reg.textureFile(rel);
  if (file.empty()) return 0;
  return atlas.add(file, tint);
}

}  // namespace

std::string prepare(Prepared& out, const std::string& worldPath,
                    const std::string& resourcePack, bool whole, int minX, int minY,
                    int minZ, int maxX, int maxY, int maxZ,
                    std::function<void(float, const std::string&)> progress) {
  out = Prepared{};

  report(progress, 0.05f, "Loading world...");

  // Accept a .mcworld archive directly by unpacking it to a temporary folder.
  std::string actualWorld = worldPath;
  std::string extractDir;
  const std::string ext = worldPath.size() >= 8 ? worldPath.substr(worldPath.size() - 8) : "";
  if (ext == ".mcworld") {
    report(progress, 0.05f, "Extracting .mcworld...");
    extractDir = extractMcworld(worldPath);
    if (extractDir.empty()) return "Failed to extract .mcworld archive.";
    actualWorld = extractDir;
  }

  LoadOptions lo;
  lo.regionDefined = !whole;
  lo.minX = minX; lo.minY = minY; lo.minZ = minZ;
  lo.maxX = maxX; lo.maxY = maxY; lo.maxZ = maxZ;
  try {
    loadWorld(actualWorld, lo, out.world);
  } catch (const std::exception& e) {
    if (!extractDir.empty()) std::filesystem::remove_all(extractDir);
    return std::string("Failed to load world: ") + e.what();
  }
  if (!extractDir.empty()) std::filesystem::remove_all(extractDir);
  if (!out.world.hasBlocks) {
    return "World contains no blocks (is the path a Bedrock world, db/ folder or .mcworld?)";
  }

  report(progress, 0.4f, "Loading resource pack...");
  out.registry.load(resourcePack);
  out.atlas.add("", Tint{});  // seed id 0 with the built-in "missing" texture

  const size_t n = out.world.palette.size();
  out.render.assign(n, RenderBlock{});
  for (uint16_t id = 1; id < n; ++id) {
    const BlockState& st = out.world.palette[id];
    const std::string shortName = out.world.shortName(id);
    Shape shape;
    std::string up, down, side;
    if (!out.registry.resolve(shortName, st.props, shape, up, down, side)) {
      // No resource pack / unknown block: render as an opaque cube with the
      // "missing" texture so the export stays complete.
      RenderBlock rb;
      rb.shape = Shape::Cube;
      rb.opaque = true;
      rb.texUp = rb.texDown = rb.texSide = 0;
      out.render[id] = rb;
      continue;
    }

    Tint tint{1.0f, 1.0f, 1.0f};
    if (shape == Shape::Water) tint = Tint{0.45f, 0.65f, 1.0f};

    RenderBlock rb;
    rb.shape = shape;
    rb.opaque = (shape == Shape::Cube);
    rb.texUp = addTexture(out.atlas, out.registry, up, tint);
    rb.texDown = addTexture(out.atlas, out.registry, down, tint);
    rb.texSide = addTexture(out.atlas, out.registry, side, tint);
    out.render[id] = rb;
  }

  // Map colors from the tinted top-face tiles in the atlas.
  out.colors.assign(n, {128, 128, 128});
  for (uint16_t id = 1; id < n; ++id) {
    uint8_t r, g, b;
    int tex = out.render[id].texUp ? out.render[id].texUp : out.render[id].texSide;
    if (out.atlas.averageOf(tex, r, g, b)) out.colors[id] = {r, g, b};
  }

  report(progress, 0.8f, "Ready");
  return "";
}

std::string exportPrepared(const Prepared& p, const std::string& outputBase,
                           const std::string& format, const Region* region) {
  Mesh mesh;
  buildMesh(p.world, p.render, p.atlas, region, mesh);
  if (mesh.empty()) return "Nothing to export: the selection contains no renderable blocks.";

  bool ok = false;
  if (format == "gltf") {
    ok = exportGltf(outputBase, mesh, p.atlas);
  } else {
    ok = exportObj(outputBase, mesh, p.atlas);
  }
  return ok ? "" : "Failed to write output files to '" + outputBase + "'.";
}

std::string runExport(const ExportOptions& opts) {
  Prepared p;
  std::string err = prepare(p, opts.worldPath, opts.resourcePack, opts.whole, opts.minX,
                            opts.minY, opts.minZ, opts.maxX, opts.maxY, opts.maxZ,
                            opts.progress);
  if (!err.empty()) return err;
  return exportPrepared(p, opts.outputBase, opts.format, nullptr);
}

}  // namespace maprock
