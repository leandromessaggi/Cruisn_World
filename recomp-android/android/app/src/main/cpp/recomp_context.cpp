#include "recomp_context.hpp"

namespace cw {
GuestCpuContextScheduler::Thread* GuestCpuContextScheduler::find(std::uint32_t id) {
  for (auto& t : threads_) if (t.id==id) return &t;
  return nullptr;
}
const GuestCpuContextScheduler::Thread* GuestCpuContextScheduler::find(std::uint32_t id) const {
  for (const auto& t : threads_) if (t.id==id) return &t;
  return nullptr;
}

bool GuestCpuContextScheduler::createThread(std::uint32_t id, std::uint8_t priority,
                                             std::uint32_t entry_pc,
                                             std::uint32_t guest_sp,
                                             std::uint32_t argument) {
  if (!memory_.valid() || id==0 || (id&3u)!=0 || (entry_pc&3u)!=0 ||
      (guest_sp&7u)!=0 || guest_sp<8u || threads_.size()>=64 || find(id)) return false;
  std::uint32_t scratch=0;
  if (!memory_.readU32(id,&scratch) || !memory_.readU32(entry_pc,&scratch) ||
      !memory_.readU32(guest_sp-8u,&scratch)) return false;
  if (!waiter_.registerThread(id,priority)) return false;
  Thread thread{};
  thread.id=id;
  thread.priority=priority;
  thread.ready_sequence=sequence_++;
  thread.context.pc=entry_pc;
  thread.context.next_pc=entry_pc+4u;
  thread.context.gpr[4]=argument; // a0, o32 initial argument
  thread.context.gpr[29]=guest_sp; // sp
  threads_.push_back(thread);
  return true;
}

std::uint32_t GuestCpuContextScheduler::dispatch() {
  Thread* current_thread=find(running_);
  Thread* choice=nullptr;
  for (auto& t : threads_) {
    if (t.phase!=GuestCpuPhase::Ready) continue;
    if (!choice || t.priority>choice->priority ||
        (t.priority==choice->priority && t.ready_sequence<choice->ready_sequence))
      choice=&t;
  }
  if (current_thread && current_thread->phase==GuestCpuPhase::Running &&
      (!choice || choice->priority<=current_thread->priority)) return running_;
  if (!choice) return running_;
  if (current_thread && current_thread->phase==GuestCpuPhase::Running) {
    current_thread->phase=GuestCpuPhase::Ready;
    current_thread->ready_sequence=sequence_++;
  }
  choice->phase=GuestCpuPhase::Running;
  running_=choice->id;
  return running_;
}

bool GuestCpuContextScheduler::yield() {
  Thread* active=find(running_);
  if (!active || active->phase!=GuestCpuPhase::Running) return false;
  active->phase=GuestCpuPhase::Ready;
  active->ready_sequence=sequence_++;
  running_=0;
  dispatch();
  return true;
}

bool GuestCpuContextScheduler::saveCurrent(const MipsCpuContext& snapshot) {
  Thread* active=find(running_);
  if (!active || active->phase!=GuestCpuPhase::Running ||
      (snapshot.pc&3u)!=0 || (snapshot.next_pc&3u)!=0) return false;
  std::uint32_t word=0;
  if (!memory_.readU32(snapshot.pc,&word)) return false;
  active->context=snapshot;
  active->context.gpr[0]=0; // hardwired MIPS $zero cannot be modified
  return true;
}

bool GuestCpuContextScheduler::getContext(std::uint32_t id,MipsCpuContext* out) const {
  const Thread* t=find(id);
  if (!t || !out) return false;
  *out=t->context;
  return true;
}

bool GuestCpuContextScheduler::getPhase(std::uint32_t id,GuestCpuPhase* out) const {
  const Thread* t=find(id);
  if (!t || !out) return false;
  *out=t->phase;
  return true;
}

void GuestCpuContextScheduler::unblockCompletions() {
  for (auto& t : threads_) {
    if (t.phase!=GuestCpuPhase::Waiting) continue;
    GuestWaitCompletion c{};
    if (!waiter_.takeCompletion(t.id,&c)) continue;
    t.phase=GuestCpuPhase::Ready;
    t.ready_sequence=sequence_++;
    t.completion=c;
    t.has_completion=true;
  }
}

std::size_t GuestCpuContextScheduler::pump() {
  const std::size_t count=waiter_.pump();
  unblockCompletions();
  dispatch();
  return count;
}

ThreadOpStatus GuestCpuContextScheduler::recv(std::uint32_t queue,
                                               std::uint32_t* message,bool block) {
  Thread* active=find(running_);
  if (!active || active->phase!=GuestCpuPhase::Running || !message)
    return ThreadOpStatus::Invalid;
  const auto status=waiter_.receive(active->id,queue,message,block);
  if (status==ThreadOpStatus::Blocked) {
    active->phase=GuestCpuPhase::Waiting;
    running_=0;
  }
  unblockCompletions();
  dispatch();
  return status;
}

ThreadOpStatus GuestCpuContextScheduler::send(std::uint32_t queue,
                                               std::uint32_t message,bool block) {
  Thread* active=find(running_);
  if (!active || active->phase!=GuestCpuPhase::Running) return ThreadOpStatus::Invalid;
  const auto status=waiter_.send(active->id,queue,message,block);
  if (status==ThreadOpStatus::Blocked) {
    active->phase=GuestCpuPhase::Waiting;
    running_=0;
  }
  unblockCompletions();
  dispatch();
  return status;
}

OsMqStatus GuestCpuContextScheduler::postMessage(std::uint32_t queue,
                                                  std::uint32_t message) {
  GuestOsMesgQueue q(memory_,queue);
  const auto status=q.send(message);
  if (status==OsMqStatus::Ok) pump();
  return status;
}

bool GuestCpuContextScheduler::takeCompletion(std::uint32_t id,GuestWaitCompletion* out) {
  Thread* t=find(id);
  if (!t || !out || !t->has_completion) return false;
  *out=t->completion;
  t->has_completion=false;
  t->completion={};
  return true;
}
}  // namespace cw
