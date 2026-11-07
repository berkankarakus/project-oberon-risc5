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

} // namespace

int main() {
  test_common_options();
  test_help_and_unknown_options();
  test_read_only_requires_disk();
  std::cout << "options-tests: all tests passed\n";
}
