#include "risc5.hpp"
#include "display.hpp"

#include <SDL.h>

#include <bit>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
  std::string rom_path;
  std::string disk_path;
  std::uint64_t steps_per_frame = 100'000;
  double scale = 1.25;
  bool disk_writable = true;
  bool flip_y = true;
};

void usage(const char* name) {
  SDL_Log("usage: %s [--rom FILE] [--disk FILE] [--read-only] [--steps-per-frame N]", name);
  SDL_Log("         [--scale N] [--no-flip-y] (default scale: 1.25; framebuffer remains 1024x768)");
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
    const auto window_width = static_cast<int>(oberon::display::Width * options.scale);
    const auto window_height = static_cast<int>(oberon::display::Height * options.scale);
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
        if (event.type != SDL_KEYDOWN || event.key.repeat != 0) continue;
        if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
        else if (event.key.keysym.sym == SDLK_SPACE) paused = !paused;
        else if (event.key.keysym.sym == SDLK_s) machine.step();
        else if (event.key.keysym.scancode <= SDL_SCANCODE_UNKNOWN + 255) {
          machine.memory().enqueue_keyboard(static_cast<std::uint8_t>(event.key.keysym.scancode));
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
