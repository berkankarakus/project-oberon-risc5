#include "input.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>

namespace oberon::input {

std::optional<KeyboardKey> keyboard_key(int key) {
  static constexpr std::array<std::uint8_t, 26> letters = {
      0x1c, 0x32, 0x21, 0x23, 0x24, 0x2b, 0x34, 0x33, 0x43, 0x3b, 0x42, 0x4b, 0x3a,
      0x31, 0x44, 0x4d, 0x15, 0x2d, 0x1b, 0x2c, 0x3c, 0x2a, 0x1d, 0x22, 0x35, 0x1a};
  if (key >= SDLK_a && key <= SDLK_z) return KeyboardKey{letters[static_cast<std::size_t>(key - SDLK_a)], false};

  switch (key) {
  case SDLK_1: return KeyboardKey{0x16, false};
  case SDLK_2: return KeyboardKey{0x1e, false};
  case SDLK_3: return KeyboardKey{0x26, false};
  case SDLK_4: return KeyboardKey{0x25, false};
  case SDLK_5: return KeyboardKey{0x2e, false};
  case SDLK_6: return KeyboardKey{0x36, false};
  case SDLK_7: return KeyboardKey{0x3d, false};
  case SDLK_8: return KeyboardKey{0x3e, false};
  case SDLK_9: return KeyboardKey{0x46, false};
  case SDLK_0: return KeyboardKey{0x45, false};
  case SDLK_MINUS: return KeyboardKey{0x4e, false};
  case SDLK_EQUALS: return KeyboardKey{0x55, false};
  case SDLK_LEFTBRACKET: return KeyboardKey{0x54, false};
  case SDLK_RIGHTBRACKET: return KeyboardKey{0x5b, false};
  case SDLK_BACKSLASH: return KeyboardKey{0x5d, false};
  case SDLK_SEMICOLON: return KeyboardKey{0x4c, false};
  case SDLK_QUOTE: return KeyboardKey{0x52, false};
  case SDLK_BACKQUOTE: return KeyboardKey{0x0e, false};
  case SDLK_COMMA: return KeyboardKey{0x41, false};
  case SDLK_PERIOD: return KeyboardKey{0x49, false};
  case SDLK_SLASH: return KeyboardKey{0x4a, false};
  case SDLK_SPACE: return KeyboardKey{0x29, false};
  case SDLK_RETURN: return KeyboardKey{0x5a, false};
  case SDLK_TAB: return KeyboardKey{0x0d, false};
  case SDLK_BACKSPACE: return KeyboardKey{0x66, false};
  case SDLK_ESCAPE: return KeyboardKey{0x76, false};
  case SDLK_CAPSLOCK: return KeyboardKey{0x58, false};
  case SDLK_LSHIFT: return KeyboardKey{0x12, false};
  case SDLK_RSHIFT: return KeyboardKey{0x59, false};
  case SDLK_LCTRL: return KeyboardKey{0x14, false};
  case SDLK_RCTRL: return KeyboardKey{0x14, true};
  case SDLK_LALT: return KeyboardKey{0x11, false};
  case SDLK_RALT: return KeyboardKey{0x11, true};
  case SDLK_F1: return KeyboardKey{0x05, false};
  case SDLK_F2: return KeyboardKey{0x06, false};
  case SDLK_F3: return KeyboardKey{0x04, false};
  case SDLK_F4: return KeyboardKey{0x0c, false};
  case SDLK_F5: return KeyboardKey{0x03, false};
  case SDLK_F6: return KeyboardKey{0x0b, false};
  case SDLK_F7: return KeyboardKey{0x83, false};
  case SDLK_F8: return KeyboardKey{0x0a, false};
  case SDLK_F9: return KeyboardKey{0x01, false};
  case SDLK_F10: return KeyboardKey{0x09, false};
  case SDLK_F11: return KeyboardKey{0x78, false};
  case SDLK_F12: return KeyboardKey{0x07, false};
  case SDLK_UP: return KeyboardKey{0x75, true};
  case SDLK_DOWN: return KeyboardKey{0x72, true};
  case SDLK_LEFT: return KeyboardKey{0x6b, true};
  case SDLK_RIGHT: return KeyboardKey{0x74, true};
  case SDLK_INSERT: return KeyboardKey{0x70, true};
  case SDLK_DELETE: return KeyboardKey{0x71, true};
  case SDLK_HOME: return KeyboardKey{0x6c, true};
  case SDLK_END: return KeyboardKey{0x69, true};
  case SDLK_PAGEUP: return KeyboardKey{0x7d, true};
  case SDLK_PAGEDOWN: return KeyboardKey{0x7a, true};
  default: return std::nullopt;
  }
}

std::uint8_t mouse_buttons_from_state(std::uint32_t state) {
  return static_cast<std::uint8_t>(((state & SDL_BUTTON_LMASK) != 0 ? 0x04u : 0u) |
                                   ((state & SDL_BUTTON_MMASK) != 0 ? 0x02u : 0u) |
                                   ((state & SDL_BUTTON_RMASK) != 0 ? 0x01u : 0u));
}

std::uint8_t mouse_button_mask(std::uint8_t button) {
  if (button == SDL_BUTTON_LEFT) return 0x04u;
  if (button == SDL_BUTTON_MIDDLE) return 0x02u;
  if (button == SDL_BUTTON_RIGHT) return 0x01u;
  return 0;
}

std::uint16_t scale_coordinate(int coordinate, int extent, std::uint32_t maximum) {
  if (extent <= 1) return 0;
  const auto clamped = std::clamp(coordinate, 0, extent - 1);
  return static_cast<std::uint16_t>(clamped * static_cast<long long>(maximum - 1) / (extent - 1));
}

} // namespace oberon::input
