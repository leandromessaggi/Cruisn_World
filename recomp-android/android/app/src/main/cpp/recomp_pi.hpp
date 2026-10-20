#pragma once
#include <cstddef>
#include <cstdint>
#include "recomp_memory.hpp"
#include "recomp_devices.hpp"

namespace cw {
constexpr std::uint32_t kCartBase = 0x10000000u;
// Native staging bridge, not a cycle-accurate PI or osPiStartDma.
class GuestPiDma final {
 public:
  GuestPiDma(NativeState& ram, const GuestClock& clock,
             GuestMessageQueue& completion_queue)
      : ram_(ram), clock_(clock), completions_(completion_queue) {}
  bool startRead(std::uint32_t cart_physical, std::uint32_t ram_virtual,
                 std::size_t length, std::uint32_t completion_message);
  bool service();
  bool busy() const { return busy_; }
  std::uint64_t completed() const { return completed_; }
 private:
  NativeState& ram_;
  const GuestClock& clock_;
  GuestMessageQueue& completions_;
  bool busy_ = false;
  std::size_t rom_offset_ = 0;
  std::size_t length_ = 0;
  std::uint32_t dst_ = 0;
  std::uint32_t message_ = 0;
  std::uint64_t ready_vi_ = 0;
  std::uint64_t completed_ = 0;
};
} // namespace cw
