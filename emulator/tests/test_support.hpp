#pragma once

#include "disk_image.hpp"
#include "display.hpp"
#include "risc5.hpp"
#include "risc5_constants.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>

namespace test_support {

using oberon::Memory;
using oberon::Risc5;
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

inline Rom rom_with(std::initializer_list<std::uint32_t> words) {
  Rom rom{};
  std::size_t index = 0;
  for (const auto word : words) rom[index++] = word;
  return rom;
}

} // namespace test_support
