// Entry point for the GUI-only executable. Double-clicking it opens the map
// window directly (optionally with a world/resource-pack path as arguments).

#include <string>

#include "gui.hpp"

int main(int argc, char** argv) {
  std::string world = argc > 1 ? argv[1] : "";
  std::string rp = argc > 2 ? argv[2] : "";
  return maprock::runGui(world, rp);
}
