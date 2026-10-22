#pragma once
#include <cstdint>
#include "recomp_context.hpp"
#include "recomp_devices.hpp"

namespace cw {
// Explicit VI ticks route messages to guest RDRAM queues and wake waiters.
// No hardware VI timer, graphics backend, or N64 execution is provided here.
enum class ViTickResult { Unregistered, NotDue, Delivered, DroppedFull, InvalidQueue };

class GuestViEventBridge final {
 public:
  GuestViEventBridge(NativeState& memory, GuestClock& clock,
                     GuestCpuContextScheduler& scheduler)
      : memory_(memory), clock_(clock), scheduler_(scheduler) {}
  bool setRetrace(std::uint32_t queue, std::uint32_t message,
                  std::uint32_t retrace_count);
  ViTickResult tick();
  std::uint64_t delivered() const { return delivered_; }
  std::uint64_t dropped() const { return dropped_; }
  std::uint64_t invalid() const { return invalid_; }
  std::uint32_t interval() const { return interval_; }
  bool registered() const { return interval_ != 0; }
 private:
  NativeState& memory_;
  GuestClock& clock_;
  GuestCpuContextScheduler& scheduler_;
  std::uint32_t queue_=0, message_=0, interval_=0;
  std::uint64_t registered_at_=0, delivered_=0, dropped_=0, invalid_=0;
};
} // namespace cw
