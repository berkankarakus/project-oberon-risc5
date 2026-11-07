# RISC5 emulator

This directory contains the modernized, testable RISC5 interpreter derived
from `RISC5.v` and `RISC5Top.v`, together with MMIO devices, disk images and an
SDL2 display frontend.

Build and test from the repository root:

```sh
make test
```

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

The SDL frontend can be built when SDL2 is installed:

```sh
make ui
./oberon-ui --rom RISC5Verilog/prom.mem --disk /path/to/RISCimg.img --scale 1.5
```

The UI advances 100,000 emulated instructions per frame by default so the
disk boot reaches the first framebuffer output quickly. Use
`--steps-per-frame N` to trade boot speed for lower host CPU usage.

The framebuffer is the original 1024×768 monochrome region at `0xE7F00`.
The window scales that framebuffer without changing the emulated address map.

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
