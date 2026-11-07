#pragma once

#include "devices.hpp"
#include "risc5_constants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace oberon {

class MemoryBus {
public:
  static constexpr std::uint32_t AddressMask = 0x00ff'ffffu;
  virtual ~MemoryBus() = default;

  virtual std::uint8_t read8(std::uint32_t address) = 0;
  virtual std::uint32_t read32(std::uint32_t address) = 0;
  virtual std::uint32_t read_instruction(std::uint32_t address) = 0;
  virtual void write8(std::uint32_t address, std::uint8_t value) = 0;
  virtual void write32(std::uint32_t address, std::uint32_t value) = 0;
};

class Memory final : public MemoryBus {
public:
  static constexpr std::uint32_t RomBase = 0x00ff'c000u;
  static constexpr std::uint32_t IoBase = DeviceBus::IoBase;
  static constexpr std::size_t RomWords = 512;

  explicit Memory(std::size_t ram_bytes = 1u << 20);

  std::uint8_t read8(std::uint32_t address) override;
  std::uint32_t read32(std::uint32_t address) override;
  std::uint32_t read_instruction(std::uint32_t address) override;
  void write8(std::uint32_t address, std::uint8_t value) override;
  void write32(std::uint32_t address, std::uint32_t value) override;

  void clear();
  void load_rom_words(const std::array<std::uint32_t, RomWords>& words);
  void load_rom_file(const std::string& path);

  // Functional-mode device hook. Machine advances this once per instruction;
  // direct callers may use it to drive devices without a CPU.
  void advance_time();
  void enqueue_uart_rx(std::uint8_t value);
  void enqueue_keyboard(std::uint8_t value);
  std::string take_uart_output();
  void attach_disk(DiskImage& disk) { devices_.attach_disk(disk); }
  void detach_disk() { devices_.detach_disk(); }
  bool has_disk() const { return devices_.has_disk(); }
  std::uint32_t leds() const { return devices_.leds(); }

  std::size_t ram_size() const { return ram_.size(); }

private:
  bool is_rom(std::uint32_t address) const;
  bool is_mmio(std::uint32_t address) const;
  std::size_t ram_index(std::uint32_t address) const;

  std::vector<std::uint8_t> ram_;
  std::array<std::uint32_t, RomWords> rom_{};
  DeviceBus devices_;
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
  struct Instruction {
    std::uint32_t raw;
    bool p;
    bool q;
    bool u;
    bool v;
    unsigned a;
    unsigned b;
    unsigned op;
    unsigned c;
    std::uint32_t imm;
  };

  static Instruction decode(std::uint32_t raw);
  static std::int32_t sign_extend(std::uint32_t value, unsigned bits);
  bool condition(std::uint32_t instruction) const;
  void execute_register(const Instruction& instruction);
  void execute_memory(const Instruction& instruction);
  void execute_branch(const Instruction& instruction, std::uint32_t next_pc);
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

class Machine {
public:
  explicit Machine(std::size_t ram_bytes = 1u << 20);

  Memory& memory() { return memory_; }
  const Memory& memory() const { return memory_; }
  Risc5& cpu() { return cpu_; }
  const Risc5& cpu() const { return cpu_; }

  void open_disk(const std::string& path, bool writable = true);
  void close_disk();
  bool has_disk() const { return disk_.has_value(); }
  void step();
  void run(std::uint64_t steps);

private:
  Memory memory_;
  Risc5 cpu_;
  std::optional<DiskImage> disk_;
};

} // namespace oberon
