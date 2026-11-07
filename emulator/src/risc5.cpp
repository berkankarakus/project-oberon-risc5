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

bool Memory::is_rom(std::uint32_t address) const { return ((address & AddressMask) >> 14) == 0x3ffu; }

std::size_t Memory::ram_index(std::uint32_t address) const {
  return static_cast<std::size_t>((address & AddressMask) % ram_.size());
}

bool Memory::is_mmio(std::uint32_t address) const { return devices_.handles(address); }

std::uint8_t Memory::read8(std::uint32_t address) {
  address &= AddressMask;
  if (is_mmio(address)) return static_cast<std::uint8_t>(devices_.read(address & ~3u) >> ((address & 3u) * 8u));
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
  if (!is_rom(address)) ram_[ram_index(address)] = value;
}

void Memory::write32(std::uint32_t address, std::uint32_t value) {
  address &= AddressMask;
  if (is_mmio(address)) { devices_.write(address & ~3u, value); return; }
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

void Memory::load_rom_words(const std::array<std::uint32_t, RomWords>& words) { rom_ = words; }

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

void Memory::advance_time() { devices_.advance_time(); }
void Memory::enqueue_uart_rx(std::uint8_t value) { devices_.enqueue_uart_rx(value); }
void Memory::enqueue_keyboard(std::uint8_t value) { devices_.enqueue_keyboard(value); }
void Memory::set_mouse(std::uint16_t x, std::uint16_t y, std::uint8_t buttons) { devices_.set_mouse(x, y, buttons); }
std::string Memory::take_uart_output() { return devices_.take_uart_output(); }

Risc5::Risc5(MemoryBus& memory) : memory_(memory) { reset(); }

void Risc5::reset() {
  regs_.fill(0);
  pc_ = ResetPc;
  high_ = 0;
  n_ = z_ = c_ = ov_ = false;
  irq_level_ = false;
  interrupt_enabled_ = false;
  interrupt_pending_ = false;
  interrupt_mode_ = false;
  saved_pc_ = 0;
  saved_n_ = saved_z_ = saved_c_ = saved_ov_ = false;
  steps_ = 0;
}

Risc5::Instruction Risc5::decode(std::uint32_t raw) {
  return Instruction{raw, (raw & instruction_bits::P) != 0, (raw & instruction_bits::Q) != 0,
                     (raw & instruction_bits::U) != 0, (raw & instruction_bits::V) != 0,
                     (raw >> 24) & instruction_bits::RegisterMask,
                     (raw >> 20) & instruction_bits::RegisterMask,
                     (raw >> 16) & instruction_bits::RegisterMask,
                     raw & instruction_bits::RegisterMask, raw & instruction_bits::ImmediateMask};
}

std::int32_t Risc5::sign_extend(std::uint32_t value, unsigned bits) {
  const auto shift = 32u - bits;
  return static_cast<std::int32_t>(value << shift) >> shift;
}

bool Risc5::condition(std::uint32_t instruction) const {
  const auto cc = static_cast<ConditionCode>((instruction >> 24) & instruction_bits::ConditionMask);
  const bool invert = (instruction & instruction_bits::Invert) != 0;
  const bool s = n_ ^ ov_;
  bool result = false;
  switch (cc) {
  case ConditionCode::Mi: result = n_; break;
  case ConditionCode::Eq: result = z_; break;
  case ConditionCode::Cs: result = c_; break;
  case ConditionCode::Vs: result = ov_; break;
  case ConditionCode::Ls: result = c_ || z_; break;
  case ConditionCode::Lt: result = s; break;
  case ConditionCode::Le: result = s || z_; break;
  case ConditionCode::Always: result = true; break;
  }
  return invert ^ result;
}

void Risc5::update_nz(std::uint32_t value) {
  n_ = (value & instruction_bits::SignBit) != 0;
  z_ = value == 0;
}

void Risc5::update_add_flags(std::uint32_t left, std::uint32_t right,
                             std::uint32_t result, bool carry_in) {
  const auto wide = static_cast<std::uint64_t>(left) + right + (carry_in ? 1u : 0u);
  c_ = (wide >> 32) != 0;
  ov_ = ((~(left ^ right) & (left ^ result)) & instruction_bits::SignBit) != 0;
}

void Risc5::update_sub_flags(std::uint32_t left, std::uint32_t right,
                             std::uint32_t result, bool borrow_in) {
  const auto subtrahend = static_cast<std::uint64_t>(right) + (borrow_in ? 1u : 0u);
  c_ = static_cast<std::uint64_t>(left) < subtrahend;
  ov_ = (((left ^ right) & (left ^ result)) & instruction_bits::SignBit) != 0;
}

void Risc5::write_reg(unsigned index, std::uint32_t value) { regs_[index & 15u] = value; }

void Risc5::restore_interrupt_state() {
  pc_ = saved_pc_;
  n_ = saved_n_;
  z_ = saved_z_;
  c_ = saved_c_;
  ov_ = saved_ov_;
  interrupt_mode_ = false;
}

void Risc5::execute_register(const Instruction& instruction) {
  const auto left = regs_[instruction.b];
  const auto operand = instruction.q ? (instruction.v ? static_cast<std::uint32_t>(sign_extend(instruction.imm, 16)) : instruction.imm) : regs_[instruction.c];
  std::uint32_t result = 0;
  bool writes = true;
  switch (static_cast<AluOp>(instruction.op)) {
  case AluOp::Mov:
    if (instruction.q) result = instruction.u ? (instruction.imm << 16) : (instruction.v ? static_cast<std::uint32_t>(sign_extend(instruction.imm, 16)) : instruction.imm);
    else if (!instruction.u) result = regs_[instruction.c];
    else if (!instruction.v) result = high_;
    else result = (static_cast<std::uint32_t>(n_) << 31) | (static_cast<std::uint32_t>(z_) << 30) |
                  (static_cast<std::uint32_t>(c_) << 29) | (static_cast<std::uint32_t>(ov_) << 28) | 0x53u;
    break;
  case AluOp::Lsl: result = left << (operand & 31u); break;
  case AluOp::Asr: result = static_cast<std::uint32_t>(static_cast<std::int32_t>(left) >> (operand & 31u)); break;
  case AluOp::Ror: { const auto shift = operand & 31u; result = shift == 0 ? left : (left >> shift) | (left << (32u - shift)); break; }
  case AluOp::And: result = left & operand; break;
  case AluOp::Ann: result = left & ~operand; break;
  case AluOp::Ior: result = left | operand; break;
  case AluOp::Xor: result = left ^ operand; break;
  case AluOp::Add: result = left + operand + (instruction.u && c_ ? 1u : 0u); update_add_flags(left, operand, result, instruction.u && c_); break;
  case AluOp::Sub: result = left - operand - (instruction.u && c_ ? 1u : 0u); update_sub_flags(left, operand, result, instruction.u && c_); break;
  case AluOp::Mul:
    if (!instruction.u) { const auto product = static_cast<std::int64_t>(static_cast<std::int32_t>(left)) * static_cast<std::int64_t>(static_cast<std::int32_t>(operand)); const auto bits = static_cast<std::uint64_t>(product); result = static_cast<std::uint32_t>(bits); high_ = static_cast<std::uint32_t>(bits >> 32); }
    else { const auto product = static_cast<std::uint64_t>(left) * operand; result = static_cast<std::uint32_t>(product); high_ = static_cast<std::uint32_t>(product >> 32); }
    break;
  case AluOp::Div:
    if (operand == 0) { result = std::numeric_limits<std::uint32_t>::max(); high_ = left; }
    else if (instruction.u) { result = left / operand; high_ = left % operand; }
    else {
      const auto dividend = static_cast<std::int32_t>(left);
      const auto divisor = static_cast<std::int32_t>(operand);
      auto quotient = dividend / divisor;
      auto remainder = dividend % divisor;
      if (remainder < 0) { --quotient; remainder += divisor; }
      result = static_cast<std::uint32_t>(quotient);
      high_ = static_cast<std::uint32_t>(remainder);
    }
    break;
  case AluOp::Fad: result = std::bit_cast<std::uint32_t>(std::bit_cast<float>(left) + std::bit_cast<float>(operand)); break;
  case AluOp::Fsb: result = std::bit_cast<std::uint32_t>(std::bit_cast<float>(left) - std::bit_cast<float>(operand)); break;
  case AluOp::Fml: result = std::bit_cast<std::uint32_t>(std::bit_cast<float>(left) * std::bit_cast<float>(operand)); break;
  case AluOp::Fdv: result = std::bit_cast<std::uint32_t>(std::bit_cast<float>(left) / std::bit_cast<float>(operand)); break;
  default: writes = false; break;
  }
  if (writes) { write_reg(instruction.a, result); update_nz(result); }
}

void Risc5::execute_memory(const Instruction& instruction) {
  const auto address = (regs_[instruction.b] + static_cast<std::uint32_t>(sign_extend(instruction.raw & instruction_bits::MemoryOffsetMask, 20))) & MemoryBus::AddressMask;
  if (instruction.u) {
    if (instruction.v) { const auto shift = (address & 3u) * 8u; const auto mask = 0xffu << shift; const auto old = memory_.read32(address & ~3u); memory_.write32(address & ~3u, (old & ~mask) | ((regs_[instruction.a] & 0xffu) << shift)); }
    else memory_.write32(address, regs_[instruction.a]);
  } else {
    auto value = memory_.read32(address & ~3u);
    if (instruction.v) value = (value >> ((address & 3u) * 8u)) & 0xffu;
    write_reg(instruction.a, value); update_nz(value);
  }
}

void Risc5::execute_branch(const Instruction& instruction, std::uint32_t next_pc) {
  if (!condition(instruction.raw)) return;
  const auto displacement = sign_extend(instruction.raw & instruction_bits::BranchDisplacementMask, 22);
  pc_ = instruction.u ? static_cast<std::uint32_t>(next_pc + displacement) & instruction_bits::BranchDisplacementMask : (regs_[instruction.c] >> 2) & instruction_bits::BranchDisplacementMask;
  if (instruction.v) { const auto link = next_pc << 2; write_reg(15, link); update_nz(link); }
}

void Risc5::step() { step(false); }

void Risc5::step(bool irq) {
  const auto instruction = decode(memory_.read_instruction(pc_byte()));
  const auto next_pc = (pc_ + 1u) & instruction_bits::BranchDisplacementMask;
  const auto interrupt_ack = interrupt_pending_ && interrupt_enabled_ && !interrupt_mode_;
  const auto irq_rising = irq && !irq_level_;
  irq_level_ = irq;
  if (irq_rising) interrupt_pending_ = true;

  pc_ = next_pc;
  if (!instruction.p) execute_register(instruction);
  else if (!instruction.q) execute_memory(instruction);
  else if (instruction.u || instruction.v || (instruction.raw & 0x30u) == 0) {
    execute_branch(instruction, next_pc);
  } else {
    if ((instruction.raw & 0x10u) != 0 && interrupt_mode_) restore_interrupt_state();
    if ((instruction.raw & 0x20u) != 0) interrupt_enabled_ = (instruction.raw & 1u) != 0;
  }
  if (interrupt_ack) {
    saved_pc_ = pc_;
    saved_n_ = n_;
    saved_z_ = z_;
    saved_c_ = c_;
    saved_ov_ = ov_;
    pc_ = 1;
    interrupt_pending_ = false;
    interrupt_mode_ = true;
  }
  ++steps_;
}

} // namespace oberon
