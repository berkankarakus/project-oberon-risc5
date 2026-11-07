#include "risc5.hpp"
#include "options.hpp"

#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

struct Options {
  oberon::CommonOptions common;
  std::uint64_t steps = 1000;
};

void usage(const char* name) {
  std::cerr << "usage: " << name << " [--rom FILE] [--disk FILE] [--read-only] [--steps N]\n";
}

Options parse_options(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (oberon::parse_common_option(argc, argv, i, options.common)) continue;
    if (arg == "--steps" && i + 1 < argc) options.steps = std::stoull(argv[++i]);
    else throw std::invalid_argument("unknown or incomplete command-line option: " + arg);
  }
  oberon::validate_common_options(options.common);
  return options;
}

} // namespace

int main(int argc, char** argv) {
  try {
    const auto options = parse_options(argc, argv);
    if (options.common.help_requested) {
      usage(argv[0]);
      return 0;
    }
    oberon::Machine machine;
    if (!options.common.rom_path.empty()) machine.memory().load_rom_file(options.common.rom_path);
    if (!options.common.disk_path.empty()) machine.open_disk(options.common.disk_path, options.common.disk_writable);
    machine.run(options.steps);
    const auto& cpu = machine.cpu();
    std::cout << "steps=" << cpu.steps() << " pc=0x" << std::hex << std::setw(6) << std::setfill('0') << cpu.pc_byte()
              << " r14=0x" << std::setw(8) << cpu.reg(14) << " r15=0x" << std::setw(8) << cpu.reg(15) << std::dec << "\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << "oberon-run: " << error.what() << '\n'; return 1; }
}
