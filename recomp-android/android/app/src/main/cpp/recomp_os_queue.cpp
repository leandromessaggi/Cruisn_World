#include "recomp_os_queue.hpp"

namespace cw {
namespace {
constexpr bool aligned(std::uint32_t address) { return (address & 3u) == 0; }
}

bool GuestOsMesgQueue::create(std::uint32_t array, std::uint32_t count) {
  if (!state_.valid() || !aligned(queue_) || !aligned(array) ||
      count == 0 || count > kOsMaxMessages) return false;
  // Preflight the ENTIRE ring and queue structure before making any writes.
  // The end-pointer reads also reject wrapped and out-of-bounds guest ranges.
  std::uint32_t unused=0;
  if (queue_ > UINT32_MAX - (kOsMqBytes - 4) ||
      array > UINT32_MAX - ((count - 1) * 4) ||
      !state_.readU32(queue_, &unused) ||
      !state_.readU32(queue_ + kOsMqBytes - 4, &unused) ||
      !state_.readU32(array, &unused) ||
      !state_.readU32(array + (count - 1) * 4, &unused)) return false;
  // Layout identical to libultra's osCreateMesgQueue initialization.
  return state_.writeU32(queue_ + kOsMqMtqueue, 0) &&
         state_.writeU32(queue_ + kOsMqFullqueue, 0) &&
         state_.writeU32(queue_ + kOsMqValidCount, 0) &&
         state_.writeU32(queue_ + kOsMqFirst, 0) &&
         state_.writeU32(queue_ + kOsMqMsgCount, count) &&
         state_.writeU32(queue_ + kOsMqMsg, array);
}

bool GuestOsMesgQueue::get(Header* h) const {
  if (!state_.valid() || !h || !aligned(queue_) || queue_ > UINT32_MAX-(kOsMqBytes-4)) return false;
  return state_.readU32(queue_+0, &h->mtqueue) &&
         state_.readU32(queue_+4, &h->fullqueue) &&
         state_.readU32(queue_+8, &h->valid) &&
         state_.readU32(queue_+12, &h->first) &&
         state_.readU32(queue_+16, &h->capacity) &&
         state_.readU32(queue_+20, &h->messages);
}

bool GuestOsMesgQueue::validHeader(const Header& h) const {
  if (h.capacity == 0 || h.capacity > kOsMaxMessages ||
      h.valid > h.capacity || h.first >= h.capacity ||
      !aligned(h.messages) ||
      h.messages > UINT32_MAX - ((h.capacity-1)*4)) return false;
  std::uint32_t unused=0;
  return state_.readU32(h.messages, &unused) &&
         state_.readU32(h.messages + (h.capacity-1)*4, &unused);
}

OsMqStatus GuestOsMesgQueue::canSend() const {
  Header h{};
  if (!get(&h) || !validHeader(h)) return OsMqStatus::Invalid;
  return h.valid == h.capacity ? OsMqStatus::Full : OsMqStatus::Ok;
}

OsMqStatus GuestOsMesgQueue::canReceive() const {
  Header h{};
  if (!get(&h) || !validHeader(h)) return OsMqStatus::Invalid;
  return h.valid == 0 ? OsMqStatus::Empty : OsMqStatus::Ok;
}

bool GuestOsMesgQueue::size(std::uint32_t* out) const {
  Header h{};
  if (!out || !get(&h) || !validHeader(h)) return false;
  *out=h.valid;
  return true;
}

OsMqStatus GuestOsMesgQueue::send(std::uint32_t message) {
  Header h{};
  if (!get(&h) || !validHeader(h)) return OsMqStatus::Invalid;
  if (h.valid == h.capacity) return OsMqStatus::Full;
  const std::uint32_t pos = (h.first + h.valid) % h.capacity;
  if (!state_.writeU32(h.messages + pos * 4, message)) return OsMqStatus::Invalid;
  if (!state_.writeU32(queue_ + kOsMqValidCount, h.valid+1)) return OsMqStatus::Invalid;
  return OsMqStatus::Ok;
}

OsMqStatus GuestOsMesgQueue::receive(std::uint32_t* message) {
  Header h{};
  if (!message || !get(&h) || !validHeader(h)) return OsMqStatus::Invalid;
  if (h.valid == 0) return OsMqStatus::Empty;
  std::uint32_t item=0;
  if (!state_.readU32(h.messages + h.first*4, &item)) return OsMqStatus::Invalid;
  if (!state_.writeU32(queue_ + kOsMqFirst, (h.first + 1) % h.capacity) ||
      !state_.writeU32(queue_ + kOsMqValidCount, h.valid - 1)) return OsMqStatus::Invalid;
  *message=item;
  return OsMqStatus::Ok;
}

bool GuestPiOsBridge::startRead(std::uint32_t cart, std::uint32_t address,
                                std::size_t bytes, std::uint32_t message) {
  // No new transfer before the preceding event has entered guest memory.
  if (pending_.has_value() || pi_.busy() || host_messages_.size() != 0 ||
      guest_.canSend() == OsMqStatus::Invalid) return false;
  return pi_.startRead(cart,address,bytes,message);
}

void GuestPiOsBridge::service() {
  if (pending_.has_value()) {
    const OsMqStatus status=guest_.send(*pending_);
    if (status == OsMqStatus::Ok) { pending_.reset(); ++notified_; }
    return;
  }
  pi_.service();
  std::uint32_t message=0;
  if (host_messages_.receive(&message)) {
    const OsMqStatus status=guest_.send(message);
    if (status == OsMqStatus::Ok) ++notified_;
    else pending_=message;  // full OR invalid queue: retain event, never drop
  }
}
}
