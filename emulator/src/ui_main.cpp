#include "risc5.hpp"

#include <SDL.h>

#include <cstdlib>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

struct Options {
  std::string rom_path;
  std::uint64_t steps_per_frame = 5'000;
};

void usage(const char* name) {
  SDL_Log("usage: %s [--rom FILE] [--steps-per-frame N]", name);
  SDL_Log("controls: Space pause/resume, S single-step, Escape quit");
}

Options parse_options(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--rom" && i + 1 < argc) {
      options.rom_path = argv[++i];
    } else if (argument == "--steps-per-frame" && i + 1 < argc) {
      options.steps_per_frame = std::stoull(argv[++i]);
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

void draw_status(SDL_Renderer* renderer, const oberon::Machine& machine, bool paused) {
  SDL_SetRenderDrawColor(renderer, 10, 14, 20, 255);
  SDL_RenderClear(renderer);

  const SDL_Rect frame{24, 24, 752, 432};
  SDL_SetRenderDrawColor(renderer, 74, 92, 116, 255);
  SDL_RenderDrawRect(renderer, &frame);

  const SDL_Rect cpu_panel{48, 56, 704, 120};
  SDL_SetRenderDrawColor(renderer, 22, 30, 42, 255);
  SDL_RenderFillRect(renderer, &cpu_panel);
  SDL_SetRenderDrawColor(renderer, paused ? 236 : 94, paused ? 182 : 210, paused ? 74 : 126, 255);
  SDL_RenderDrawRect(renderer, &cpu_panel);

  const SDL_Rect led_panel{48, 208, 704, 176};
  SDL_SetRenderDrawColor(renderer, 22, 30, 42, 255);
  SDL_RenderFillRect(renderer, &led_panel);
  SDL_SetRenderDrawColor(renderer, 74, 92, 116, 255);
  SDL_RenderDrawRect(renderer, &led_panel);

  const auto leds = machine.memory().leds();
  for (int bit = 0; bit < 8; ++bit) {
    const SDL_Rect led{88 + bit * 80, 264, 48, 48};
    const bool on = (leds & (1u << bit)) != 0;
    SDL_SetRenderDrawColor(renderer, on ? 72 : 28, on ? 220 : 36, on ? 124 : 48, 255);
    SDL_RenderFillRect(renderer, &led);
    SDL_SetRenderDrawColor(renderer, 130, 150, 170, 255);
    SDL_RenderDrawRect(renderer, &led);
  }

  SDL_RenderPresent(renderer);
}

std::string window_title(const oberon::Machine& machine, bool paused) {
  std::ostringstream title;
  title << "Project Oberon | " << (paused ? "paused" : "running") << " | pc=0x" << std::hex << std::setw(6)
        << std::setfill('0') << machine.cpu().pc_byte() << " | steps=" << std::dec << machine.cpu().steps();
  return title.str();
}

} // namespace

int main(int argc, char** argv) {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  try {
    const auto options = parse_options(argc, argv);
    oberon::Machine machine;
    if (!options.rom_path.empty()) machine.memory().load_rom_file(options.rom_path);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
    window = SDL_CreateWindow("Project Oberon", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 480, 0);
    if (window == nullptr) throw std::runtime_error(SDL_GetError());
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) throw std::runtime_error(SDL_GetError());

    bool running = true;
    bool paused = false;
    while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) running = false;
        if (event.type != SDL_KEYDOWN || event.key.repeat != 0) continue;
        if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
        if (event.key.keysym.sym == SDLK_SPACE) paused = !paused;
        if (event.key.keysym.sym == SDLK_s) machine.step();
      }

      if (!paused) machine.run(options.steps_per_frame);
      SDL_SetWindowTitle(window, window_title(machine, paused).c_str());
      draw_status(renderer, machine, paused);
    }
  } catch (const std::exception& error) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "oberon-ui: %s", error.what());
    if (renderer != nullptr) SDL_DestroyRenderer(renderer);
    if (window != nullptr) SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  if (renderer != nullptr) SDL_DestroyRenderer(renderer);
  if (window != nullptr) SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
