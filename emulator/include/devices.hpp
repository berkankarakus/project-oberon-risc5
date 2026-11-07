#pragma once

#include "disk_image.hpp"
#include "risc5_constants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>

namespace oberon {

class DeviceBus {
public:
  static constexpr std::uint32_t IoBase = 0x00ff'ffc0u;

  bool handles(std::uint32_t address) const;
  std::uint32_t read(std::uint32_t address);
  void write(std::uint32_t address, std::uint32_t value);
  void reset();
  void tick();

  void enqueue_uart_rx(std::uint8_t value);
  void enqueue_keyboard(std::uint8_t value);
  std::string take_uart_output();
  void attach_disk(DiskImage& disk) { disk_ = disk; }
  void detach_disk() { disk_.reset(); }
  bool has_disk() const { return disk_.has_value(); }
  std::uint32_t leds() const { return leds_; }

private:
  std::uint8_t spi_exchange_byte(std::uint8_t value);
  std::uint32_t spi_exchange_word(std::uint32_t value);
  void spi_command_complete();

  std::uint32_t milliseconds_ = 0;
  std::uint8_t leds_ = 0;
  std::uint8_t gpio_out_ = 0;
  std::uint8_t gpio_control_ = 0;
  std::uint8_t spi_control_ = 0;
  std::uint8_t uart_control_ = 0;
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
  std::optional<std::reference_wrapper<DiskImage>> disk_;
  std::deque<std::uint8_t> uart_rx_;
  std::deque<std::uint8_t> keyboard_;
  std::string uart_output_;
};

} // namespace oberon
