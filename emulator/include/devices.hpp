#pragma once

#include "risc5_constants.hpp"
#include "spi_device.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

namespace oberon {

class DeviceBus {
public:
  static constexpr std::uint32_t IoBase = 0x00ff'ffc0u;

  bool handles(std::uint32_t address) const;
  std::uint32_t read(std::uint32_t address);
  void write(std::uint32_t address, std::uint32_t value);
  void reset();
  void advance_time();

  void enqueue_uart_rx(std::uint8_t value);
  void enqueue_keyboard(std::uint8_t value);
  void set_mouse(std::uint16_t x, std::uint16_t y, std::uint8_t buttons);
  std::string take_uart_output();
  void attach_disk(DiskImage& disk) { spi_.attach_disk(disk); }
  void detach_disk() { spi_.detach_disk(); }
  bool has_disk() const { return spi_.has_disk(); }
  std::uint32_t leds() const { return leds_; }

private:
  std::uint32_t milliseconds_ = 0;
  std::uint8_t leds_ = 0;
  std::uint8_t gpio_out_ = 0;
  std::uint8_t gpio_control_ = 0;
  std::uint8_t uart_control_ = 0;
  std::deque<std::uint8_t> uart_rx_;
  std::deque<std::uint8_t> keyboard_;
  std::uint16_t mouse_x_ = 0;
  std::uint16_t mouse_y_ = 0;
  std::uint8_t mouse_buttons_ = 0;
  std::string uart_output_;
  SpiDevice spi_;
};

} // namespace oberon
