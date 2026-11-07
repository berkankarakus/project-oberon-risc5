#include "risc5.hpp"
#include "disk_image.hpp"
#include "display.hpp"
#include "risc5_constants.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <utility>

using oberon::Memory;
using oberon::Risc5;

namespace {
using oberon::AluOp;
using oberon::ConditionCode;
using oberon::MmioRegister;
using oberon::instruction_bits::BranchDisplacementMask;
using oberon::instruction_bits::ImmediateMask;
using oberon::instruction_bits::P;
using oberon::instruction_bits::Q;
using oberon::instruction_bits::U;
using oberon::instruction_bits::V;

constexpr std::uint32_t mmio(MmioRegister reg) {
  return Memory::IoBase + static_cast<std::uint32_t>(reg);
}

constexpr std::uint32_t mov_imm(unsigned a, std::uint16_t value, bool negative = false) {
  return Q | (a << 24) | (negative ? V : 0u) | value;
}

constexpr std::uint32_t alu(AluOp op, unsigned a, unsigned b, unsigned c) {
  return (a << 24) | (b << 20) | (static_cast<unsigned>(op) << 16) | c;
}

constexpr std::uint32_t memory_op(bool store, bool byte, unsigned a, unsigned b, std::uint32_t offset) {
  return P | (store ? U : 0u) | (byte ? V : 0u) | (a << 24) | (b << 20) | (offset & ImmediateMask);
}

constexpr std::uint32_t branch_relative(ConditionCode condition, std::int32_t displacement, bool link = false) {
  return P | Q | U | (link ? V : 0u) |
         (static_cast<std::uint32_t>(condition) << 24) |
         (static_cast<std::uint32_t>(displacement) & BranchDisplacementMask);
}

using Rom = std::array<std::uint32_t, Memory::RomWords>;

Rom rom_with(std::initializer_list<std::uint32_t> words) {
  Rom rom{};
  std::size_t index = 0;
  for (const auto word : words) rom[index++] = word;
  return rom;
}

void test_arithmetic_and_flags() {
  Memory memory;
  memory.load_rom_words(rom_with({mov_imm(1, 7), mov_imm(2, 5), alu(AluOp::Add, 3, 1, 2)}));
  Risc5 cpu(memory);
  cpu.step();
  cpu.step();
  cpu.step();
  assert(cpu.reg(3) == 12);
  assert(!cpu.zero());
  assert(!cpu.negative());
}

void test_branch_and_link() {
  Memory memory;
  memory.load_rom_words(rom_with({branch_relative(ConditionCode::Always, 2, true), mov_imm(1, 0xdead), mov_imm(1, 0xbeef), mov_imm(2, 42)}));
  Risc5 cpu(memory);
  cpu.step();
  cpu.step();
  assert(cpu.reg(1) == 0);
  assert(cpu.reg(2) == 42);
  assert(cpu.reg(15) == (Risc5::ResetPc + 1) * 4);
}

void test_memory_word_and_byte_access() {
  Memory memory;
  memory.load_rom_words(rom_with({mov_imm(1, 0x100), mov_imm(2, 0xaa), memory_op(true, true, 2, 1, 0x20), memory_op(false, true, 3, 1, 0x20)}));
  Risc5 cpu(memory);
  for (int i = 0; i < 4; ++i) cpu.step();
  assert(cpu.reg(3) == 0xaa);
}

void test_mmio_devices() {
  Memory memory;
  memory.advance_time();
  memory.advance_time();
  assert(memory.read32(mmio(MmioRegister::Timer)) == 2);
  memory.write32(mmio(MmioRegister::Leds), 0x5a);
  assert(memory.read32(mmio(MmioRegister::Leds)) == 0x5a);
  memory.write32(mmio(MmioRegister::UartData), 'O');
  memory.write32(mmio(MmioRegister::UartData), 'K');
  assert(memory.take_uart_output() == "OK");
  memory.enqueue_uart_rx('A');
  assert((memory.read32(mmio(MmioRegister::UartControl)) & 1u) != 0);
  assert(memory.read32(mmio(MmioRegister::UartData)) == 'A');
  assert((memory.read32(mmio(MmioRegister::UartControl)) & 1u) == 0);
  memory.enqueue_keyboard(0x1c);
  assert((memory.read32(mmio(MmioRegister::KeyboardStatus)) & 0x1000'0000u) != 0);
  assert(memory.read32(mmio(MmioRegister::KeyboardData)) == 0x1c);
  assert((memory.read32(mmio(MmioRegister::KeyboardStatus)) & 0x1000'0000u) == 0);
}

void test_display_memory_layout() {
  Memory memory;
  memory.write32(oberon::display::Base, 0x8000'0001u);
  assert(memory.read32(oberon::display::Base) == 0x8000'0001u);
  assert(oberon::display::Base + oberon::display::Height * oberon::display::RowBytes < memory.ram_size());
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
  memory.write32(mmio(MmioRegister::SpiControl), 1);
  const auto spi = [&](std::uint8_t byte) {
    memory.write32(mmio(MmioRegister::SpiData), byte);
    return static_cast<std::uint8_t>(memory.read32(mmio(MmioRegister::SpiData)));
  };
  for (const auto byte : {std::uint8_t(0x51), std::uint8_t(0), std::uint8_t(0), std::uint8_t(0), std::uint8_t(1), std::uint8_t(0xff)}) spi(byte);
  assert(spi(0xff) == 0x00);
  assert(spi(0xff) == 0xfe);
  for (const auto expected : source) assert(spi(0xff) == expected);
  assert(spi(0xff) == 0xff);
  assert(spi(0xff) == 0xff);

  memory.write32(mmio(MmioRegister::SpiControl), 1);
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
  machine.memory().load_rom_words(rom_with({mov_imm(1, 7)}));
  machine.step();
  assert(machine.cpu().steps() == 1);
  assert(machine.memory().read32(mmio(MmioRegister::Timer)) == 1);
}
}

int main() {
  using Test = void (*)();
  const std::array<std::pair<const char*, Test>, 9> tests = {
      std::pair{"arithmetic and flags", &test_arithmetic_and_flags},
      std::pair{"branch and link", &test_branch_and_link},
      std::pair{"memory word and byte access", &test_memory_word_and_byte_access},
      std::pair{"MMIO devices", &test_mmio_devices},
      std::pair{"display memory layout", &test_display_memory_layout},
      std::pair{"disk image", &test_disk_image},
      std::pair{"SPI disk read/write", &test_spi_disk_read_write},
      std::pair{"machine disk ownership", &test_machine_owns_disk_connection},
      std::pair{"machine timing", &test_machine_advances_time_with_cpu},
  };
  for (const auto& [name, test] : tests) {
    test();
    std::cout << "[ok] " << name << '\n';
  }
  std::cout << "risc5-tests: all tests passed\n";
}
