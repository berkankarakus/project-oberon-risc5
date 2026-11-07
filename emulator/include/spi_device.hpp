#pragma once

#include "disk_image.hpp"
#include "risc5_constants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>

namespace oberon {

class SpiDevice {
public:
  void attach_disk(DiskImage& disk) { disk_ = &disk; }
  void detach_disk() { disk_ = nullptr; }
  bool has_disk() const { return disk_ != nullptr; }

  void reset();
  std::uint32_t read_data() const { return spi_rx_; }
  std::uint32_t read_status() const { return 1u; }
  void write_data(std::uint32_t value);
  void write_control(std::uint32_t value);

private:
  std::uint8_t exchange_byte(std::uint8_t value);
  std::uint32_t exchange_word(std::uint32_t value);
  void command_complete();

  DiskImage* disk_ = nullptr;
  std::uint8_t spi_control_ = 0;
  std::uint32_t spi_rx_ = 0xffff'ffffu;
  bool spi_selected_ = false;
  bool spi_app_command_ = false;
  bool spi_write_active_ = false;
  std::uint32_t spi_write_sector_ = 0;
  std::size_t spi_write_offset_ = 0;
  DiskImage::Sector spi_write_buffer_{};
  std::array<std::uint8_t, 6> spi_command_{};
  std::size_t spi_command_size_ = 0;
  std::deque<std::uint8_t> spi_response_;
};

} // namespace oberon
