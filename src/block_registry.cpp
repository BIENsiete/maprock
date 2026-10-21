#include "block_registry.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace maprock {

namespace {

using json = nlohmann::json;

std::string readFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return "";
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

// Bedrock's terrain_texture.json begins with a `//` comment line, which is not
// valid JSON. Strip whole-line `//` comments.
std::string stripLineComments(const std::string& s) {
  std::istringstream in(s);
  std::string line, out;
  while (std::getline(in, line)) {
    size_t first = line.find_first_not_of(" \t\r");
    if (first != std::string::npos && line.compare(first, 2, "//") == 0) continue;
    out += line;
    out += '\n';
  }
  return out;
}

bool contains(const std::string& hay, const std::string& needle) {
  return hay.find(needle) != std::string::npos;
}

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

bool fileExists(const std::string& p) {
  FILE* f = nullptr;
#ifdef _WIN32
  fopen_s(&f, p.c_str(), "rb");
#else
  f = fopen(p.c_str(), "rb");
#endif
  if (!f) return false;
  fclose(f);
  return true;
}

}  // namespace

int BlockRegistry::colorVariantIndex(const std::string& colorValue) {
  static const std::vector<std::string> order = {
      "white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray",
      "silver", "cyan", "purple", "blue", "brown", "green", "red", "black"};
  // Accept numeric values ("0".."15").
  for (size_t i = 0; i < order.size(); ++i) {
    if (colorValue == order[i]) return static_cast<int>(i);
  }
  try {
    int n = std::stoi(colorValue);
    if (n >= 0 && n < 16) return n;
  } catch (...) {
  }
  return 0;
}

bool BlockRegistry::load(const std::string& resourcePackDir) {
  dir_ = resourcePackDir;
  hasPack_ = false;
  blocks_.clear();
  textures_.clear();

  if (!loadBlocksJson(dir_ + "/blocks.json")) return false;
  if (!loadTerrainTexture(dir_ + "/textures/terrain_texture.json")) return false;
  hasPack_ = true;
  return true;
}

bool BlockRegistry::loadBlocksJson(const std::string& path) {
  std::string raw = readFile(path);
  if (raw.empty()) return false;
  try {
    json j = json::parse(raw);
    if (!j.is_object()) return false;
    for (auto it = j.begin(); it != j.end(); ++it) {
      const std::string name = it.key();
      const json& def = it.value();
      BlockDef bd;
      bd.known = false;
      if (def.contains("textures")) {
        const json& t = def["textures"];
        if (t.is_string()) {
          bd.texUp = bd.texDown = bd.texSide = t.get<std::string>();
          bd.known = true;
        } else if (t.is_object()) {
          if (t.contains("down")) bd.texDown = t["down"].get<std::string>();
          if (t.contains("up")) bd.texUp = t["up"].get<std::string>();
          if (t.contains("side")) bd.texSide = t["side"].get<std::string>();
          bd.known = !bd.texUp.empty() || !bd.texDown.empty() || !bd.texSide.empty();
        }
      }
      // If only some faces are set, fill the rest from the side/up faces.
      if (bd.texUp.empty()) bd.texUp = !bd.texSide.empty() ? bd.texSide : bd.texDown;
      if (bd.texDown.empty()) bd.texDown = !bd.texSide.empty() ? bd.texSide : bd.texUp;
      if (bd.texSide.empty()) bd.texSide = !bd.texUp.empty() ? bd.texUp : bd.texDown;
      blocks_[name] = bd;
    }
  } catch (...) {
    return false;
  }
  return true;
}

bool BlockRegistry::loadTerrainTexture(const std::string& path) {
  std::string raw = readFile(path);
  if (raw.empty()) return false;
  raw = stripLineComments(raw);
  try {
    json j = json::parse(raw);
    const json& td = j["texture_data"];
    if (!td.is_object()) return false;
    for (auto it = td.begin(); it != td.end(); ++it) {
      const std::string name = it.key();
      const json& entry = it.value();
      if (!entry.contains("textures")) continue;
      const json& t = entry["textures"];
      std::vector<Variant> variants;
      auto addVariant = [&](const json& e) {
        Variant v;
        if (e.is_string()) {
          v.path = e.get<std::string>();
        } else if (e.is_object()) {
          if (e.contains("path")) v.path = e["path"].get<std::string>();
          if (e.contains("overlay_color")) v.overlay = e["overlay_color"].get<std::string>();
        }
        if (!v.path.empty()) variants.push_back(std::move(v));
      };
      if (t.is_array()) {
        for (const json& e : t) addVariant(e);
      } else {
        addVariant(t);
      }
      if (!variants.empty()) textures_[name] = std::move(variants);
    }
  } catch (...) {
    return false;
  }
  return true;
}

std::string BlockRegistry::resolveTexture(
    const std::string& name, const std::vector<std::pair<std::string, std::string>>& props,
    std::string* overlayOut) const {
  if (name.empty()) return "";
  auto it = textures_.find(name);
  if (it == textures_.end()) return "";

  const std::vector<Variant>& variants = it->second;
  size_t idx = 0;
  // Colored blocks (wool, concrete, stained glass, ...) use the "color" state.
  if (variants.size() > 1) {
    for (const auto& p : props) {
      if (p.first == "color") {
        idx = static_cast<size_t>(colorVariantIndex(p.second));
        break;
      }
    }
  }
  if (idx >= variants.size()) idx = 0;
  if (overlayOut) *overlayOut = variants[idx].overlay;
  return variants[idx].path;
}

Shape BlockRegistry::classifyShape(const std::string& shortName, const BlockDef& def) {
  const std::string n = lower(shortName);

  if (n == "air" || n == "structure_void" || n == "light_block" ||
      n == "invisible_bedrock" || n == "barrier" || n == "moving_block" ||
      n == "unknown") {
    return Shape::None;
  }
  if (n == "water" || n == "flowing_water") return Shape::Water;
  if (n == "lava" || n == "flowing_lava") return Shape::Lava;

  static const std::vector<std::string> cross = {
      "sapling", "dandelion", "poppy", "allium", "azure_bluet", "blue_orchid",
      "cornflower", "lily_of_the_valley", "oxeye_daisy", "tulip", "wither_rose",
      "torchflower", "pitcher", "grass", "fern", "dead_bush", "mushroom", "roots",
      "sprouts", "fungus", "sugar_cane", "reeds", "sweet_berry", "nether_wart",
      "wheat", "beetroots", "carrots", "potatoes", "bamboo_sapling"};
  for (const std::string& k : cross) {
    if (contains(n, k)) return Shape::Cross;
  }

  static const std::vector<std::string> translucent = {
      "glass", "leaves", "ice", "slime", "honey", "web", "sculk", "tinted",
      "glow_lichen", "carpet"};
  for (const std::string& k : translucent) {
    if (contains(n, k)) return Shape::Translucent;
  }

  return Shape::Cube;
}

bool BlockRegistry::resolve(const std::string& shortName,
                            const std::vector<std::pair<std::string, std::string>>& props,
                            Shape& shape, std::string& texUp, std::string& texDown,
                            std::string& texSide) const {
  shape = Shape::None;
  texUp.clear();
  texDown.clear();
  texSide.clear();

  // Water/lava handled without needing a texture lookup (still resolve for tint).
  auto it = blocks_.find(shortName);
  BlockDef def;
  bool known = it != blocks_.end();
  if (known) def = it->second;

  shape = classifyShape(shortName, def);
  if (shape == Shape::None) return false;
  if (shape == Shape::Water || shape == Shape::Lava) {
    // Use the water/lava still texture if available for the top surface.
    if (known) {
      std::string o;
      std::string up = resolveTexture(def.texUp.empty() ? def.texSide : def.texUp, props, &o);
      std::string side = resolveTexture(def.texSide, props, nullptr);
      texUp = up;
      texDown = up;
      texSide = side.empty() ? up : side;
    }
    return true;
  }

  if (!known || !def.known) return false;

  texUp = resolveTexture(def.texUp, props, nullptr);
  texDown = resolveTexture(def.texDown, props, nullptr);
  texSide = resolveTexture(def.texSide, props, nullptr);
  if (texUp.empty() && texDown.empty() && texSide.empty()) return false;
  return true;
}

std::string BlockRegistry::textureFile(const std::string& relPath) const {
  if (relPath.empty()) return "";
  std::string base = dir_ + "/" + relPath;
  if (fileExists(base + ".png")) return base + ".png";
  if (fileExists(base + ".tga")) return base + ".tga";
  if (fileExists(base)) return base;
  return "";
}

}  // namespace maprock
