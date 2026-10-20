#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "recomp_thread_wait.hpp"

namespace cw {
// Host-side snapshot of the guest MIPS III / o32 CPU state. A native recomp
// function dispatcher must eventually restore it; this class does NOT execute
// guest instructions or model libultra's in-RDRAM OSThread layout.
struct MipsCpuContext {
  std::array<std::uint64_t, 32> gpr{};
  std::array<std::uint64_t, 32> fpr{};
  std::uint64_t hi=0, lo=0;
  std::uint32_t pc=0, next_pc=0;
  std::uint32_t cop0_status=0, cop0_cause=0, cop0_epc=0, cop0_count=0;
  std::uint32_t fcsr=0;
  bool in_delay_slot=false;
};

enum class GuestCpuPhase { Ready, Running, Waiting, Finished };

// Deterministic host-side thread-context bookkeeping layered on the tested
// libultra-compatible guest queue memory. A saved context is *data*, not an
// execution engine; no MIPS stack or host context switches occur here.
class GuestCpuContextScheduler final {
 public:
  explicit GuestCpuContextScheduler(NativeState& memory)
      : memory_(memory), waiter_(memory) {}
  bool createThread(std::uint32_t id, std::uint8_t priority,
                    std::uint32_t entry_pc, std::uint32_t guest_sp,
                    std::uint32_t argument=0);
  // Keeps the current running thread unless a READY higher-priority thread
  // exists. With no running thread, chooses highest-priority READY thread.
  std::uint32_t dispatch();
  bool yield();
  bool saveCurrent(const MipsCpuContext& context);
  // Atomic exit from a translated leaf into host, preserving final registers.
  bool finishCurrent(const MipsCpuContext& final_context);
  bool getContext(std::uint32_t id, MipsCpuContext* out) const;
  bool getPhase(std::uint32_t id, GuestCpuPhase* out) const;
  std::uint32_t current() const { return running_; }
  std::size_t threadCount() const { return threads_.size(); }
  // Operations of the running thread can leave it waiting without losing its
  // already-saved PC, HI/LO, COP0, GPR or FPR data.
  ThreadOpStatus recv(std::uint32_t queue, std::uint32_t* message, bool block);
  ThreadOpStatus send(std::uint32_t queue, std::uint32_t message, bool block);
  OsMqStatus postMessage(std::uint32_t queue, std::uint32_t message);
  std::size_t pump();
  bool takeCompletion(std::uint32_t id, GuestWaitCompletion* out);
 private:
  struct Thread {
    std::uint32_t id=0;
    std::uint8_t priority=0;
    GuestCpuPhase phase=GuestCpuPhase::Ready;
    std::uint64_t ready_sequence=0;
    MipsCpuContext context{};
    GuestWaitCompletion completion{};
    bool has_completion=false;
  };
  Thread* find(std::uint32_t id);
  const Thread* find(std::uint32_t id) const;
  void unblockCompletions();
  NativeState& memory_;
  GuestQueueWaiter waiter_;
  std::vector<Thread> threads_;
  std::uint32_t running_=0;
  std::uint64_t sequence_=0;
};
}  // namespace cw
