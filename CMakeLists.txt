cmake_minimum_required(VERSION 3.20)
project(project_oberon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(risc5 emulator/src/risc5.cpp emulator/src/disk_image.cpp)
target_include_directories(risc5 PUBLIC emulator/include)
target_compile_options(risc5 PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_executable(oberon-run emulator/src/main.cpp)
target_link_libraries(oberon-run PRIVATE risc5)

enable_testing()
add_executable(risc5-tests emulator/tests/risc5_tests.cpp)
target_link_libraries(risc5-tests PRIVATE risc5)
add_test(NAME risc5-tests COMMAND risc5-tests)
