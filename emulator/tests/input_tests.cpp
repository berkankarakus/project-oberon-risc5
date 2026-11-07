#include "input.hpp"

#include <SDL.h>

#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
  const auto space = oberon::input::keyboard_key(SDLK_SPACE);
  assert(space.has_value() && space->code == 0x29 && !space->extended);

  const auto left = oberon::input::keyboard_key(SDLK_LEFT);
  assert(left.has_value() && left->code == 0x6b && left->extended);

  const auto unknown = oberon::input::keyboard_key(SDLK_PRINTSCREEN);
  assert(!unknown.has_value());

  assert(oberon::input::mouse_button_mask(SDL_BUTTON_LEFT) == 0x04);
  assert(oberon::input::mouse_button_mask(SDL_BUTTON_MIDDLE) == 0x02);
  assert(oberon::input::mouse_button_mask(SDL_BUTTON_RIGHT) == 0x01);
  assert(oberon::input::mouse_buttons_from_state(SDL_BUTTON_LMASK | SDL_BUTTON_RMASK) == 0x05);

  assert(oberon::input::scale_coordinate(-1, 1500, 1024) == 0);
  assert(oberon::input::scale_coordinate(1499, 1500, 1024) == 1023);
  assert(oberon::input::scale_coordinate(750, 1500, 1024) == 511);
  std::cout << "input-tests: all tests passed\n";
}
