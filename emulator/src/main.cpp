#include "risc5.hpp"

#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

struct Options {
  std::string rom_path;
  std::string disk_path;
  std::uint64_t steps = 1000;
  bool disk_writable = true;
  bool help_requested = false;
};

void usage(const char* name) {
  std::cerr << "usage: " << name << " [--rom FILE] [--disk FILE] [--read-only] [--steps N]\n";
}

Options parse_options(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--rom" && i + 1 < argc) options.rom_path = argv[++i];
    else if (arg == "--disk" && i + 1 < argc) options.disk_path = argv[++i];
    else if (arg == "--read-only") options.disk_writable = false;
    else if (arg == "--steps" && i + 1 < argc) options.steps = std::stoull(argv[++i]);
    else if (arg == "--help" || arg == "-h") {
      options.help_requested = true;
      return options;
    }
    else throw std::invalid_argument("unknown or incomplete command-line option: " + arg);
  }
  if (!options.disk_writable && options.disk_path.empty()) {
    throw std::invalid_argument("--read-only requires --disk FILE");
  }
  return options;
}

} // namespace

int main(int argc, char** argv) {
  try {
    const auto options = parse_options(argc, argv);
    if (options.help_requested) {
      usage(argv[0]);
      return 0;
    }
    oberon::Machine machine;
    if (!options.rom_path.empty()) machine.memory().load_rom_file(options.rom_path);
    if (!options.disk_path.empty()) machine.open_disk(options.disk_path, options.disk_writable);
    machine.run(options.steps);
    const auto& cpu = machine.cpu();
    std::cout << "steps=" << cpu.steps() << " pc=0x" << std::hex << std::setw(6) << std::setfill('0') << cpu.pc_byte()
              << " r14=0x" << std::setw(8) << cpu.reg(14) << " r15=0x" << std::setw(8) << cpu.reg(15) << std::dec << "\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << "oberon-run: " << error.what() << '\n'; return 1; }
}
