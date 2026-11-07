#pragma once

#include <cstdint>

namespace oberon {

enum class AluOp : std::uint8_t {
  Mov = 0, Lsl = 1, Asr = 2, Ror = 3, And = 4, Ann = 5, Ior = 6, Xor = 7,
  Add = 8, Sub = 9, Mul = 10, Div = 11, Fad = 12, Fsb = 13, Fml = 14, Fdv = 15
};

enum class ConditionCode : std::uint8_t { Mi = 0, Eq = 1, Cs = 2, Vs = 3, Ls = 4, Lt = 5, Le = 6, Always = 7 };

enum class MmioRegister : std::uint32_t {
  Timer = 0x00, Leds = 0x04, UartData = 0x08, UartControl = 0x0c,
  SpiData = 0x10, SpiControl = 0x14, KeyboardStatus = 0x18,
  KeyboardData = 0x1c, GpioData = 0x20, GpioControl = 0x24
};

namespace instruction_bits {
constexpr std::uint32_t P = 0x8000'0000u;
constexpr std::uint32_t Q = 0x4000'0000u;
constexpr std::uint32_t U = 0x2000'0000u;
constexpr std::uint32_t V = 0x1000'0000u;
constexpr std::uint32_t Invert = 0x0800'0000u;
constexpr std::uint32_t ConditionMask = 0x07u;
constexpr std::uint32_t RegisterMask = 0x0fu;
constexpr std::uint32_t ImmediateMask = 0xffffu;
constexpr std::uint32_t MemoryOffsetMask = 0x000f'ffffu;
constexpr std::uint32_t BranchDisplacementMask = 0x003f'ffffu;
constexpr std::uint32_t SignBit = 0x8000'0000u;
}

namespace spi_control {
constexpr std::uint8_t Card0 = 1u;
constexpr std::uint8_t Fast = 4u;
}

} // namespace oberon
