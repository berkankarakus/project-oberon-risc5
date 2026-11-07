#include "risc5.hpp"

#include <bit>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace oberon {

Memory::Memory(std::size_t ram_bytes) : ram_(ram_bytes, 0) {
  if (ram_.empty()) throw std::invalid_argument("RISC5 RAM must not be empty");
}

bool Memory::is_rom(std::uint32_t address) const {
  return ((address & AddressMask) >> 14) == 0x3ffu;
}

std::size_t Memory::ram_index(std::uint32_t address) const {
  return static_cast<std::size_t>((address & AddressMask) % ram_.size());
}

bool Memory::is_mmio(std::uint32_t address) const {
  return devices_.handles(address);
}

std::uint8_t Memory::read8(std::uint32_t address) {
  address &= AddressMask;
  if (is_mmio(address)) {
    return static_cast<std::uint8_t>(devices_.read(address & ~3u) >> ((address & 3u) * 8u));
  }
  if (is_rom(address)) {
    const auto word = rom_[(address >> 2) & (RomWords - 1u)];
    return static_cast<std::uint8_t>((word >> ((address & 3u) * 8u)) & 0xffu);
  }
  return ram_[ram_index(address)];
}

std::uint32_t Memory::read32(std::uint32_t address) {
  address &= AddressMask;
  if (is_mmio(address)) return devices_.read(address & ~3u);
  if (is_rom(address)) return rom_[(address >> 2) & (RomWords - 1u)];
  return static_cast<std::uint32_t>(read8(address)) |
         (static_cast<std::uint32_t>(read8(address + 1)) << 8) |
         (static_cast<std::uint32_t>(read8(address + 2)) << 16) |
         (static_cast<std::uint32_t>(read8(address + 3)) << 24);
}

std::uint32_t Memory::read_instruction(std::uint32_t address) {
  address &= AddressMask;
  if (is_rom(address)) return rom_[(address >> 2) & (RomWords - 1u)];
  return read32(address);
}

void Memory::write8(std::uint32_t address, std::uint8_t value) {
  address &= AddressMask;
  if (is_mmio(address)) {
    const auto aligned = address & ~3u;
    const auto shift = (address & 3u) * 8u;
    const auto old = devices_.read(aligned);
    devices_.write(aligned, (old & ~(0xffu << shift)) | (static_cast<std::uint32_t>(value) << shift));
    return;
  }
  if (is_rom(address)) return;
  ram_[ram_index(address)] = value;
}

void Memory::write32(std::uint32_t address, std::uint32_t value) {
  address &= AddressMask;
  if (is_mmio(address)) {
    devices_.write(address & ~3u, value);
    return;
  }
  if (is_rom(address)) return;
  write8(address, static_cast<std::uint8_t>(value));
  write8(address + 1, static_cast<std::uint8_t>(value >> 8));
  write8(address + 2, static_cast<std::uint8_t>(value >> 16));
  write8(address + 3, static_cast<std::uint8_t>(value >> 24));
}

void Memory::clear() {
  std::fill(ram_.begin(), ram_.end(), 0);
  rom_.fill(0);
  devices_.reset();
}

void Memory::load_rom_words(const std::array<std::uint32_t, RomWords>& words) {
  rom_ = words;
}

void Memory::load_rom_file(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open ROM file: " + path);
  rom_.fill(0);
  std::string line;
  std::size_t index = 0;
  while (std::getline(input, line)) {
    if (index >= RomWords) throw std::runtime_error("ROM file contains more than 512 words: " + path);
    const auto comment = line.find_first_of("#;");
    if (comment != std::string::npos) line.resize(comment);
    std::istringstream parser(line);
    std::uint32_t word = 0;
    parser >> std::hex >> word;
    if (!parser.fail()) rom_[index++] = word;
  }
}

void Memory::tick() { devices_.tick(); }

void Memory::enqueue_uart_rx(std::uint8_t value) { devices_.enqueue_uart_rx(value); }

void Memory::enqueue_keyboard(std::uint8_t value) { devices_.enqueue_keyboard(value); }

std::string Memory::take_uart_output() { return devices_.take_uart_output(); }

Risc5::Risc5(MemoryBus& memory) : memory_(memory) { reset(); }

void Risc5::reset() {
  regs_.fill(0);
  pc_ = ResetPc;
  high_ = 0;
  n_ = z_ = c_ = ov_ = false;
  steps_ = 0;
}

std::int32_t Risc5::sign_extend(std::uint32_t value, unsigned bits) {
  const auto shift = 32u - bits;
  return static_cast<std::int32_t>(value << shift) >> shift;
}

bool Risc5::condition(std::uint32_t instruction) const {
  const auto cc = (instruction >> 24) & 7u;
  const bool invert = ((instruction >> 27) & 1u) != 0;
  const bool s = n_ ^ ov_;
  bool result = false;
  switch (cc) {
  case 0: result = n_; break;
  case 1: result = z_; break;
  case 2: result = c_; break;
  case 3: result = ov_; break;
  case 4: result = c_ || z_; break;
  case 5: result = s; break;
  case 6: result = s || z_; break;
  case 7: result = true; break;
  }
  return invert ^ result;
}

void Risc5::update_nz(std::uint32_t value) {
  n_ = (value & 0x8000'0000u) != 0;
  z_ = value == 0;
}

void Risc5::update_add_flags(std::uint32_t left, std::uint32_t right,
                             std::uint32_t result, bool carry_in) {
  const auto wide = static_cast<std::uint64_t>(left) + right + (carry_in ? 1u : 0u);
  c_ = (wide >> 32) != 0;
  ov_ = ((~(left ^ right) & (left ^ result)) & 0x8000'0000u) != 0;
}

void Risc5::update_sub_flags(std::uint32_t left, std::uint32_t right,
                             std::uint32_t result, bool borrow_in) {
  const auto subtrahend = static_cast<std::uint64_t>(right) + (borrow_in ? 1u : 0u);
  c_ = static_cast<std::uint64_t>(left) >= subtrahend;
  ov_ = (((left ^ right) & (left ^ result)) & 0x8000'0000u) != 0;
}

void Risc5::write_reg(unsigned index, std::uint32_t value) {
  regs_[index & 15u] = value;
}

void Risc5::step() {
  const auto instruction = memory_.read_instruction(pc_byte());
  const auto next_pc = (pc_ + 1u) & 0x003f'ffffu;
  const bool p = (instruction & 0x8000'0000u) != 0;
  const bool q = (instruction & 0x4000'0000u) != 0;
  const bool u = (instruction & 0x2000'0000u) != 0;
  const bool v = (instruction & 0x1000'0000u) != 0;
  const unsigned a = (instruction >> 24) & 0x0fu;
  const unsigned b = (instruction >> 20) & 0x0fu;
  const unsigned op = (instruction >> 16) & 0x0fu;
  const unsigned c = instruction & 0x0fu;
  const auto imm = instruction & 0xffffu;
  pc_ = next_pc;

  if (!p) {
    const auto left = regs_[b];
    const auto operand = q ? (v ? static_cast<std::uint32_t>(sign_extend(imm, 16)) : imm) : regs_[c];
    std::uint32_t result = 0;
    bool writes = true;
    switch (op) {
    case 0: result = q ? (u ? (imm << 16) : static_cast<std::uint32_t>(sign_extend(imm, 16))) : regs_[c]; break;
    case 1: result = left << (operand & 31u); break;
    case 2: result = static_cast<std::uint32_t>(static_cast<std::int32_t>(left) >> (operand & 31u)); break;
    case 3: { const auto shift = operand & 31u; result = shift == 0 ? left : (left >> shift) | (left << (32u - shift)); break; }
    case 4: result = left & operand; break;
    case 5: result = left & ~operand; break;
    case 6: result = left | operand; break;
    case 7: result = left ^ operand; break;
    case 8: result = left + operand + (u && c_ ? 1u : 0u); update_add_flags(left, operand, result, u && c_); break;
    case 9: result = left - operand - (u && c_ ? 1u : 0u); update_sub_flags(left, operand, result, u && c_); break;
    case 10:
      if (u) {
        const auto product = static_cast<std::int64_t>(static_cast<std::int32_t>(left)) * static_cast<std::int64_t>(static_cast<std::int32_t>(operand));
        const auto bits = static_cast<std::uint64_t>(product); result = static_cast<std::uint32_t>(bits); high_ = static_cast<std::uint32_t>(bits >> 32);
      } else { const auto product = static_cast<std::uint64_t>(left) * operand; result = static_cast<std::uint32_t>(product); high_ = static_cast<std::uint32_t>(product >> 32); }
      break;
    case 11:
      if (operand == 0) { result = std::numeric_limits<std::uint32_t>::max(); high_ = left; }
      else if (u) { const auto quotient = static_cast<std::int32_t>(left) / static_cast<std::int32_t>(operand); const auto remainder = static_cast<std::int32_t>(left) % static_cast<std::int32_t>(operand); result = static_cast<std::uint32_t>(quotient); high_ = static_cast<std::uint32_t>(remainder); }
      else { result = left / operand; high_ = left % operand; }
      break;
    case 12: result = std::bit_cast<std::uint32_t>(std::bit_cast<float>(left) + std::bit_cast<float>(operand)); break;
    case 13: result = std::bit_cast<std::uint32_t>(std::bit_cast<float>(left) - std::bit_cast<float>(operand)); break;
    case 14: result = std::bit_cast<std::uint32_t>(std::bit_cast<float>(left) * std::bit_cast<float>(operand)); break;
    case 15: result = std::bit_cast<std::uint32_t>(std::bit_cast<float>(left) / std::bit_cast<float>(operand)); break;
    default: writes = false; break;
    }
    if (writes) { write_reg(a, result); update_nz(result); }
    memory_.tick();
    ++steps_;
    return;
  }

  if (!q) {
    const auto address = (regs_[b] + static_cast<std::uint32_t>(sign_extend(instruction & 0x000f'ffffu, 20))) & MemoryBus::AddressMask;
    if (u) {
      if (v) { const auto shift = (address & 3u) * 8u; const auto mask = 0xffu << shift; const auto old = memory_.read32(address & ~3u); memory_.write32(address & ~3u, (old & ~mask) | ((regs_[a] & 0xffu) << shift)); }
      else memory_.write32(address, regs_[a]);
    } else {
      auto value = memory_.read32(address & ~3u);
      if (v) value = (value >> ((address & 3u) * 8u)) & 0xffu;
      write_reg(a, value); update_nz(value);
    }
    ++steps_;
    memory_.tick();
    return;
  }

  const bool taken = condition(instruction);
  if (taken) {
    const auto displacement = sign_extend(instruction & 0x003f'ffffu, 22);
    pc_ = u ? static_cast<std::uint32_t>(next_pc + displacement) & 0x003f'ffffu : (regs_[c] >> 2) & 0x003f'ffffu;
    if (v) { const auto link = next_pc << 2; write_reg(15, link); update_nz(link); }
  }
  ++steps_;
  memory_.tick();
}

} // namespace oberon
