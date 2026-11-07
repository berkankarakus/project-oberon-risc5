#include "spi_device.hpp"

namespace oberon {

void SpiDevice::reset() {
  spi_control_ = 0;
  spi_rx_ = 0xffff'ffffu;
  spi_selected_ = false;
  spi_app_command_ = false;
  spi_write_active_ = false;
  spi_write_sector_ = 0;
  spi_write_offset_ = 0;
  spi_command_.fill(0);
  spi_command_size_ = 0;
  spi_response_.clear();
}

void SpiDevice::write_data(std::uint32_t value) { spi_rx_ = exchange_word(value); }

void SpiDevice::write_control(std::uint32_t value) {
  const auto next = static_cast<std::uint8_t>(value);
  spi_selected_ = (next & spi_control::Card0) != 0;
  if (!spi_selected_) {
    spi_command_size_ = 0;
    spi_response_.clear();
    spi_write_active_ = false;
  }
  spi_control_ = next;
}

std::uint8_t SpiDevice::exchange_byte(std::uint8_t value) {
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
  if (spi_command_size_ == spi_command_.size()) command_complete();
  return 0xff;
}

std::uint32_t SpiDevice::exchange_word(std::uint32_t value) {
  if ((spi_control_ & spi_control::Fast) == 0) return exchange_byte(static_cast<std::uint8_t>(value));
  std::uint32_t result = 0;
  for (unsigned i = 0; i < 4; ++i) {
    result |= static_cast<std::uint32_t>(exchange_byte(static_cast<std::uint8_t>(value >> (i * 8)))) << (i * 8);
  }
  return result;
}

void SpiDevice::command_complete() {
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
