#include "recomp_pi.hpp"
#include <limits>

namespace cw {
bool GuestPiDma::startRead(std::uint32_t cart_physical,
                           std::uint32_t ram_virtual, std::size_t length,
                           std::uint32_t completion_message) {
  if (busy_ || cart_physical < kCartBase || length == 0 ||
      length > kRomSize || clock_.viCount() ==
          std::numeric_limits<std::uint64_t>::max()) return false;
  const std::size_t rom_offset =
      static_cast<std::size_t>(cart_physical - kCartBase);
  if (!ram_.canDmaRead(rom_offset, ram_virtual, length)) return false;
  rom_offset_ = rom_offset;
  length_ = length;
  dst_ = ram_virtual;
  message_ = completion_message;
  ready_vi_ = clock_.viCount() + 1;
  busy_ = true;
  return true;
}

bool GuestPiDma::service() {
  if (!busy_ || clock_.viCount() < ready_vi_ ||
      completions_.size() >= completions_.capacity()) return false;
  if (!ram_.dmaRead(rom_offset_, dst_, length_)) return false;
  if (!completions_.send(message_)) return false;
  busy_ = false;
  ++completed_;
  return true;
}
} // namespace cw
