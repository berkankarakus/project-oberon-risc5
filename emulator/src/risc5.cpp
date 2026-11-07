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
  const auto masked = address & AddressMask;
  return masked >= IoBase && masked < IoBase + 0x28u;
}

std::uint32_t Memory::read_mmio(std::uint32_t address) const {
  switch ((address - IoBase) & 0x3fu) {
  case 0x00: return milliseconds_;
  case 0x04: return leds_;
  case 0x08: {
    const auto value = uart_rx_.empty() ? 0u : uart_rx_.front();
    if (!uart_rx_.empty()) uart_rx_.pop_front();
    return value;
  }
  case 0x0c:
    return (uart_rx_.empty() ? 0u : 1u) | (1u << 1); // RX ready, TX ready
  case 0x10: {
    const auto value = spi_rx_;
    spi_rx_ = 0xffff'ffffu;
    return value;
  }
  case 0x14: return 1u; // SPI ready
  case 0x18:
    return (keyboard_.empty() ? 0u : 1u) << 28; // keyboard ready in bit 28
  case 0x1c: {
    const auto value = keyboard_.empty() ? 0u : keyboard_.front();
    if (!keyboard_.empty()) keyboard_.pop_front();
    return value;
  }
  case 0x20: return gpio_out_;
  case 0x24: return gpio_control_;
  default: return 0;
  }
}

void Memory::write_mmio(std::uint32_t address, std::uint32_t value) {
  switch ((address - IoBase) & 0x3fu) {
  case 0x04: leds_ = static_cast<std::uint8_t>(value); break;
  case 0x08: uart_output_.push_back(static_cast<char>(value & 0xffu)); break;
  case 0x0c: uart_control_ = static_cast<std::uint8_t>(value); break;
  case 0x10:
    spi_rx_ = spi_exchange_word(value);
    break;
  case 0x14: {
    const auto next = static_cast<std::uint8_t>(value);
    spi_selected_ = (next & 1u) != 0;
    if (!spi_selected_) {
      spi_command_size_ = 0;
      spi_response_.clear();
      spi_write_active_ = false;
    }
    spi_control_ = next;
    break;
  }
  case 0x20: gpio_out_ = static_cast<std::uint8_t>(value); break;
  case 0x24: gpio_control_ = static_cast<std::uint8_t>(value); break;
  default: break;
  }
}

std::uint8_t Memory::read8(std::uint32_t address) const {
  address &= AddressMask;
  if (is_mmio(address)) {
    return static_cast<std::uint8_t>(read_mmio(address & ~3u) >> ((address & 3u) * 8u));
  }
  if (is_rom(address)) {
    const auto word = rom_[(address >> 2) & (RomWords - 1u)];
    return static_cast<std::uint8_t>((word >> ((address & 3u) * 8u)) & 0xffu);
  }
  return ram_[ram_index(address)];
}

std::uint32_t Memory::read32(std::uint32_t address) const {
  address &= AddressMask;
  if (is_mmio(address)) return read_mmio(address & ~3u);
  if (is_rom(address)) return rom_[(address >> 2) & (RomWords - 1u)];
  return static_cast<std::uint32_t>(read8(address)) |
         (static_cast<std::uint32_t>(read8(address + 1)) << 8) |
         (static_cast<std::uint32_t>(read8(address + 2)) << 16) |
         (static_cast<std::uint32_t>(read8(address + 3)) << 24);
}

std::uint32_t Memory::read_instruction(std::uint32_t address) const {
  address &= AddressMask;
  if (is_rom(address)) return rom_[(address >> 2) & (RomWords - 1u)];
  return read32(address);
}

void Memory::write8(std::uint32_t address, std::uint8_t value) {
  address &= AddressMask;
  if (is_mmio(address)) {
    const auto aligned = address & ~3u;
    const auto shift = (address & 3u) * 8u;
    const auto old = read_mmio(aligned);
    write_mmio(aligned, (old & ~(0xffu << shift)) | (static_cast<std::uint32_t>(value) << shift));
    return;
  }
  if (is_rom(address)) return;
  ram_[ram_index(address)] = value;
}

void Memory::write32(std::uint32_t address, std::uint32_t value) {
  address &= AddressMask;
  if (is_mmio(address)) {
    write_mmio(address & ~3u, value);
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
  milliseconds_ = 0;
  leds_ = gpio_out_ = gpio_control_ = spi_control_ = uart_control_ = 0;
  uart_rx_.clear();
  keyboard_.clear();
  uart_output_.clear();
  spi_rx_ = 0xff;
  spi_selected_ = false;
  spi_app_command_ = false;
  spi_write_active_ = false;
  spi_write_sector_ = 0;
  spi_write_offset_ = 0;
  spi_command_.fill(0);
  spi_command_size_ = 0;
  spi_response_.clear();
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

void Memory::tick() { ++milliseconds_; }

void Memory::enqueue_uart_rx(std::uint8_t value) { uart_rx_.push_back(value); }

void Memory::enqueue_keyboard(std::uint8_t value) { keyboard_.push_back(value); }

std::string Memory::take_uart_output() {
  auto output = uart_output_;
  uart_output_.clear();
  return output;
}

std::uint8_t Memory::spi_exchange_byte(std::uint8_t value) {
  if (!spi_selected_ || disk_ == nullptr) return 0xff;
  if (!spi_response_.empty()) {
    const auto response = spi_response_.front();
    spi_response_.pop_front();
    return response;
  }

  if (spi_write_active_) {
    if (spi_write_offset_ == 0) {
      if (value == 0xfe) spi_write_offset_ = 1;
      return 0xff;
    }
    if (spi_write_offset_ <= DiskImage::SectorSize) {
      spi_write_buffer_[spi_write_offset_ - 1] = value;
      ++spi_write_offset_;
      return 0xff;
    }
    if (spi_write_offset_ == DiskImage::SectorSize + 1) {
      ++spi_write_offset_; // first CRC byte
      return 0xff;
    }
    disk_->write(spi_write_sector_, spi_write_buffer_);
    spi_write_active_ = false;
    spi_write_offset_ = 0;
    return 0x05; // data accepted
  }

  if (spi_command_size_ == 0 && value == 0xff) return 0xff;
  spi_command_[spi_command_size_++] = value;
  if (spi_command_size_ == spi_command_.size()) spi_command_complete();
  return 0xff;
}

std::uint32_t Memory::spi_exchange_word(std::uint32_t value) {
  if ((spi_control_ & 4u) == 0) return spi_exchange_byte(static_cast<std::uint8_t>(value));
  std::uint32_t result = 0;
  for (unsigned i = 0; i < 4; ++i) {
    result |= static_cast<std::uint32_t>(spi_exchange_byte(static_cast<std::uint8_t>(value >> (i * 8)))) << (i * 8);
  }
  return result;
}

void Memory::spi_command_complete() {
  const auto command = spi_command_[0] & 0x3f;
  const auto argument = (static_cast<std::uint32_t>(spi_command_[1]) << 24) |
                        (static_cast<std::uint32_t>(spi_command_[2]) << 16) |
                        (static_cast<std::uint32_t>(spi_command_[3]) << 8) |
                        spi_command_[4];
  spi_command_size_ = 0;
  switch (command) {
  case 0: spi_response_.push_back(0x01); break; // CMD0: idle
  case 8: spi_response_ = {0x01, 0, 0, 1, 0xaa}; break; // CMD8
  case 55: spi_app_command_ = true; spi_response_.push_back(0x01); break;
  case 41:
    spi_response_.push_back(spi_app_command_ ? 0x00 : 0x05);
    spi_app_command_ = false;
    break;
  case 58: spi_response_ = {0x00, 0x40, 0xff, 0x80, 0x00}; break; // SDHC OCR
  case 17: {
    spi_response_.push_back(0x00);
    const auto sector = argument;
    const auto data = disk_->read(sector < disk_->sector_count() ? sector : 0);
    spi_response_.push_back(0xfe);
    spi_response_.insert(spi_response_.end(), data.begin(), data.end());
    spi_response_.push_back(0xff);
    spi_response_.push_back(0xff);
    break;
  }
  case 24:
    spi_response_.push_back(0x00);
    spi_write_active_ = true;
    spi_write_sector_ = argument < disk_->sector_count() ? argument : 0;
    spi_write_offset_ = 0;
    spi_write_buffer_.fill(0);
    break;
  default: spi_response_.push_back(0x04); break;
  }
}

Risc5::Risc5(Memory& memory) : memory_(memory) { reset(); }

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
    // In format-0/1 instructions `a` is the destination, while `b` and `c`
    // select the two source operands (see Registers.v's rno0/rno1/rno2).
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
    const auto address = (regs_[b] + static_cast<std::uint32_t>(sign_extend(instruction & 0x000f'ffffu, 20))) & Memory::AddressMask;
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
