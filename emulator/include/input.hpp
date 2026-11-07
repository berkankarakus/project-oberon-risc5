#pragma once

#include <cstdint>
#include <optional>

namespace oberon::input {

struct KeyboardKey {
  std::uint8_t code;
  bool extended;
};

std::optional<KeyboardKey> keyboard_key(int key);
std::uint8_t mouse_buttons_from_state(std::uint32_t state);
std::uint8_t mouse_button_mask(std::uint8_t button);
std::uint16_t scale_coordinate(int coordinate, int extent, std::uint32_t maximum);

} // namespace oberon::input
