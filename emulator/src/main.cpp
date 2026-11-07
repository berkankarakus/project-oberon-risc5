#include "risc5.hpp"

#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

namespace { void usage(const char* name) { std::cerr << "usage: " << name << " [--rom FILE] [--steps N]\n"; } }

int main(int argc, char** argv) {
  std::string rom_path;
  std::uint64_t steps = 1000;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--rom" && i + 1 < argc) rom_path = argv[++i];
    else if (arg == "--steps" && i + 1 < argc) steps = std::stoull(argv[++i]);
    else if (arg == "--help" || arg == "-h") { usage(argv[0]); return 0; }
    else { usage(argv[0]); return 2; }
  }
  try {
    oberon::Memory memory;
    if (!rom_path.empty()) memory.load_rom_file(rom_path);
    oberon::Risc5 cpu(memory);
    for (std::uint64_t i = 0; i < steps; ++i) cpu.step();
    std::cout << "steps=" << cpu.steps() << " pc=0x" << std::hex << std::setw(6) << std::setfill('0') << cpu.pc_byte()
              << " r14=0x" << std::setw(8) << cpu.reg(14) << " r15=0x" << std::setw(8) << cpu.reg(15) << std::dec << "\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << "oberon-run: " << error.what() << '\n'; return 1; }
}
