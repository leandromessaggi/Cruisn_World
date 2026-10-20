#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
namespace cw {
// Independent native service primitives. Not yet wired into libultra.
constexpr std::uint64_t kCountHz=46875000ull; // R4300 COP0 Count (half CPU rate)
constexpr std::uint64_t kViRateNumerator=60000ull, kViRateDenominator=1001ull;
class GuestClock final {
 public:
  void tickVi();
  std::uint64_t totalCount() const { return count_; }
  std::uint32_t cop0Count() const { return static_cast<std::uint32_t>(count_); }
  std::uint64_t viCount() const { return frames_; }
 private:
  std::uint64_t frames_=0, count_=0;
};
class GuestMessageQueue final {
 public:
  explicit GuestMessageQueue(std::size_t capacity);
  bool send(std::uint32_t message);
  bool receive(std::uint32_t* message);
  std::size_t size() const { return messages_.size(); }
 private:
  std::size_t capacity_;
  std::deque<std::uint32_t> messages_;
};
constexpr std::uint16_t kButtonA=0x8000, kButtonB=0x4000, kButtonZ=0x2000,
  kButtonStart=0x1000, kDpadUp=0x0800, kDpadDown=0x0400,
  kDpadLeft=0x0200, kDpadRight=0x0100, kButtonL=0x0020,
  kButtonR=0x0010, kCUp=0x0008, kCDown=0x0004,
  kCLeft=0x0002, kCRight=0x0001;
struct ControllerSample {
  std::uint16_t buttons=0;
  std::int8_t stick_x=0, stick_y=0;
  std::array<std::uint8_t,4> serialize() const;
};
// The N64's EEPROM 16K is 16 kilobits = 2 KiB, not 16 kilobytes.
// Each physical block contains exactly 8 bytes.
class Eeprom16k final {
 public:
  static constexpr std::size_t kBlockSize=8, kTotalBytes=2048, kBlocks=256;
  Eeprom16k();
  bool load(const std::string& private_path,std::string* error=nullptr);
  bool save(std::string* error=nullptr) const;
  bool readBlock(unsigned index,std::uint8_t* dest,std::size_t length) const;
  bool writeBlock(unsigned index,const std::uint8_t* src,std::size_t length);
 private:
  std::array<std::uint8_t,kTotalBytes> data_{};
  std::string path_;
};
}