#include "risc5.hpp"
#include "display.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
  std::string rom_path;
  std::string disk_path;
  std::uint64_t steps_per_frame = 100'000;
  double scale = 1.5;
  bool disk_writable = true;
  bool flip_y = true;
};

void usage(const char* name) {
  SDL_Log("usage: %s [--rom FILE] [--disk FILE] [--read-only] [--steps-per-frame N]", name);
  SDL_Log("         [--scale N] [--no-flip-y] (default scale: 1.5; framebuffer remains 1024x768)");
  SDL_Log("controls: Space pause/resume, S single-step, Escape quit, other keys -> Oberon keyboard");
  SDL_Log("text input is also forwarded to the emulated UART receive queue");
}

Options parse_options(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--rom" && i + 1 < argc) {
      options.rom_path = argv[++i];
    } else if (argument == "--disk" && i + 1 < argc) {
      options.disk_path = argv[++i];
    } else if (argument == "--read-only") {
      options.disk_writable = false;
    } else if (argument == "--no-flip-y") {
      options.flip_y = false;
    } else if (argument == "--steps-per-frame" && i + 1 < argc) {
      options.steps_per_frame = std::stoull(argv[++i]);
    } else if (argument == "--scale" && i + 1 < argc) {
      options.scale = std::stod(argv[++i]);
      if (options.scale < 0.5 || options.scale > 4.0) {
        throw std::invalid_argument("--scale must be between 0.5 and 4.0");
      }
    } else if (argument == "--help" || argument == "-h") {
      usage(argv[0]);
      std::exit(0);
    } else {
      usage(argv[0]);
      throw std::invalid_argument("unknown or incomplete command-line option: " + argument);
    }
  }
  if (!options.disk_writable && options.disk_path.empty()) {
    throw std::invalid_argument("--read-only requires --disk FILE");
  }
  return options;
}

std::size_t update_framebuffer(oberon::Memory& memory, std::vector<std::uint32_t>& pixels,
                               std::uint32_t black, std::uint32_t white, bool flip_y) {
  std::size_t lit_pixels = 0;
  for (std::uint32_t y = 0; y < oberon::display::Height; ++y) {
    const auto row = oberon::display::Base + y * oberon::display::RowBytes;
    for (std::uint32_t word = 0; word < oberon::display::Width / 32; ++word) {
      const auto bits = memory.read32(row + word * sizeof(std::uint32_t));
      lit_pixels += std::popcount(bits);
      const auto screen_y = flip_y ? oberon::display::Height - 1 - y : y;
      for (std::uint32_t bit = 0; bit < 32; ++bit) {
        const auto x = word * 32 + bit;
        pixels[screen_y * oberon::display::Width + x] = (bits & (1u << bit)) != 0 ? white : black;
      }
    }
  }
  return lit_pixels;
}

struct KeyboardKey {
  std::uint8_t code;
  bool extended;
};

std::optional<KeyboardKey> keyboard_key(SDL_Keycode key) {
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

void enqueue_keyboard_event(oberon::Memory& memory, SDL_Keycode key, bool pressed) {
  const auto mapped = keyboard_key(key);
  if (!mapped.has_value()) return;
  if (mapped->extended) memory.enqueue_keyboard(0xe0);
  if (!pressed) memory.enqueue_keyboard(0xf0);
  memory.enqueue_keyboard(mapped->code);
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

std::uint16_t mouse_coordinate(int coordinate, int extent, std::uint32_t maximum) {
  if (extent <= 1) return 0;
  const auto clamped = std::clamp(coordinate, 0, extent - 1);
  return static_cast<std::uint16_t>(clamped * static_cast<long long>(maximum - 1) / (extent - 1));
}

void update_mouse(oberon::Memory& memory, int x, int y, int window_width, int window_height, bool flip_y,
                  std::uint8_t buttons) {
  const auto screen_x = mouse_coordinate(x, window_width, oberon::display::Width);
  const auto screen_y = mouse_coordinate(y, window_height, oberon::display::Height);
  const auto memory_y = flip_y ? static_cast<std::uint16_t>(oberon::display::Height - 1 - screen_y) : screen_y;
  memory.set_mouse(screen_x, memory_y, buttons);
}

std::string window_title(const oberon::Machine& machine, bool paused, const std::string& uart_output,
                         std::size_t lit_pixels) {
  std::ostringstream title;
  title << "Project Oberon | " << (paused ? "paused" : "running") << " | pc=0x" << std::hex << std::setw(6)
        << std::setfill('0') << machine.cpu().pc_byte() << " | steps=" << std::dec << machine.cpu().steps()
        << " | fb=" << lit_pixels;
  if (!uart_output.empty()) {
    const auto start = uart_output.size() > 32 ? uart_output.size() - 32 : 0;
    title << " | uart=\"";
    for (std::size_t i = start; i < uart_output.size(); ++i) {
      const auto character = uart_output[i];
      title << ((character == '\n' || character == '\r') ? ' ' : character);
    }
    title << '"';
  }
  return title.str();
}

} // namespace

int main(int argc, char** argv) {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* texture = nullptr;
  bool text_input_started = false;
  const auto cleanup_sdl = [&] {
    if (text_input_started) {
      SDL_StopTextInput();
      text_input_started = false;
    }
    if (texture != nullptr) {
      SDL_DestroyTexture(texture);
      texture = nullptr;
    }
    if (renderer != nullptr) {
      SDL_DestroyRenderer(renderer);
      renderer = nullptr;
    }
    if (window != nullptr) {
      SDL_DestroyWindow(window);
      window = nullptr;
    }
    SDL_Quit();
  };
  try {
    const auto options = parse_options(argc, argv);
    oberon::Machine machine;
    if (!options.rom_path.empty()) machine.memory().load_rom_file(options.rom_path);
    if (!options.disk_path.empty()) machine.open_disk(options.disk_path, options.disk_writable);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
    auto window_width = static_cast<int>(oberon::display::Width * options.scale);
    auto window_height = static_cast<int>(oberon::display::Height * options.scale);
    window = SDL_CreateWindow("Project Oberon", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              window_width, window_height, SDL_WINDOW_RESIZABLE);
    if (window == nullptr) throw std::runtime_error(SDL_GetError());
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == nullptr) throw std::runtime_error(SDL_GetError());
    auto* pixel_format = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    if (pixel_format == nullptr) throw std::runtime_error(SDL_GetError());
    const auto black = SDL_MapRGB(pixel_format, 0, 0, 0);
    const auto white = SDL_MapRGB(pixel_format, 255, 255, 255);
    SDL_FreeFormat(pixel_format);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                oberon::display::Width, oberon::display::Height);
    if (texture == nullptr) throw std::runtime_error(SDL_GetError());
    std::vector<std::uint32_t> pixels(oberon::display::Width * oberon::display::Height, black);
    SDL_StartTextInput();
    text_input_started = true;
    std::uint8_t mouse_buttons = 0;
    update_mouse(machine.memory(), window_width / 2, window_height / 2, window_width, window_height, options.flip_y,
                 mouse_buttons);

    bool running = true;
    bool paused = false;
    std::string uart_output;
    while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) running = false;
        if (event.type == SDL_TEXTINPUT) {
          for (const auto* character = event.text.text; *character != '\0'; ++character) {
            machine.memory().enqueue_uart_rx(static_cast<std::uint8_t>(*character));
          }
          continue;
        }
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          window_width = std::max(1, event.window.data1);
          window_height = std::max(1, event.window.data2);
          continue;
        }
        if (event.type == SDL_MOUSEMOTION) {
          update_mouse(machine.memory(), event.motion.x, event.motion.y, window_width, window_height, options.flip_y,
                       mouse_buttons_from_state(event.motion.state));
          continue;
        }
        if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
          const auto mask = mouse_button_mask(event.button.button);
          if (mask != 0) {
            if (event.type == SDL_MOUSEBUTTONDOWN) mouse_buttons |= static_cast<std::uint8_t>(mask);
            else mouse_buttons &= static_cast<std::uint8_t>(~mask);
          }
          update_mouse(machine.memory(), event.button.x, event.button.y, window_width, window_height, options.flip_y,
                       mouse_buttons);
          continue;
        }
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
          if (event.key.repeat != 0) continue;
          if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
          else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) paused = !paused;
          else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_s) machine.step();
          else enqueue_keyboard_event(machine.memory(), event.key.keysym.sym, event.type == SDL_KEYDOWN);
        }
      }

      if (!paused) machine.run(options.steps_per_frame);
      const auto frame_output = machine.memory().take_uart_output();
      if (!frame_output.empty()) uart_output += frame_output;
      const auto lit_pixels = update_framebuffer(machine.memory(), pixels, black, white, options.flip_y);
      SDL_SetWindowTitle(window, window_title(machine, paused, uart_output, lit_pixels).c_str());
      if (SDL_UpdateTexture(texture, nullptr, pixels.data(), oberon::display::Width * sizeof(std::uint32_t)) != 0) {
        throw std::runtime_error(SDL_GetError());
      }
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, texture, nullptr, nullptr);
      SDL_RenderPresent(renderer);
    }
    cleanup_sdl();
  } catch (const std::exception& error) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "oberon-ui: %s", error.what());
    cleanup_sdl();
    return 1;
  }
  return 0;
}
