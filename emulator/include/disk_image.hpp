#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

namespace oberon {

class DiskImage {
public:
  static constexpr std::size_t SectorSize = 512;
  using Sector = std::array<std::uint8_t, SectorSize>;

  DiskImage() = default;
  ~DiskImage();

  DiskImage(const DiskImage&) = delete;
  DiskImage& operator=(const DiskImage&) = delete;

  void create(const std::string& path, std::uint32_t sectors);
  void open(const std::string& path, bool writable = true);
  void close();

  std::uint32_t sector_count() const { return sectors_; }
  const std::string& path() const { return path_; }
  bool writable() const { return writable_; }

  Sector read(std::uint32_t sector);
  void write(std::uint32_t sector, const Sector& data);

private:
  void check_sector(std::uint32_t sector) const;

  std::fstream file_;
  std::string path_;
  std::uint32_t sectors_ = 0;
  bool writable_ = false;
};

} // namespace oberon
