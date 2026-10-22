#include "recomp_vi_events.hpp"
#include <chrono>
#include <thread>

namespace cw {
bool GuestViEventBridge::setRetrace(std::uint32_t queue,
                                     std::uint32_t message,
                                     std::uint32_t retrace_count) {
  // Reject invalid ABI arguments before changing the active registration.
  // A queue can be full at registration time and still be a valid queue.
  if (!memory_.valid() || retrace_count == 0 || retrace_count > 1024 ||
      GuestOsMesgQueue(memory_,queue).canSend() == OsMqStatus::Invalid)
    return false;
  queue_=queue;
  message_=message;
  interval_=retrace_count;
  registered_at_=clock_.viCount();
  return true;
}

ViTickResult GuestViEventBridge::tick() {
  clock_.tickVi();
  if (!registered()) return ViTickResult::Unregistered;
  if ((clock_.viCount() - registered_at_) % interval_ != 0)
    return ViTickResult::NotDue;
  const OsMqStatus status=scheduler_.postMessage(queue_,message_);
  switch (status) {
    case OsMqStatus::Ok: ++delivered_; return ViTickResult::Delivered;
    case OsMqStatus::Full: ++dropped_; return ViTickResult::DroppedFull;
    case OsMqStatus::Invalid: ++invalid_; return ViTickResult::InvalidQueue;
    case OsMqStatus::Empty: break;
  }
  ++invalid_;
  return ViTickResult::InvalidQueue;
}
std::uint32_t GuestViHostPacer::runTicks(GuestViEventBridge& bridge,
                                        std::uint32_t ticks) {
  // Cap synchronous work to ten seconds. The Android Activity calls this
  // only from a background Java worker, never from the UI thread.
  if (ticks == 0 || ticks > 600) return 0;
  const auto start=std::chrono::steady_clock::now();
  for (std::uint32_t n=1; n<=ticks; ++n) {
    const auto due=start+std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(static_cast<double>(n)/60.0));
    std::this_thread::sleep_until(due);
    bridge.tick();
  }
  return ticks;
}
} // namespace cw
