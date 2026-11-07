CXX ?= clang++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -I emulator/include

.PHONY: all ui test input-tests clean

all: oberon-run risc5-tests device-tests input-tests

ui: oberon-ui

oberon-run: emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/spi_device.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/src/main.cpp emulator/include/risc5.hpp emulator/include/risc5_constants.hpp emulator/include/devices.hpp emulator/include/spi_device.hpp emulator/include/disk_image.hpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/spi_device.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/src/main.cpp -o $@

risc5-tests: emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/spi_device.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/tests/risc5_core_tests.cpp emulator/tests/test_support.hpp emulator/include/risc5.hpp emulator/include/risc5_constants.hpp emulator/include/devices.hpp emulator/include/spi_device.hpp emulator/include/disk_image.hpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/spi_device.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/tests/risc5_core_tests.cpp -o $@

device-tests: emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/spi_device.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/tests/device_tests.cpp emulator/tests/test_support.hpp emulator/include/risc5.hpp emulator/include/risc5_constants.hpp emulator/include/devices.hpp emulator/include/spi_device.hpp emulator/include/disk_image.hpp emulator/include/display.hpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/spi_device.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/tests/device_tests.cpp -o $@

input-tests: emulator/src/input.cpp emulator/tests/input_tests.cpp emulator/include/input.hpp
	@if ! command -v sdl2-config >/dev/null 2>&1; then echo "input-tests: sdl2-config is required"; exit 1; fi
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $$(sdl2-config --cflags) emulator/src/input.cpp emulator/tests/input_tests.cpp -o $@

test: risc5-tests device-tests input-tests
	./risc5-tests
	./device-tests
	./input-tests

oberon-ui: emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/spi_device.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/src/input.cpp emulator/src/ui.cpp emulator/src/ui_main.cpp emulator/include/risc5.hpp emulator/include/risc5_constants.hpp emulator/include/devices.hpp emulator/include/spi_device.hpp emulator/include/disk_image.hpp emulator/include/display.hpp emulator/include/input.hpp emulator/include/ui.hpp
	@if ! command -v sdl2-config >/dev/null 2>&1; then echo "oberon-ui: sdl2-config is required"; exit 1; fi
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $$(sdl2-config --cflags) emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/spi_device.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/src/input.cpp emulator/src/ui.cpp emulator/src/ui_main.cpp -o $@ $$(sdl2-config --libs)

clean:
	rm -f oberon-run oberon-ui risc5-tests device-tests input-tests
