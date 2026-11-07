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

void Machine::run(std::uint64_t steps) {
  for (std::uint64_t i = 0; i < steps; ++i) cpu_.step();
}

} // namespace oberon
