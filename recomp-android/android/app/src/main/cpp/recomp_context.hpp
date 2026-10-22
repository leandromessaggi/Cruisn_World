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
// Entry-point-neutral o32 libultra message calls. The translated caller must
// save its continuation PC before invoking this adapter.
enum class GuestQueueCall { Send, Receive };

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
  // libultra-style receive: optional 32-bit guest OSMesg* destination.
  // Caller saves the translated continuation PC before a blocking call.
  // Completion updates the guest destination and saved MIPS v0 register.
  ThreadOpStatus recvGuest(std::uint32_t queue, std::uint32_t guest_message,
                           bool block);
  ThreadOpStatus send(std::uint32_t queue, std::uint32_t message, bool block);
  // Guest osSendMesg ABI: updates saved MIPS v0 immediately or on wakeup.
  ThreadOpStatus sendGuest(std::uint32_t queue, std::uint32_t message, bool block);
  // Reads o32 a0=queue, a1=message/pointer, a2=OS_MESG_NOBLOCK(0) or
  // OS_MESG_BLOCK(1). Does not advance PC or resolve libultra symbol addresses.
  ThreadOpStatus invokeQueueCall(GuestQueueCall call);
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
    bool guest_receive_pending=false;
    bool guest_send_pending=false;
    std::uint32_t guest_receive_destination=0;
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
