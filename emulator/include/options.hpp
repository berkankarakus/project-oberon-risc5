#pragma once

#include <string>

namespace oberon {

struct CommonOptions {
  std::string rom_path;
  std::string disk_path;
  bool disk_writable = true;
  bool help_requested = false;
};

bool parse_common_option(int argc, char** argv, int& index, CommonOptions& options);
void validate_common_options(const CommonOptions& options);

} // namespace oberon
