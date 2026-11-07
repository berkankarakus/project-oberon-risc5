#pragma once

#include "disk_image.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace oberon {

class MemoryBus {
public:
  static constexpr std::uint32_t AddressMask = 0x00ff'ffffu;
  virtual ~MemoryBus() = default;

  virtual std::uint8_t read8(std::uint32_t address) const = 0;
  virtual std::uint32_t read32(std::uint32_t address) const = 0;
  virtual std::uint32_t read_instruction(std::uint32_t address) const = 0;
  virtual void write8(std::uint32_t address, std::uint8_t value) = 0;
  virtual void write32(std::uint32_t address, std::uint32_t value) = 0;
  virtual void tick() = 0;
};

class Memory final : public MemoryBus {
public:
  static constexpr std::uint32_t RomBase = 0x00ff'c000u;
  static constexpr std::uint32_t IoBase = 0x00ff'ffc0u;
  static constexpr std::size_t RomWords = 512;

  explicit Memory(std::size_t ram_bytes = 1u << 20);

  std::uint8_t read8(std::uint32_t address) const override;
  std::uint32_t read32(std::uint32_t address) const override;
  std::uint32_t read_instruction(std::uint32_t address) const override;
  void write8(std::uint32_t address, std::uint8_t value) override;
  void write32(std::uint32_t address, std::uint32_t value) override;

  void clear();
  void load_rom_words(const std::array<std::uint32_t, RomWords>& words);
  void load_rom_file(const std::string& path);

  // Functional-mode device hooks. One interpreter step advances the
  // emulated millisecond counter by one tick; cycle-accurate timing will be
  // added when the cycle runner is introduced.
  void tick() override;
  void enqueue_uart_rx(std::uint8_t value);
  void enqueue_keyboard(std::uint8_t value);
  std::string take_uart_output();
  void attach_disk(DiskImage* disk) { disk_ = disk; }
  bool has_disk() const { return disk_ != nullptr; }
  std::uint32_t leds() const { return leds_; }

  std::size_t ram_size() const { return ram_.size(); }

private:
  bool is_rom(std::uint32_t address) const;
  bool is_mmio(std::uint32_t address) const;
  std::uint32_t read_mmio(std::uint32_t address) const;
  void write_mmio(std::uint32_t address, std::uint32_t value);
  std::uint8_t spi_exchange_byte(std::uint8_t value);
  std::uint32_t spi_exchange_word(std::uint32_t value);
  void spi_command_complete();
  std::size_t ram_index(std::uint32_t address) const;

  std::vector<std::uint8_t> ram_;
  std::array<std::uint32_t, RomWords> rom_{};
  std::uint32_t milliseconds_ = 0;
  std::uint8_t leds_ = 0;
  std::uint8_t gpio_out_ = 0;
  std::uint8_t gpio_control_ = 0;
  std::uint8_t spi_control_ = 0;
  std::uint8_t uart_control_ = 0;
  mutable std::uint32_t spi_rx_ = 0xffff'ffffu;
  bool spi_selected_ = false;
  bool spi_app_command_ = false;
  bool spi_write_active_ = false;
  std::uint32_t spi_write_sector_ = 0;
  std::size_t spi_write_offset_ = 0;
  DiskImage::Sector spi_write_buffer_{};
  std::array<std::uint8_t, 6> spi_command_{};
  std::size_t spi_command_size_ = 0;
  mutable std::deque<std::uint8_t> spi_response_;
  DiskImage* disk_ = nullptr;
  mutable std::deque<std::uint8_t> uart_rx_;
  mutable std::deque<std::uint8_t> keyboard_;
  std::string uart_output_;
};

class Risc5 {
public:
  static constexpr std::uint32_t ResetPc = 0x003f'f800u;

  explicit Risc5(MemoryBus& memory);

  void reset();
  void step();

  std::uint32_t pc_word() const { return pc_; }
  std::uint32_t pc_byte() const { return (pc_ << 2) & MemoryBus::AddressMask; }
  std::uint32_t reg(std::size_t index) const { return regs_.at(index & 31u); }
  std::uint32_t high() const { return high_; }
  bool negative() const { return n_; }
  bool zero() const { return z_; }
  bool carry() const { return c_; }
  bool overflow() const { return ov_; }
  std::uint64_t steps() const { return steps_; }

private:
  static std::int32_t sign_extend(std::uint32_t value, unsigned bits);
  bool condition(std::uint32_t instruction) const;
  void update_nz(std::uint32_t value);
  void update_add_flags(std::uint32_t left, std::uint32_t right,
                        std::uint32_t result, bool carry_in);
  void update_sub_flags(std::uint32_t left, std::uint32_t right,
                        std::uint32_t result, bool borrow_in);
  void write_reg(unsigned index, std::uint32_t value);

  MemoryBus& memory_;
  std::array<std::uint32_t, 32> regs_{};
  std::uint32_t pc_ = ResetPc;
  std::uint32_t high_ = 0;
  bool n_ = false;
  bool z_ = false;
  bool c_ = false;
  bool ov_ = false;
  std::uint64_t steps_ = 0;
};

} // namespace oberon
