#pragma once

#include <string>

namespace maprock {

// Launches the interactive map-view GUI. Returns the process exit code.
int runGui(const std::string& worldPath, const std::string& resourcePack);

}  // namespace maprock
