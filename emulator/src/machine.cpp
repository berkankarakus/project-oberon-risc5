#include "risc5.hpp"

namespace oberon {

Machine::Machine(std::size_t ram_bytes) : memory_(ram_bytes), cpu_(memory_) {}

void Machine::open_disk(const std::string& path, bool writable) {
  close_disk();
  disk_.emplace();
  disk_->open(path, writable);
  memory_.attach_disk(*disk_);
}

void Machine::close_disk() {
  memory_.detach_disk();
  disk_.reset();
}

void Machine::step() {
  const auto irq = timer_phase_ == TimerInterruptPeriod - 1;
  cpu_.step(irq);
  timer_phase_ = (timer_phase_ + 1) % TimerInterruptPeriod;
  memory_.advance_time();
}

void Machine::run(std::uint64_t steps) {
  for (std::uint64_t i = 0; i < steps; ++i) step();
}

} // namespace oberon
