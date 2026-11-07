# RISC5 emulator (first vertical slice)

This directory contains the first executable part of the modernization work:
a small, testable RISC5 interpreter derived from `RISC5.v` and `RISC5Top.v`.

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

The current slice implements the integer/branch/load-store core and the
PROM/SRAM address behavior. MMIO devices, interrupts, disk images and the
graphical frontend are intentionally the next layers.
