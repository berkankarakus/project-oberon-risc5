#include "test_support.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

namespace {
using namespace test_support;

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
  memory.set_mouse(123, 456, 5);
  assert((memory.read32(mmio(MmioRegister::KeyboardStatus)) & 0x0fff'ffffu) == ((5u << 24) | (456u << 12) | 123u));
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
  std::filesystem::remove(invalid_path);
  assert(rejected_partial_sector);

  oberon::DiskImage image;
  image.create(path, 4);
  assert(image.sector_count() == 4);
  oberon::DiskImage::Sector sector{};
  for (std::size_t i = 0; i < sector.size(); ++i) sector[i] = static_cast<std::uint8_t>(i ^ 0xa5u);
  image.write(2, sector);
  assert(image.read(2) == sector);

  bool rejected_read_out_of_range = false;
  try { image.read(image.sector_count()); } catch (const std::out_of_range&) { rejected_read_out_of_range = true; }
  assert(rejected_read_out_of_range);
  bool rejected_write_out_of_range = false;
  try { image.write(image.sector_count(), sector); } catch (const std::out_of_range&) { rejected_write_out_of_range = true; }
  assert(rejected_write_out_of_range);
  image.close();

  oberon::DiskImage read_only;
  read_only.open(path, false);
  assert(!read_only.writable());
  bool rejected_read_only_write = false;
  try { read_only.write(0, sector); } catch (const std::runtime_error&) { rejected_read_only_write = true; }
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

void test_spi_data_read_is_non_destructive() {
  Memory memory;
  memory.write32(mmio(MmioRegister::SpiControl), 1);
  memory.write32(mmio(MmioRegister::SpiData), 0xff);
  const auto first = memory.read32(mmio(MmioRegister::SpiData));
  assert(memory.read32(mmio(MmioRegister::SpiData)) == first);
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

void test_machine_timer_interrupt_delivery() {
  oberon::Machine machine;
  const auto enable_interrupts = 0xc700'0021u;
  const auto return_from_interrupt = 0xc700'0010u;
  machine.memory().load_rom_words(rom_with({enable_interrupts}));
  machine.memory().write32(4, return_from_interrupt);
  machine.run(oberon::Machine::TimerInterruptPeriod);
  assert(machine.memory().read32(mmio(MmioRegister::Timer)) == oberon::Machine::TimerInterruptPeriod);
  assert(machine.cpu().pc_word() != 1);
  machine.step();
  assert(machine.cpu().pc_word() == 1);
  machine.step();
  assert(machine.cpu().pc_word() == Risc5::ResetPc + oberon::Machine::TimerInterruptPeriod + 1);
}

} // namespace

int main() {
  using Test = void (*)();
  const std::array<std::pair<const char*, Test>, 8> tests = {
      std::pair{"MMIO devices", &test_mmio_devices},
      std::pair{"display memory layout", &test_display_memory_layout},
      std::pair{"disk image", &test_disk_image},
      std::pair{"SPI disk read/write", &test_spi_disk_read_write},
      std::pair{"SPI data read semantics", &test_spi_data_read_is_non_destructive},
      std::pair{"machine disk ownership", &test_machine_owns_disk_connection},
      std::pair{"machine timing", &test_machine_advances_time_with_cpu},
      std::pair{"machine timer interrupt delivery", &test_machine_timer_interrupt_delivery},
  };
  for (const auto& [name, test] : tests) {
    test();
    std::cout << "[ok] " << name << '\n';
  }
  std::cout << "device-tests: all tests passed\n";
}
