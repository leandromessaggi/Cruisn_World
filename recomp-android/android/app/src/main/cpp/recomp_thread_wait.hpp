#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "recomp_os_queue.hpp"

namespace cw {
// Cooperative model of libultra message-queue blocking semantics. Guest thread
// addresses are opaque 32-bit identifiers: host pointers never enter RDRAM.
// This is NOT an execution-context scheduler for MIPS CPU threads.
enum class ThreadOpStatus { Complete, Blocked, WouldBlock, Invalid };
enum class ThreadWaitKind { None, Send, Receive };

struct GuestWaitCompletion {
  ThreadOpStatus status=ThreadOpStatus::Invalid;
  ThreadWaitKind operation=ThreadWaitKind::None;
  std::uint32_t message=0; // populated for completed blocking receives
};

class GuestQueueWaiter final {
 public:
  explicit GuestQueueWaiter(NativeState& memory) : memory_(memory) {}
  bool registerThread(std::uint32_t guest_thread, std::uint8_t priority);
  ThreadOpStatus send(std::uint32_t guest_thread, std::uint32_t queue,
                      std::uint32_t message, bool blocking);
  ThreadOpStatus receive(std::uint32_t guest_thread, std::uint32_t queue,
                         std::uint32_t* message, bool blocking);
  // Resumes pending queue operations in priority order. No guest MIPS executed.
  std::size_t pump();
  bool takeCompletion(std::uint32_t guest_thread, GuestWaitCompletion* out);
  std::size_t waiting() const;
 private:
  struct Thread {
    std::uint32_t guest=0, queue=0, message=0;
    std::uint8_t priority=0;
    ThreadWaitKind wait=ThreadWaitKind::None;
    GuestWaitCompletion completion{};
    bool completed=false;
  };
  Thread* find(std::uint32_t guest_thread);
  NativeState& memory_;
  std::vector<Thread> threads_;
};
} // namespace cw
