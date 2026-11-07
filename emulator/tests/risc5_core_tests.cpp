#include "test_support.hpp"

#include <array>
#include <cassert>
#include <iostream>
#include <utility>

namespace {
using namespace test_support;

void test_arithmetic_and_flags() {
  Memory memory;
  memory.load_rom_words(rom_with({mov_imm(1, 7), mov_imm(2, 5), alu(AluOp::Add, 3, 1, 2)}));
  Risc5 cpu(memory);
  cpu.step();
  cpu.step();
  cpu.step();
  assert(cpu.reg(3) == 12);
  assert(!cpu.zero());
  assert(!cpu.negative());
}

void test_integer_signedness_and_subtract_borrow() {
  {
    Memory memory;
    memory.load_rom_words(rom_with({mov_imm(1, 0xffff, true), mov_imm(2, 2), alu(AluOp::Mul, 3, 1, 2)}));
    Risc5 cpu(memory);
    for (int i = 0; i < 3; ++i) cpu.step();
    assert(cpu.reg(3) == 0xffff'fffeu);
    assert(cpu.high() == 0xffff'ffffu);
  }
  {
    Memory memory;
    memory.load_rom_words(rom_with({mov_imm(1, 0xffff, true), mov_imm(2, 2), alu(AluOp::Mul, 3, 1, 2) | U}));
    Risc5 cpu(memory);
    for (int i = 0; i < 3; ++i) cpu.step();
    assert(cpu.reg(3) == 0xffff'fffeu);
    assert(cpu.high() == 1);
  }
  {
    Memory memory;
    memory.load_rom_words(rom_with({mov_imm(1, 3), mov_imm(2, 5), alu(AluOp::Sub, 3, 1, 2)}));
    Risc5 cpu(memory);
    for (int i = 0; i < 3; ++i) cpu.step();
    assert(cpu.reg(3) == 0xffff'fffeu);
    assert(cpu.carry());
  }
  {
    Memory memory;
    memory.load_rom_words(rom_with({mov_imm(1, 0xffff, true), mov_imm(2, 2), alu(AluOp::Mul, 3, 1, 2),
                                    alu(AluOp::Mov, 4, 0, 0) | U, alu(AluOp::Mov, 5, 0, 0) | U | V}));
    Risc5 cpu(memory);
    for (int i = 0; i < 5; ++i) cpu.step();
    assert(cpu.reg(4) == 0xffff'ffffu);
    assert((cpu.reg(5) & 0xffu) == 0x53u);
  }
}

void test_branch_and_link() {
  Memory memory;
  memory.load_rom_words(rom_with({branch_relative(ConditionCode::Always, 2, true), mov_imm(1, 0xdead),
                                  mov_imm(1, 0xbeef), mov_imm(2, 42)}));
  Risc5 cpu(memory);
  cpu.step();
  cpu.step();
  assert(cpu.reg(1) == 0);
  assert(cpu.reg(2) == 42);
  assert(cpu.reg(15) == (Risc5::ResetPc + 1) * 4);
}

void test_memory_word_and_byte_access() {
  Memory memory;
  memory.load_rom_words(rom_with({mov_imm(1, 0x100), mov_imm(2, 0xaa), memory_op(true, true, 2, 1, 0x20),
                                  memory_op(false, true, 3, 1, 0x20)}));
  Risc5 cpu(memory);
  for (int i = 0; i < 4; ++i) cpu.step();
  assert(cpu.reg(3) == 0xaa);
}

void test_interrupt_vector_and_rti() {
  Memory memory;
  const auto enable_interrupts = 0xc700'0021u;
  const auto return_from_interrupt = 0xc700'0010u;
  memory.load_rom_words(rom_with({enable_interrupts, return_from_interrupt}));
  memory.write32(4, return_from_interrupt);
  Risc5 cpu(memory);
  cpu.step();
  cpu.step(true);
  cpu.step(false);
  assert(cpu.pc_word() == 1);
  cpu.step(false);
  assert(cpu.pc_word() == Risc5::ResetPc + 3);
}

} // namespace

int main() {
  using Test = void (*)();
  const std::array<std::pair<const char*, Test>, 5> tests = {
      std::pair{"arithmetic and flags", &test_arithmetic_and_flags},
      std::pair{"integer signedness and subtract borrow", &test_integer_signedness_and_subtract_borrow},
      std::pair{"branch and link", &test_branch_and_link},
      std::pair{"memory word and byte access", &test_memory_word_and_byte_access},
      std::pair{"interrupt vector and RTI", &test_interrupt_vector_and_rti},
  };
  for (const auto& [name, test] : tests) {
    test();
    std::cout << "[ok] " << name << '\n';
  }
  std::cout << "risc5-core-tests: all tests passed\n";
}
