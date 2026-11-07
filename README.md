# Project Oberon RISC5

Project Oberon RISC5 is a C++20 emulator for the RISC5 processor and the
Project Oberon hardware model. It includes the CPU core, memory-mapped devices,
SPI storage, disk-image support, an SDL2 display/input frontend, and the
original Oberon PROM image.

The emulator runs the custom RISC5 instruction set directly. QEMU and
VirtualBox are useful for running the emulator as a Linux guest process, but
they do not emulate RISC5 themselves.

## Requirements

- C++20 compiler (`clang++` or `g++`)
- GNU Make
- CMake 3.16 or newer (optional)
- SDL2 development files for the graphical UI and SDL input tests

On Debian/Ubuntu:

```sh
sudo apt install build-essential cmake libsdl2-dev
```

## Build and test

From the repository root:

```sh
make test
make all
make ui
```

`make test` runs the RISC5 core, device/SPI, SDL input, and command-line
option tests. The SDL-dependent targets require `sdl2-config`.

The equivalent CMake workflow is:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Run a bounded ROM smoke test without a disk image:

```sh
./oberon-run --rom RISC5Verilog/prom.mem --steps 1000
```

Run with a Project Oberon disk image:

```sh
./oberon-run \
  --rom RISC5Verilog/prom.mem \
  --disk /path/to/RISCimg.img \
  --steps 100000
```

Use `--read-only` when the disk image must not be changed. Use `--help` for
the complete CLI option list.

## Graphical UI

Build and start the SDL2 frontend with:

```sh
make ui
./oberon-ui --rom RISC5Verilog/prom.mem --disk /path/to/RISCimg.img
```

The UI presents the original 1024×768 monochrome framebuffer. `--scale`
changes only the host window size; it does not change the emulated display or
font rendering. `--steps-per-frame` changes emulation speed, and `--no-flip-y`
shows the raw framebuffer orientation.

Controls:

- `F9`: pause/resume
- `F10`: single-step
- `Escape`: quit
- Other keyboard and mouse input: forwarded to Oberon through the PS/2 and
  mouse MMIO devices

## Linux installation

Install into a user-writable prefix without `sudo`:

```sh
scripts/install-linux.sh "$HOME/.local"
```

This installs `project-oberon-run` and, when SDL2 is available,
`project-oberon-ui` under the selected prefix. The PROM image is installed
alongside the binaries, so commands do not need a repository-relative ROM
path.

For a self-contained Linux bundle:

```sh
scripts/build-linux-bundle.sh "$HOME/project-oberon-bundle"
"$HOME/project-oberon-bundle/run.sh" \
  --disk /path/to/RISCimg.img \
  --steps 100000
```

The bundle can be copied to a Linux guest over SSH, a QEMU shared folder, or a
VirtualBox shared folder.

## Repository layout

```text
emulator/include/   Public C++ headers
emulator/src/       Emulator and SDL implementation
emulator/tests/     Core, device, input, and option tests
RISC5Verilog/       RISC5 HDL sources and PROM image
OStationVerilog/    Oberon Station HDL sources
or/, graph/, ...    Original Project Oberon source packages
archives/           Original ZIP packages kept as reference inputs
scripts/            Linux installer and bundle helpers
.github/workflows/  GitHub Actions build and smoke-test workflow
```

The extracted Oberon sources and HDL files are kept in the repository for
traceability. ZIP files in `archives/` are archival inputs and are not needed
by the normal C++ build.

## Continuous integration

GitHub Actions builds the project on Ubuntu, installs SDL2, runs all tests,
builds the CLI and UI targets, and performs a bounded ROM smoke test.

## License and provenance

The emulator changes in this repository are maintained as a practical C++
implementation around the Project Oberon/RISC5 design. Original Oberon and
HDL material remains attributed to its upstream Project Oberon sources; see
the files in the corresponding source directories for their available
notices and documentation.
