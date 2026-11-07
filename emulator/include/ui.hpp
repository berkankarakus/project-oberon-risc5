#pragma once

#include <cstdint>
#include <string>

namespace oberon {
class Machine;

namespace ui {

struct Options {
  std::string rom_path;
  std::string disk_path;
  std::uint64_t steps_per_frame = 100'000;
  double scale = 1.25;
  bool disk_writable = true;
  bool flip_y = true;
};

Options parse_options(int argc, char** argv);
int run(Machine& machine, const Options& options);

} // namespace ui
} // namespace oberon
