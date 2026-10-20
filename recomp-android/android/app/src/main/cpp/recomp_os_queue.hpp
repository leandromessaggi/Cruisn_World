#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include "recomp_memory.hpp"
#include "recomp_pi.hpp"

namespace cw {
// libultra's N64 o32 OSMesgQueue ABI (six 32-bit words, big endian).
// mtqueue/fullqueue are pointers to OS thread wait lists. Only NONBLOCKING
// operations are implemented; this is not osRecvMesg's blocking scheduler.
constexpr std::uint32_t kOsMqMtqueue = 0x00;
constexpr std::uint32_t kOsMqFullqueue = 0x04;
constexpr std::uint32_t kOsMqValidCount = 0x08;
constexpr std::uint32_t kOsMqFirst = 0x0C;
constexpr std::uint32_t kOsMqMsgCount = 0x10;
constexpr std::uint32_t kOsMqMsg = 0x14;
constexpr std::uint32_t kOsMqBytes = 0x18;
constexpr std::uint32_t kOsMaxMessages = 1024;

enum class OsMqStatus { Ok, Empty, Full, Invalid };

// Operates entirely on guest RDRAM. Queue structure and messages stay at the
// N64 addresses visible to statically recompiled MIPS code; no host pointers
// are written into guest memory. Deliberately disallows BLOCKING operations.
class GuestOsMesgQueue final {
 public:
  GuestOsMesgQueue(NativeState& state, std::uint32_t queue_address)
      : state_(state), queue_(queue_address) {}
  bool create(std::uint32_t message_array, std::uint32_t capacity);
  OsMqStatus send(std::uint32_t message);
  OsMqStatus receive(std::uint32_t* message);
  OsMqStatus canSend() const;
  OsMqStatus canReceive() const;
  bool size(std::uint32_t* out) const;
  std::uint32_t guestAddress() const { return queue_; }
 private:
  struct Header {
    std::uint32_t mtqueue, fullqueue, valid, first, capacity, messages;
  };
  bool get(Header* out) const;
  bool validHeader(const Header& h) const;
  NativeState& state_;
  const std::uint32_t queue_;
};

// Connects an already-tested PI DMA staging engine to a real guest OSMesgQueue
// structure. The DMA runs once; full guest queues backpressure the completion
// message, which is held on the host until the guest consumes a slot.
// Still NOT the game's actual osPiStartDma symbol or libultra thread runtime.
class GuestPiOsBridge final {
 public:
  GuestPiOsBridge(NativeState& state, const GuestClock& clock,
                  GuestOsMesgQueue& guest_queue)
      : host_messages_(1), pi_(state, clock, host_messages_), guest_(guest_queue) {}
  bool startRead(std::uint32_t cart_addr, std::uint32_t ram_addr,
                 std::size_t size, std::uint32_t completion_message);
  void service();
  bool busy() const { return pi_.busy() || pending_.has_value(); }
  bool pendingMessage() const { return pending_.has_value(); }
  std::uint64_t completed() const { return pi_.completed(); }
  std::uint64_t notifications() const { return notified_; }
 private:
  GuestMessageQueue host_messages_;
  GuestPiDma pi_;
  GuestOsMesgQueue& guest_;
  std::optional<std::uint32_t> pending_;
  std::uint64_t notified_=0;
};
}
