#pragma once
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include "recomp_context.hpp"
#include "recomp_functions.hpp"

namespace cw {
// Only ROM-verified, statically translated C++ bodies and continuations run.
// The verified 0x802AF6EC -> 0x802AF5F4 call is intentionally split across
// guest PC boundaries to exercise the o32 stack and RA save/restore protocol.
// This is NOT a generic MIPS interpreter or an N64 execution runtime.
enum class RecompStepResult {
  Executed, Called, Resumed, Returned, NotRecompiled, NoReadyThread,
  InvalidReturn, InvalidFrame, Failed
};
class GuestRecompDispatcher final {
 public:
  GuestRecompDispatcher(NativeState& memory, GuestCpuContextScheduler& scheduler)
      : memory_(memory), scheduler_(scheduler) {}
  RecompStepResult step(CallTrace* trace=nullptr);
  std::size_t run(std::size_t budget, CallTrace* trace=nullptr);
  std::size_t pendingCallFrames() const { return pending_frames_.size(); }
 private:
  struct WrapperFrame {
    std::uint32_t guest_sp;
    std::uint32_t caller_ra;
  };
  RecompStepResult beginVerifiedWrapper(std::uint32_t thread, MipsCpuContext& context,
                                        CallTrace* trace);
  RecompStepResult resumeVerifiedWrapper(std::uint32_t thread, MipsCpuContext& context,
                                         CallTrace* trace);
  NativeState& memory_;
  GuestCpuContextScheduler& scheduler_;
  // Per-thread host metadata guards resuming a continuation from an arbitrary PC.
  // Actual MIPS RA and SP are also stored in guest registers/RDRAM.
  std::unordered_map<std::uint32_t, WrapperFrame> pending_frames_;
};
}
