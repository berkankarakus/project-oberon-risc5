#include "risc5.hpp"
#include "disk_image.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using oberon::Memory;
using oberon::Risc5;

namespace {
constexpr std::uint32_t mov_imm(unsigned a, std::uint16_t value, bool negative = false) { return 0x4000'0000u | (a << 24) | (negative ? 0x1000'0000u : 0u) | value; }
constexpr std::uint32_t alu(unsigned op, unsigned a, unsigned b, unsigned c) { return (a << 24) | (b << 20) | (op << 16) | c; }
constexpr std::uint32_t branch_relative(unsigned condition_code, std::int32_t displacement, bool link = false) { const auto branch_opcode = 0xc000'0000u | (link ? 0x3000'0000u : 0x1000'0000u); return branch_opcode | ((condition_code & 0x0fu) << 24) | (static_cast<std::uint32_t>(displacement) & 0x003f'ffffu); }

void test_arithmetic_and_flags() {
  Memory memory; std::array<std::uint32_t, Memory::RomWords> rom{}; rom[0] = mov_imm(1, 7); rom[1] = mov_imm(2, 5); rom[2] = alu(8, 3, 1, 2); memory.load_rom_words(rom); Risc5 cpu(memory); cpu.step(); cpu.step(); cpu.step(); assert(cpu.reg(3) == 12); assert(!cpu.zero()); assert(!cpu.negative());
}

void test_branch_and_link() {
  Memory memory; std::array<std::uint32_t, Memory::RomWords> rom{}; rom[0] = branch_relative(7, 2, true); rom[1] = mov_imm(1, 0xdead); rom[2] = mov_imm(1, 0xbeef); rom[3] = mov_imm(2, 42); memory.load_rom_words(rom); Risc5 cpu(memory); cpu.step(); cpu.step(); assert(cpu.reg(1) == 0); assert(cpu.reg(2) == 42); assert(cpu.reg(15) == (Risc5::ResetPc + 1) * 4);
}

void test_memory_word_and_byte_access() {
  Memory memory; std::array<std::uint32_t, Memory::RomWords> rom{}; rom[0] = mov_imm(1, 0x100); rom[1] = 0x4000'0000u | (2u << 24) | 0x00aau; rom[2] = 0x8000'0000u | 0x2000'0000u | 0x1000'0000u | (2u << 24) | (1u << 20) | 0x20u; rom[3] = 0x8000'0000u | 0x1000'0000u | (3u << 24) | (1u << 20) | 0x20u; memory.load_rom_words(rom); Risc5 cpu(memory); cpu.step(); cpu.step(); cpu.step(); cpu.step(); assert(cpu.reg(3) == 0xaa);
}

void test_mmio_devices() {
  Memory memory;
  memory.advance_time();
  memory.advance_time();
  assert(memory.read32(Memory::IoBase) == 2);
  memory.write32(Memory::IoBase + 4, 0x5a);
  assert(memory.read32(Memory::IoBase + 4) == 0x5a);
  memory.write32(Memory::IoBase + 8, 'O');
  memory.write32(Memory::IoBase + 8, 'K');
  assert(memory.take_uart_output() == "OK");
  memory.enqueue_uart_rx('A');
  assert((memory.read32(Memory::IoBase + 12) & 1u) != 0);
  assert(memory.read32(Memory::IoBase + 8) == 'A');
  assert((memory.read32(Memory::IoBase + 12) & 1u) == 0);
  memory.enqueue_keyboard(0x1c);
  assert((memory.read32(Memory::IoBase + 24) & 0x1000'0000u) != 0);
  assert(memory.read32(Memory::IoBase + 28) == 0x1c);
  assert((memory.read32(Memory::IoBase + 24) & 0x1000'0000u) == 0);
}

void test_disk_image() {
  const auto path = (std::filesystem::temp_directory_path() / "project-oberon-test.img").string();
  bool rejected_zero_sectors = false;
  try {
    oberon::DiskImage invalid;
    invalid.create(path, 0);
  } catch (const std::invalid_argument&) {
    rejected_zero_sectors = true;
  }
  assert(rejected_zero_sectors);

  const auto invalid_path = path + ".invalid";
  {
    std::ofstream malformed(invalid_path, std::ios::binary | std::ios::trunc);
    malformed.put('\0');
  }
  bool rejected_partial_sector = false;
  try {
    oberon::DiskImage invalid;
    invalid.open(invalid_path, false);
  } catch (const std::runtime_error&) {
    rejected_partial_sector = true;
  }
  assert(rejected_partial_sector);
  std::filesystem::remove(invalid_path);

  oberon::DiskImage image;
  image.create(path, 4);
  assert(image.sector_count() == 4);
  oberon::DiskImage::Sector sector{};
  for (std::size_t i = 0; i < sector.size(); ++i) sector[i] = static_cast<std::uint8_t>(i ^ 0xa5u);
  image.write(2, sector);
  const auto loaded = image.read(2);
  assert(loaded == sector);

  bool rejected_read_out_of_range = false;
  try {
    image.read(image.sector_count());
  } catch (const std::out_of_range&) {
    rejected_read_out_of_range = true;
  }
  assert(rejected_read_out_of_range);

  bool rejected_write_out_of_range = false;
  try {
    image.write(image.sector_count(), sector);
  } catch (const std::out_of_range&) {
    rejected_write_out_of_range = true;
  }
  assert(rejected_write_out_of_range);
  image.close();

  oberon::DiskImage read_only;
  read_only.open(path, false);
  assert(!read_only.writable());
  bool rejected_read_only_write = false;
  try {
    read_only.write(0, sector);
  } catch (const std::runtime_error&) {
    rejected_read_only_write = true;
  }
  assert(rejected_read_only_write);
  read_only.close();
  std::filesystem::remove(path);
}

void test_spi_disk_read_write() {
  const auto path = (std::filesystem::temp_directory_path() / "project-oberon-spi.img").string();
  oberon::DiskImage image;
  image.create(path, 2);
  oberon::DiskImage::Sector source{};
  for (std::size_t i = 0; i < source.size(); ++i) source[i] = static_cast<std::uint8_t>((i * 3u) ^ 0x42u);
  image.write(1, source);
  Memory memory;
  memory.attach_disk(image);

  // Select card and issue CMD17 for sector 1.
  memory.write32(Memory::IoBase + 20, 1);
  const auto spi = [&](std::uint8_t byte) {
    memory.write32(Memory::IoBase + 16, byte);
    return static_cast<std::uint8_t>(memory.read32(Memory::IoBase + 16));
  };
  for (const auto byte : {std::uint8_t(0x51), std::uint8_t(0), std::uint8_t(0), std::uint8_t(0), std::uint8_t(1), std::uint8_t(0xff)}) spi(byte);
  assert(spi(0xff) == 0x00);
  assert(spi(0xff) == 0xfe);
  for (const auto expected : source) assert(spi(0xff) == expected);
  assert(spi(0xff) == 0xff);
  assert(spi(0xff) == 0xff);

  memory.write32(Memory::IoBase + 20, 1);
  for (const auto byte : {std::uint8_t(0x58), std::uint8_t(0), std::uint8_t(0), std::uint8_t(0), std::uint8_t(1), std::uint8_t(0xff)}) spi(byte);
  assert(spi(0xff) == 0x00);
  spi(0xfe);
  oberon::DiskImage::Sector replacement{};
  for (std::size_t i = 0; i < replacement.size(); ++i) {
    replacement[i] = static_cast<std::uint8_t>(0xffu - (i & 0xffu));
    spi(replacement[i]);
  }
  spi(0xff);
  assert(spi(0xff) == 0x05);
  assert(image.read(1) == replacement);
  std::filesystem::remove(path);
}

void test_machine_owns_disk_connection() {
  const auto path = (std::filesystem::temp_directory_path() / "project-oberon-machine.img").string();
  oberon::DiskImage image;
  image.create(path, 1);
  image.close();
  oberon::Machine machine;
  machine.open_disk(path, true);
  assert(machine.has_disk());
  assert(machine.memory().has_disk());
  machine.close_disk();
  assert(!machine.has_disk());
  assert(!machine.memory().has_disk());
  std::filesystem::remove(path);
}

void test_machine_advances_time_with_cpu() {
  oberon::Machine machine;
  std::array<std::uint32_t, Memory::RomWords> rom{};
  rom[0] = mov_imm(1, 7);
  machine.memory().load_rom_words(rom);
  machine.step();
  assert(machine.cpu().steps() == 1);
  assert(machine.memory().read32(Memory::IoBase) == 1);
}
}

int main() { test_arithmetic_and_flags(); test_branch_and_link(); test_memory_word_and_byte_access(); test_mmio_devices(); test_disk_image(); test_spi_disk_read_write(); test_machine_owns_disk_connection(); test_machine_advances_time_with_cpu(); std::cout << "risc5-tests: all tests passed\n"; }
