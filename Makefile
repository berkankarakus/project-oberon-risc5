CXX ?= clang++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -I emulator/include

.PHONY: all test clean

all: oberon-run risc5-tests

oberon-run: emulator/src/risc5.cpp emulator/src/disk_image.cpp emulator/src/main.cpp emulator/include/risc5.hpp emulator/include/disk_image.hpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) emulator/src/risc5.cpp emulator/src/disk_image.cpp emulator/src/main.cpp -o $@

risc5-tests: emulator/src/risc5.cpp emulator/src/disk_image.cpp emulator/tests/risc5_tests.cpp emulator/include/risc5.hpp emulator/include/disk_image.hpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) emulator/src/risc5.cpp emulator/src/disk_image.cpp emulator/tests/risc5_tests.cpp -o $@

test: risc5-tests
	./risc5-tests

clean:
	rm -f oberon-run risc5-tests
