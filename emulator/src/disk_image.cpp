#include "disk_image.hpp"

#include <limits>
#include <stdexcept>

namespace oberon {

DiskImage::~DiskImage() { close(); }

void DiskImage::create(const std::string& path, std::uint32_t sectors) {
  if (sectors == 0) throw std::invalid_argument("disk image must contain at least one sector");
  close();
  {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot create disk image: " + path);
    Sector zero{};
    for (std::uint32_t i = 0; i < sectors; ++i) {
      output.write(reinterpret_cast<const char*>(zero.data()), static_cast<std::streamsize>(zero.size()));
    }
    if (!output) throw std::runtime_error("cannot size disk image: " + path);
  }
  open(path, true);
}

void DiskImage::open(const std::string& path, bool writable) {
  close();
  auto mode = std::ios::binary | std::ios::in;
  if (writable) mode |= std::ios::out;
  file_.open(path, mode);
  if (!file_) throw std::runtime_error("cannot open disk image: " + path);

  file_.seekg(0, std::ios::end);
  const auto size = file_.tellg();
  if (size < 0 || size % static_cast<std::streamoff>(SectorSize) != 0 ||
      size / static_cast<std::streamoff>(SectorSize) > std::numeric_limits<std::uint32_t>::max()) {
    close();
    throw std::runtime_error("disk image size is not a whole number of 512-byte sectors: " + path);
  }
  sectors_ = static_cast<std::uint32_t>(size / static_cast<std::streamoff>(SectorSize));
  path_ = path;
  writable_ = writable;
}

void DiskImage::close() {
  if (file_.is_open()) file_.close();
  path_.clear();
  sectors_ = 0;
  writable_ = false;
}

void DiskImage::check_sector(std::uint32_t sector) const {
  if (!file_.is_open()) throw std::runtime_error("disk image is not open");
  if (sector >= sectors_) throw std::out_of_range("disk sector is outside the image");
}

DiskImage::Sector DiskImage::read(std::uint32_t sector) {
  check_sector(sector);
  Sector data{};
  file_.clear();
  file_.seekg(static_cast<std::streamoff>(sector) * SectorSize, std::ios::beg);
  file_.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
  if (!file_) throw std::runtime_error("cannot read disk sector");
  return data;
}

void DiskImage::write(std::uint32_t sector, const Sector& data) {
  check_sector(sector);
  if (!writable_) throw std::runtime_error("disk image is read-only");
  file_.clear();
  file_.seekp(static_cast<std::streamoff>(sector) * SectorSize, std::ios::beg);
  file_.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  file_.flush();
  if (!file_) throw std::runtime_error("cannot write disk sector");
}

} // namespace oberon
