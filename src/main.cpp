#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "export_pipeline.hpp"

#ifdef MAPROCK_GUI
#include "gui/gui.hpp"
#endif

namespace {

void printUsage() {
  std::printf(
      "maprock - Minecraft Bedrock world -> 3D model exporter\n\n"
      "Usage:\n"
      "  maprock --world <path> [--resource-pack <path>] [options]\n\n"
      "Options:\n"
      "  --world <path>         A .mcworld file, a world directory (containing 'db'),\n"
      "                         or a 'db' folder\n"
      "  --resource-pack <path> Path to the bedrock-samples 'resource_pack' folder\n"
      "                         (optional; a built-in fallback is used if missing)\n"
      "  --whole                Export the entire world\n"
      "  --min x,y,z            Inclusive min corner of a region to export\n"
      "  --max x,y,z            Inclusive max corner of a region to export\n"
      "  --format obj|gltf      Output format (default: obj)\n"
      "  --output <path>        Output base path (default: world)\n"
      "  --gui                  Launch the interactive map-view GUI\n"
      "  --help                 Show this help\n\n"
      "Examples:\n"
      "  maprock --world MyWorld --whole --format obj --output out\n"
      "  maprock --world MyWorld --min -64,0,-64 --max 64,120,64 --format gltf\n");
}

bool split3(const std::string& s, int& a, int& b, int& c) {
  int v[3] = {0, 0, 0};
  size_t start = 0;
  for (int i = 0; i < 3; ++i) {
    size_t comma = s.find(',', start);
    std::string part = s.substr(start, comma == std::string::npos ? std::string::npos
                                                                  : comma - start);
    try {
      v[i] = std::stoi(part);
    } catch (...) {
      return false;
    }
    if (comma == std::string::npos) {
      if (i != 2) return false;
      break;
    }
    start = comma + 1;
  }
  a = v[0];
  b = v[1];
  c = v[2];
  return true;
}

std::string stripExtension(std::string s) {
  static const char* exts[] = {".obj", ".gltf", ".glb", ".mtl"};
  for (const char* e : exts) {
    size_t n = std::strlen(e);
    if (s.size() > n && s.compare(s.size() - n, n, e) == 0) {
      s.resize(s.size() - n);
      break;
    }
  }
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  // Double-clicking the console exe (no arguments) opens the GUI directly.
  if (argc == 1) {
#ifdef MAPROCK_GUI
    return maprock::runGui("", "");
#else
    printUsage();
    return 0;
#endif
  }

  maprock::ExportOptions opts;
  opts.outputBase = "world";
  bool gui = false;
  bool haveMin = false, haveMax = false;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&](const char* name) -> std::string {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "error: %s requires a value\n", name);
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--help" || a == "-h") {
      printUsage();
      return 0;
    } else if (a == "--world") {
      opts.worldPath = next("--world");
    } else if (a == "--resource-pack") {
      opts.resourcePack = next("--resource-pack");
    } else if (a == "--whole") {
      opts.whole = true;
    } else if (a == "--min") {
      std::string v = next("--min");
      if (!split3(v, opts.minX, opts.minY, opts.minZ)) {
        std::fprintf(stderr, "error: --min expects x,y,z\n");
        return 2;
      }
      haveMin = true;
    } else if (a == "--max") {
      std::string v = next("--max");
      if (!split3(v, opts.maxX, opts.maxY, opts.maxZ)) {
        std::fprintf(stderr, "error: --max expects x,y,z\n");
        return 2;
      }
      haveMax = true;
    } else if (a == "--format") {
      opts.format = next("--format");
    } else if (a == "--output") {
      opts.outputBase = stripExtension(next("--output"));
    } else if (a == "--gui") {
      gui = true;
    } else {
      std::fprintf(stderr, "error: unknown argument '%s'\n", a.c_str());
      printUsage();
      return 2;
    }
  }

  if (opts.worldPath.empty()) {
    std::fprintf(stderr, "error: --world is required\n");
    printUsage();
    return 2;
  }
  if (opts.format != "obj" && opts.format != "gltf") {
    std::fprintf(stderr, "error: --format must be 'obj' or 'gltf'\n");
    return 2;
  }
  if (haveMin != haveMax) {
    std::fprintf(stderr, "error: --min and --max must be provided together\n");
    return 2;
  }
  if (haveMin && haveMax) opts.whole = false;

#ifdef MAPROCK_GUI
  if (gui) {
    return maprock::runGui(opts.worldPath, opts.resourcePack);
  }
#else
  if (gui) {
    std::fprintf(stderr, "error: this build does not include the GUI (MAPROCK_BUILD_GUI)\n");
    return 2;
  }
#endif

  opts.progress = [](float f, const std::string& s) {
    std::printf("\r[%3d%%] %-40s", static_cast<int>(f * 100), s.c_str());
    std::fflush(stdout);
    if (f >= 1.0f) std::printf("\n");
  };

  std::string err = maprock::runExport(opts);
  if (!err.empty()) {
    std::fprintf(stderr, "\nerror: %s\n", err.c_str());
    return 1;
  }
  std::printf("\nExported to %s.%s\n", opts.outputBase.c_str(), opts.format.c_str());
  return 0;
}
