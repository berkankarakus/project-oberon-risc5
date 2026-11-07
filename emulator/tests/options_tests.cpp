#include "options.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

namespace {

void test_common_options() {
  char arg0[] = "test";
  char arg1[] = "--rom";
  char arg2[] = "prom.mem";
  char arg3[] = "--disk";
  char arg4[] = "work.img";
  char arg5[] = "--read-only";
  char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5};
  oberon::CommonOptions options;
  for (int index = 1; index < 6; ++index) assert(oberon::parse_common_option(6, argv, index, options));
  assert(options.rom_path == "prom.mem");
  assert(options.disk_path == "work.img");
  assert(!options.disk_writable);
  oberon::validate_common_options(options);
}

void test_help_and_unknown_options() {
  char arg0[] = "test";
  char arg1[] = "--help";
  char arg2[] = "--steps";
  char* argv[] = {arg0, arg1, arg2};
  oberon::CommonOptions options;
  int help_index = 1;
  assert(oberon::parse_common_option(3, argv, help_index, options));
  assert(options.help_requested);
  int unknown_index = 2;
  assert(!oberon::parse_common_option(3, argv, unknown_index, options));
}

void test_read_only_requires_disk() {
  oberon::CommonOptions options;
  options.disk_writable = false;
  bool rejected = false;
  try { oberon::validate_common_options(options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void test_read_only_with_disk_is_valid() {
  oberon::CommonOptions options;
  options.disk_path = "work.img";
  options.disk_writable = false;
  oberon::validate_common_options(options);
}

void test_missing_values_are_rejected() {
  char arg0[] = "test";
  char rom[] = "--rom";
  char disk[] = "--disk";
  char* rom_argv[] = {arg0, rom};
  char* disk_argv[] = {arg0, disk};

  for (auto* argv : {rom_argv, disk_argv}) {
    bool rejected = false;
    try {
      int index = 1;
      oberon::CommonOptions options;
      oberon::parse_common_option(2, argv, index, options);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
}

void test_repeated_paths_use_last_value() {
  char arg0[] = "test";
  char rom[] = "--rom";
  char first[] = "first.mem";
  char second[] = "second.mem";
  char* argv[] = {arg0, rom, first, rom, second};
  oberon::CommonOptions options;
  for (int index = 1; index < 5; ++index) assert(oberon::parse_common_option(5, argv, index, options));
  assert(options.rom_path == "second.mem");
}

} // namespace

int main() {
  test_common_options();
  test_help_and_unknown_options();
  test_read_only_requires_disk();
  test_read_only_with_disk_is_valid();
  test_missing_values_are_rejected();
  test_repeated_paths_use_last_value();
  std::cout << "options-tests: all tests passed\n";
}
