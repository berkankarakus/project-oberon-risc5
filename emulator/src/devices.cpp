#include "devices.hpp"

#include <stdexcept>

namespace oberon {

bool DeviceBus::handles(std::uint32_t address) const {
  const auto masked = address & 0x00ff'ffffu;
  return masked >= IoBase && masked < IoBase + 0x28u;
}

std::uint32_t DeviceBus::read(std::uint32_t address) {
  switch ((address - IoBase) & 0x3fu) {
  case 0x00: return milliseconds_;
  case 0x04: return leds_;
  case 0x08: {
    const auto value = uart_rx_.empty() ? 0u : uart_rx_.front();
    if (!uart_rx_.empty()) uart_rx_.pop_front();
    return value;
  }
  case 0x0c: return (uart_rx_.empty() ? 0u : 1u) | (1u << 1);
  case 0x10: {
    const auto value = spi_rx_;
    spi_rx_ = 0xffff'ffffu;
    return value;
  }
  case 0x14: return 1u;
  case 0x18: return (keyboard_.empty() ? 0u : 1u) << 28;
  case 0x1c: {
    const auto value = keyboard_.empty() ? 0u : keyboard_.front();
    if (!keyboard_.empty()) keyboard_.pop_front();
    return value;
  }
  case 0x20: return gpio_out_;
  case 0x24: return gpio_control_;
  default: return 0;
  }
}

void DeviceBus::write(std::uint32_t address, std::uint32_t value) {
  switch ((address - IoBase) & 0x3fu) {
  case 0x04: leds_ = static_cast<std::uint8_t>(value); break;
  case 0x08: uart_output_.push_back(static_cast<char>(value & 0xffu)); break;
  case 0x0c: uart_control_ = static_cast<std::uint8_t>(value); break;
  case 0x10: spi_rx_ = spi_exchange_word(value); break;
  case 0x14: {
    const auto next = static_cast<std::uint8_t>(value);
    spi_selected_ = (next & 1u) != 0;
    if (!spi_selected_) {
      spi_command_size_ = 0;
      spi_response_.clear();
      spi_write_active_ = false;
    }
    spi_control_ = next;
    break;
  }
  case 0x20: gpio_out_ = static_cast<std::uint8_t>(value); break;
  case 0x24: gpio_control_ = static_cast<std::uint8_t>(value); break;
  default: break;
  }
}

void DeviceBus::reset() {
  milliseconds_ = 0;
  leds_ = gpio_out_ = gpio_control_ = spi_control_ = uart_control_ = 0;
  spi_rx_ = 0xffff'ffffu;
  spi_selected_ = false;
  spi_app_command_ = false;
  spi_write_active_ = false;
  spi_write_sector_ = 0;
  spi_write_offset_ = 0;
  spi_command_.fill(0);
  spi_command_size_ = 0;
  spi_response_.clear();
  uart_rx_.clear();
  keyboard_.clear();
  uart_output_.clear();
}

void DeviceBus::tick() { ++milliseconds_; }

void DeviceBus::enqueue_uart_rx(std::uint8_t value) { uart_rx_.push_back(value); }

void DeviceBus::enqueue_keyboard(std::uint8_t value) { keyboard_.push_back(value); }

std::string DeviceBus::take_uart_output() {
  auto output = uart_output_;
  uart_output_.clear();
  return output;
}

std::uint8_t DeviceBus::spi_exchange_byte(std::uint8_t value) {
  if (!spi_selected_ || disk_ == nullptr) return 0xff;
  if (!spi_response_.empty()) {
    const auto response = spi_response_.front();
    spi_response_.pop_front();
    return response;
  }
  if (spi_write_active_) {
    if (spi_write_offset_ == 0) {
      if (value == 0xfe) spi_write_offset_ = 1;
      return 0xff;
    }
    if (spi_write_offset_ <= DiskImage::SectorSize) {
      spi_write_buffer_[spi_write_offset_ - 1] = value;
      ++spi_write_offset_;
      return 0xff;
    }
    if (spi_write_offset_ == DiskImage::SectorSize + 1) {
      ++spi_write_offset_;
      return 0xff;
    }
    disk_->write(spi_write_sector_, spi_write_buffer_);
    spi_write_active_ = false;
    spi_write_offset_ = 0;
    return 0x05;
  }
  if (spi_command_size_ == 0 && value == 0xff) return 0xff;
  spi_command_[spi_command_size_++] = value;
  if (spi_command_size_ == spi_command_.size()) spi_command_complete();
  return 0xff;
}

std::uint32_t DeviceBus::spi_exchange_word(std::uint32_t value) {
  if ((spi_control_ & 4u) == 0) return spi_exchange_byte(static_cast<std::uint8_t>(value));
  std::uint32_t result = 0;
  for (unsigned i = 0; i < 4; ++i) {
    result |= static_cast<std::uint32_t>(spi_exchange_byte(static_cast<std::uint8_t>(value >> (i * 8)))) << (i * 8);
  }
  return result;
}

void DeviceBus::spi_command_complete() {
  const auto command = spi_command_[0] & 0x3f;
  const auto argument = (static_cast<std::uint32_t>(spi_command_[1]) << 24) |
                        (static_cast<std::uint32_t>(spi_command_[2]) << 16) |
                        (static_cast<std::uint32_t>(spi_command_[3]) << 8) |
                        spi_command_[4];
  spi_command_size_ = 0;
  switch (command) {
  case 0: spi_response_.push_back(0x01); break;
  case 8: spi_response_ = {0x01, 0, 0, 1, 0xaa}; break;
  case 55: spi_app_command_ = true; spi_response_.push_back(0x01); break;
  case 41: spi_response_.push_back(spi_app_command_ ? 0x00 : 0x05); spi_app_command_ = false; break;
  case 58: spi_response_ = {0x00, 0x40, 0xff, 0x80, 0x00}; break;
  case 17: {
    spi_response_.push_back(0x00);
    const auto sector = argument < disk_->sector_count() ? argument : 0;
    const auto data = disk_->read(sector);
    spi_response_.push_back(0xfe);
    spi_response_.insert(spi_response_.end(), data.begin(), data.end());
    spi_response_.push_back(0xff);
    spi_response_.push_back(0xff);
    break;
  }
  case 24:
    spi_response_.push_back(0x00);
    spi_write_active_ = true;
    spi_write_sector_ = argument < disk_->sector_count() ? argument : 0;
    spi_write_offset_ = 0;
    spi_write_buffer_.fill(0);
    break;
  default: spi_response_.push_back(0x04); break;
  }
}

} // namespace oberon
