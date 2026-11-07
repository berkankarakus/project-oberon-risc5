CXX ?= clang++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -I emulator/include

.PHONY: all ui test clean

all: oberon-run risc5-tests

ui: oberon-ui

oberon-run: emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/src/main.cpp emulator/include/risc5.hpp emulator/include/risc5_constants.hpp emulator/include/devices.hpp emulator/include/disk_image.hpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/src/main.cpp -o $@

risc5-tests: emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/tests/risc5_tests.cpp emulator/include/risc5.hpp emulator/include/risc5_constants.hpp emulator/include/devices.hpp emulator/include/disk_image.hpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/tests/risc5_tests.cpp -o $@

oberon-ui: emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/src/ui_main.cpp emulator/include/risc5.hpp emulator/include/risc5_constants.hpp emulator/include/devices.hpp emulator/include/disk_image.hpp
	@if ! command -v sdl2-config >/dev/null 2>&1; then echo "oberon-ui: sdl2-config is required"; exit 1; fi
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $$(sdl2-config --cflags) emulator/src/risc5.cpp emulator/src/devices.cpp emulator/src/disk_image.cpp emulator/src/machine.cpp emulator/src/ui_main.cpp -o $@ $$(sdl2-config --libs)

test: risc5-tests
	./risc5-tests

clean:
	rm -f oberon-run oberon-ui risc5-tests
