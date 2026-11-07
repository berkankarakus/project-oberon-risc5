#include "devices.hpp"

#include <stdexcept>

namespace oberon {

bool DeviceBus::handles(std::uint32_t address) const {
  const auto masked = address & 0x00ff'ffffu;
  return masked >= IoBase && masked < IoBase + 0x28u;
}

std::uint32_t DeviceBus::read(std::uint32_t address) {
  switch (static_cast<MmioRegister>((address - IoBase) & 0x3fu)) {
  case MmioRegister::Timer: return milliseconds_;
  case MmioRegister::Leds: return leds_;
  case MmioRegister::UartData: {
    const auto value = uart_rx_.empty() ? 0u : uart_rx_.front();
    if (!uart_rx_.empty()) uart_rx_.pop_front();
    return value;
  }
  case MmioRegister::UartControl: return (uart_rx_.empty() ? 0u : 1u) | (1u << 1);
  case MmioRegister::SpiData: {
    // The hardware register retains the last received word until the next
    // transfer. The boot ROM reads this value once for the response and may
    // read it again while decoding the SD-card status.
    return spi_.read_data();
  }
  case MmioRegister::SpiControl: return spi_.read_status();
  case MmioRegister::KeyboardStatus:
    return (static_cast<std::uint32_t>(mouse_buttons_ & 0x07u) << 24) |
           (static_cast<std::uint32_t>(mouse_y_ & 0x03ffu) << 12) | (mouse_x_ & 0x03ffu) |
           (keyboard_.empty() ? 0u : 1u) << 28;
  case MmioRegister::KeyboardData: {
    const auto value = keyboard_.empty() ? 0u : keyboard_.front();
    if (!keyboard_.empty()) keyboard_.pop_front();
    return value;
  }
  case MmioRegister::GpioData: return gpio_out_;
  case MmioRegister::GpioControl: return gpio_control_;
  default: return 0;
  }
}

void DeviceBus::write(std::uint32_t address, std::uint32_t value) {
  switch (static_cast<MmioRegister>((address - IoBase) & 0x3fu)) {
  case MmioRegister::Leds: leds_ = static_cast<std::uint8_t>(value); break;
  case MmioRegister::UartData: uart_output_.push_back(static_cast<char>(value & 0xffu)); break;
  case MmioRegister::UartControl: uart_control_ = static_cast<std::uint8_t>(value); break;
  case MmioRegister::SpiData: spi_.write_data(value); break;
  case MmioRegister::SpiControl: spi_.write_control(value); break;
  case MmioRegister::GpioData: gpio_out_ = static_cast<std::uint8_t>(value); break;
  case MmioRegister::GpioControl: gpio_control_ = static_cast<std::uint8_t>(value); break;
  default: break;
  }
}

void DeviceBus::reset() {
  milliseconds_ = 0;
  leds_ = gpio_out_ = gpio_control_ = uart_control_ = 0;
  spi_.reset();
  uart_rx_.clear();
  keyboard_.clear();
  mouse_x_ = mouse_y_ = 0;
  mouse_buttons_ = 0;
  uart_output_.clear();
}

void DeviceBus::advance_time() { ++milliseconds_; }

void DeviceBus::enqueue_uart_rx(std::uint8_t value) { uart_rx_.push_back(value); }

void DeviceBus::enqueue_keyboard(std::uint8_t value) { keyboard_.push_back(value); }

void DeviceBus::set_mouse(std::uint16_t x, std::uint16_t y, std::uint8_t buttons) {
  mouse_x_ = x & 0x03ffu;
  mouse_y_ = y & 0x03ffu;
  mouse_buttons_ = buttons & 0x07u;
}

std::string DeviceBus::take_uart_output() {
  auto output = uart_output_;
  uart_output_.clear();
  return output;
}

} // namespace oberon
