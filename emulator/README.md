# RISC5 emulator

This directory contains the modernized, testable RISC5 interpreter derived
from `RISC5.v` and `RISC5Top.v`, together with MMIO devices, disk images and an
SDL2 display frontend.

Build and test from the repository root:

```sh
make test
```

This runs the RISC5 core, device/SPI, SDL input, and command-line option tests.
GitHub Actions runs the same suites on pushes and pull requests to `main`.

To build all command-line and test binaries:

```sh
make all
```

Install locally on Linux without `sudo`:

```sh
scripts/install-linux.sh ~/.local
project-oberon-run --disk /path/to/RISCimg.img --steps 100000
project-oberon-ui --disk /path/to/RISCimg.img
```

The installer always installs the CLI and installs the SDL UI when
`sdl2-config` is available. Pass another prefix as the first argument, for
example `scripts/install-linux.sh "$HOME/.local"`.

Or, when CMake is available:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Run a bounded ROM smoke test:

```sh
./build/oberon-run --rom RISC5Verilog/prom.mem --steps 1000
```

Run with a disk image:

```sh
./oberon-run --rom RISC5Verilog/prom.mem --disk /path/to/RISCimg.img --steps 100000
```

The CLI also accepts `--read-only` with `--disk FILE`; `--help` prints usage.
The UI shares the ROM, disk, and read-only option handling, and adds
`--steps-per-frame`, `--scale`, and `--no-flip-y`.

The SDL frontend can be built when SDL2 is installed:

```sh
make ui
./oberon-ui --rom RISC5Verilog/prom.mem --disk /path/to/RISCimg.img
```

The UI advances 100,000 emulated instructions per frame by default so the
disk boot reaches the first framebuffer output quickly. Use
`--steps-per-frame N` to trade boot speed for lower host CPU usage.
The default window scale is 1.25; `--scale N` changes the window size without
changing the emulated 1024×768 framebuffer.
The default framebuffer presentation flips the vertical axis to match the
FPGA video controller; use `--no-flip-y` for the raw memory orientation.

UI controls are `F9` for pause/resume, `F10` for single-step, and `Escape` to
quit. Other key presses, including Space, are forwarded to Oberon using PS/2
Set 2 scan codes. Mouse motion and the left, middle, and right buttons are
forwarded through the Oberon mouse MMIO register.

The framebuffer is the original 1024×768 monochrome region at `0xE7F00`.
The window scales that framebuffer without changing the emulated address map.

Generated binaries (`oberon-run`, `oberon-ui`, and the test executables) are
local build outputs. The disk image used for boot tests is not modified by
read-only runs.

## Linux guest bundle

QEMU and VirtualBox do not execute the custom RISC5 instruction set directly.
The supported virtualization path is to run the emulator as a native Linux
guest process. Inside the Linux guest, install a C++20 compiler and run:

```sh
scripts/build-linux-bundle.sh ~/project-oberon-bundle
~/project-oberon-bundle/run.sh --disk /path/to/RISCimg.img --steps 100000
```

The bundle contains the CLI emulator and the original PROM image, so it can be
copied through a QEMU shared folder, SSH, or a VirtualBox shared folder.
