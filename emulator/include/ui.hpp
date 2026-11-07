#pragma once

#include "options.hpp"

#include <cstdint>
#include <string>

namespace oberon {
class Machine;

namespace ui {

struct Options : CommonOptions {
  std::uint64_t steps_per_frame = 100'000;
  double scale = 1.25;
  bool flip_y = true;
};

Options parse_options(int argc, char** argv);
int run(Machine& machine, const Options& options);

} // namespace ui
} // namespace oberon
