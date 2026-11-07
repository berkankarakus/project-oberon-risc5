#include "risc5.hpp"
#include "ui.hpp"

#include <cstdio>
#include <exception>

int main(int argc, char** argv) {
  try {
    const auto options = oberon::ui::parse_options(argc, argv);
    oberon::Machine machine;
    if (!options.rom_path.empty()) machine.memory().load_rom_file(options.rom_path);
    if (!options.disk_path.empty()) machine.open_disk(options.disk_path, options.disk_writable);
    return oberon::ui::run(machine, options);
  } catch (const std::exception& error) {
    std::fprintf(stderr, "oberon-ui: %s\n", error.what());
    return 1;
  }
}
