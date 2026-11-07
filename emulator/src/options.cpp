#include "options.hpp"

#include <stdexcept>

namespace oberon {

bool parse_common_option(int argc, char** argv, int& index, CommonOptions& options) {
  const std::string argument = argv[index];
  if (argument == "--rom" || argument == "--disk") {
    if (index + 1 >= argc) {
      throw std::invalid_argument("unknown or incomplete command-line option: " + argument);
    }
    auto& destination = argument == "--rom" ? options.rom_path : options.disk_path;
    destination = argv[++index];
    return true;
  }
  if (argument == "--read-only") {
    options.disk_writable = false;
    return true;
  }
  if (argument == "--help" || argument == "-h") {
    options.help_requested = true;
    return true;
  }
  return false;
}

void validate_common_options(const CommonOptions& options) {
  if (!options.disk_writable && options.disk_path.empty()) {
    throw std::invalid_argument("--read-only requires --disk FILE");
  }
}

} // namespace oberon
