#include "recomp_thread_wait.hpp"
#include <algorithm>

namespace cw {
GuestQueueWaiter::Thread* GuestQueueWaiter::find(std::uint32_t id) {
  for (auto& thread : threads_) if (thread.guest == id) return &thread;
  return nullptr;
}

bool GuestQueueWaiter::registerThread(std::uint32_t id, std::uint8_t priority) {
  if ((id & 3u) != 0 || id == 0 || threads_.size() >= 64 || find(id)) return false;
  Thread thread{};
  thread.guest=id;
  thread.priority=priority;
  threads_.push_back(thread);
  return true;
}

ThreadOpStatus GuestQueueWaiter::send(std::uint32_t id, std::uint32_t queue,
                                     std::uint32_t message, bool blocking) {
  Thread* t=find(id);
  if (!t || t->wait!=ThreadWaitKind::None || t->completed) return ThreadOpStatus::Invalid;
  GuestOsMesgQueue q(memory_,queue);
  const OsMqStatus result=q.send(message);
  if (result==OsMqStatus::Ok) {
    pump();
    return ThreadOpStatus::Complete;
  }
  if (result==OsMqStatus::Invalid) return ThreadOpStatus::Invalid;
  if (!blocking) return ThreadOpStatus::WouldBlock;
  t->wait=ThreadWaitKind::Send;
  t->queue=queue;
  t->message=message;
  return ThreadOpStatus::Blocked;
}

ThreadOpStatus GuestQueueWaiter::receive(std::uint32_t id, std::uint32_t queue,
                                        std::uint32_t* message, bool blocking) {
  Thread* t=find(id);
  if (!t || !message || t->wait!=ThreadWaitKind::None || t->completed)
    return ThreadOpStatus::Invalid;
  GuestOsMesgQueue q(memory_,queue);
  const OsMqStatus result=q.receive(message);
  if (result==OsMqStatus::Ok) {
    pump();
    return ThreadOpStatus::Complete;
  }
  if (result==OsMqStatus::Invalid) return ThreadOpStatus::Invalid;
  if (!blocking) return ThreadOpStatus::WouldBlock;
  t->wait=ThreadWaitKind::Receive;
  t->queue=queue;
  return ThreadOpStatus::Blocked;
}

std::size_t GuestQueueWaiter::pump() {
  // Revisit waiters after every completed operation: the transfer may unblock
  // another waiting thread. Stable sort preserves registration order for ties.
  std::vector<std::size_t> order;
  order.reserve(threads_.size());
  for (std::size_t i=0;i<threads_.size();++i) order.push_back(i);
  std::stable_sort(order.begin(),order.end(),[this](std::size_t lhs,std::size_t rhs) {
    return threads_[lhs].priority > threads_[rhs].priority;
  });
  std::size_t done=0;
  bool progressed=true;
  while (progressed) {
    progressed=false;
    for (std::size_t index : order) {
      Thread& t=threads_[index];
      if (t.wait==ThreadWaitKind::None) continue;
      GuestOsMesgQueue q(memory_,t.queue);
      std::uint32_t value=0;
      const OsMqStatus status=(t.wait==ThreadWaitKind::Send)
        ? q.send(t.message) : q.receive(&value);
      if (status==OsMqStatus::Full || status==OsMqStatus::Empty) continue;
      t.completion={status==OsMqStatus::Ok ? ThreadOpStatus::Complete : ThreadOpStatus::Invalid,
                    t.wait, t.wait==ThreadWaitKind::Receive ? value : t.message};
      t.completed=true;
      t.wait=ThreadWaitKind::None;
      ++done;
      progressed=true;
    }
  }
  return done;
}

bool GuestQueueWaiter::takeCompletion(std::uint32_t id, GuestWaitCompletion* out) {
  Thread* t=find(id);
  if (!t || !out || !t->completed) return false;
  *out=t->completion;
  t->completed=false;
  t->completion={};
  return true;
}

std::size_t GuestQueueWaiter::waiting() const {
  return std::count_if(threads_.begin(),threads_.end(),[](const Thread& t) {
    return t.wait!=ThreadWaitKind::None;
  });
}
} // namespace cw
