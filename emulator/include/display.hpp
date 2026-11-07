#pragma once

#include <cstdint>

namespace oberon::display {

constexpr std::uint32_t Base = 0x000e'7f00u;
constexpr std::uint32_t Width = 1024;
constexpr std::uint32_t Height = 768;
constexpr std::uint32_t RowBytes = Width / 8;

} // namespace oberon::display
