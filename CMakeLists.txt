cmake_minimum_required(VERSION 3.20)
project(project_oberon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

set(RISC5_SOURCES
    emulator/src/risc5.cpp
    emulator/src/devices.cpp
    emulator/src/spi_device.cpp
    emulator/src/disk_image.cpp
    emulator/src/machine.cpp)

add_library(risc5 ${RISC5_SOURCES})
target_include_directories(risc5 PUBLIC emulator/include)
target_compile_options(risc5 PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)

add_library(project_oberon_options emulator/src/options.cpp)
target_include_directories(project_oberon_options PUBLIC emulator/include)
target_compile_options(project_oberon_options PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)

add_executable(oberon-run emulator/src/main.cpp)
target_link_libraries(oberon-run PRIVATE risc5 project_oberon_options)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
endif()

if(TARGET PkgConfig::SDL2)
  add_library(project_oberon_input emulator/src/input.cpp)
  target_include_directories(project_oberon_input PUBLIC emulator/include)
  target_link_libraries(project_oberon_input PUBLIC PkgConfig::SDL2)
  target_compile_options(project_oberon_input PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)

  add_executable(oberon-ui emulator/src/ui.cpp emulator/src/ui_main.cpp)
  target_link_libraries(oberon-ui PRIVATE risc5 project_oberon_options project_oberon_input PkgConfig::SDL2)

  add_executable(input-tests emulator/tests/input_tests.cpp)
  target_link_libraries(input-tests PRIVATE project_oberon_input PkgConfig::SDL2)
  add_test(NAME input-tests COMMAND input-tests)
endif()

add_executable(risc5-tests emulator/tests/risc5_core_tests.cpp)
target_link_libraries(risc5-tests PRIVATE risc5)

add_executable(device-tests emulator/tests/device_tests.cpp)
target_link_libraries(device-tests PRIVATE risc5)

add_executable(options-tests emulator/tests/options_tests.cpp)
target_link_libraries(options-tests PRIVATE project_oberon_options)

add_test(NAME risc5-tests COMMAND risc5-tests)
add_test(NAME device-tests COMMAND device-tests)
add_test(NAME options-tests COMMAND options-tests)
