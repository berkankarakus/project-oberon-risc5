#include "risc5.hpp"
#include "display.hpp"

#include <SDL.h>

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
  std::uint64_t steps_per_frame = 5'000;
  double scale = 1.25;
};

void usage(const char* name) {
  SDL_Log("usage: %s [--rom FILE] [--steps-per-frame N]", name);
  SDL_Log("         [--scale N] (default: 1.25; framebuffer remains 1024x768)");
  SDL_Log("controls: Space pause/resume, S single-step, Escape quit, other keys -> Oberon keyboard");
  SDL_Log("text input is also forwarded to the emulated UART receive queue");
}

Options parse_options(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--rom" && i + 1 < argc) {
      options.rom_path = argv[++i];
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
  return options;
}

void update_framebuffer(oberon::Memory& memory, std::vector<std::uint32_t>& pixels,
                        std::uint32_t black, std::uint32_t white) {
  for (std::uint32_t y = 0; y < oberon::display::Height; ++y) {
    const auto row = oberon::display::Base + y * oberon::display::RowBytes;
    for (std::uint32_t word = 0; word < oberon::display::Width / 32; ++word) {
      const auto bits = memory.read32(row + word * sizeof(std::uint32_t));
      for (std::uint32_t bit = 0; bit < 32; ++bit) {
        const auto x = word * 32 + bit;
        pixels[y * oberon::display::Width + x] = (bits & (1u << bit)) != 0 ? white : black;
      }
    }
  }
}

std::string window_title(const oberon::Machine& machine, bool paused, const std::string& uart_output) {
  std::ostringstream title;
  title << "Project Oberon | " << (paused ? "paused" : "running") << " | pc=0x" << std::hex << std::setw(6)
        << std::setfill('0') << machine.cpu().pc_byte() << " | steps=" << std::dec << machine.cpu().steps();
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
  try {
    const auto options = parse_options(argc, argv);
    oberon::Machine machine;
    if (!options.rom_path.empty()) machine.memory().load_rom_file(options.rom_path);

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
      SDL_SetWindowTitle(window, window_title(machine, paused, uart_output).c_str());
      update_framebuffer(machine.memory(), pixels, black, white);
      if (SDL_UpdateTexture(texture, nullptr, pixels.data(), oberon::display::Width * sizeof(std::uint32_t)) != 0) {
        throw std::runtime_error(SDL_GetError());
      }
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, texture, nullptr, nullptr);
      SDL_RenderPresent(renderer);
    }
    SDL_StopTextInput();
    SDL_DestroyTexture(texture);
  } catch (const std::exception& error) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "oberon-ui: %s", error.what());
    if (texture != nullptr) SDL_DestroyTexture(texture);
    if (renderer != nullptr) SDL_DestroyRenderer(renderer);
    if (window != nullptr) SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  if (texture != nullptr) SDL_DestroyTexture(texture);
  if (renderer != nullptr) SDL_DestroyRenderer(renderer);
  if (window != nullptr) SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
