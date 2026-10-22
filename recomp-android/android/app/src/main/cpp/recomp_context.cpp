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

bool GuestCpuContextScheduler::finishCurrent(const MipsCpuContext& final_context) {
  Thread* active=find(running_);
  if (!active || active->phase!=GuestCpuPhase::Running ||
      final_context.pc!=0 || final_context.next_pc!=0) return false;
  active->context=final_context;
  active->context.gpr[0]=0;
  active->phase=GuestCpuPhase::Finished;
  running_=0;
  dispatch();
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
    if (t.guest_receive_pending) {
      if (c.operation!=ThreadWaitKind::Receive ||
          (c.status==ThreadOpStatus::Complete && t.guest_receive_destination!=0 &&
           !memory_.writeU32(t.guest_receive_destination,c.message)))
        c.status=ThreadOpStatus::Invalid;
      t.context.gpr[2]=(c.status==ThreadOpStatus::Complete) ? 0u : UINT64_MAX;
      t.guest_receive_pending=false;
      t.guest_receive_destination=0;
    }
    if (t.guest_send_pending) {
      if (c.operation!=ThreadWaitKind::Send)
        c.status=ThreadOpStatus::Invalid;
      t.context.gpr[2]=(c.status==ThreadOpStatus::Complete) ? 0u : UINT64_MAX;
      t.guest_send_pending=false;
    }
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

ThreadOpStatus GuestCpuContextScheduler::recvGuest(std::uint32_t queue,
                                                       std::uint32_t destination,
                                                       bool block) {
  Thread* active=find(running_);
  if (!active || active->phase!=GuestCpuPhase::Running ||
      active->guest_receive_pending || active->guest_send_pending)
    return ThreadOpStatus::Invalid;
  // Validate the guest destination before consuming any queue message.
  // Null OSMesg* discards the received value, as permitted by libultra.
  std::uint32_t unused=0;
  if (destination!=0 && ((destination&3u)!=0 ||
      !memory_.readU32(destination,&unused))) {
    active->context.gpr[2]=UINT64_MAX;
    return ThreadOpStatus::Invalid;
  }
  active->guest_receive_pending=true;
  active->guest_receive_destination=destination;
  std::uint32_t value=0;
  const ThreadOpStatus status=recv(queue,&value,block);
  if (status==ThreadOpStatus::Blocked) return status;
  active->guest_receive_pending=false;
  active->guest_receive_destination=0;
  const bool written=status!=ThreadOpStatus::Complete || destination==0 ||
      memory_.writeU32(destination,value);
  const ThreadOpStatus result=written ? status : ThreadOpStatus::Invalid;
  active->context.gpr[2]=(result==ThreadOpStatus::Complete) ? 0u : UINT64_MAX;
  return result;
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

ThreadOpStatus GuestCpuContextScheduler::sendGuest(std::uint32_t queue,
                                                    std::uint32_t message,
                                                    bool block) {
  Thread* active=find(running_);
  if (!active || active->phase!=GuestCpuPhase::Running ||
      active->guest_receive_pending || active->guest_send_pending)
    return ThreadOpStatus::Invalid;
  active->guest_send_pending=true;
  const ThreadOpStatus status=send(queue,message,block);
  if (status==ThreadOpStatus::Blocked) return status;
  active->guest_send_pending=false;
  active->context.gpr[2]=(status==ThreadOpStatus::Complete) ? 0u : UINT64_MAX;
  return status;
}

ThreadOpStatus GuestCpuContextScheduler::invokeQueueCall(GuestQueueCall call) {
  Thread* active=find(running_);
  if (!active || active->phase!=GuestCpuPhase::Running)
    return ThreadOpStatus::Invalid;
  // N64 o32 argument words are the low 32 bits of MIPS III GPRs.
  const auto queue=static_cast<std::uint32_t>(active->context.gpr[4]);
  const auto value=static_cast<std::uint32_t>(active->context.gpr[5]);
  const auto flag=static_cast<std::uint32_t>(active->context.gpr[6]);
  if (flag>1u) {
    active->context.gpr[2]=UINT64_MAX;
    return ThreadOpStatus::Invalid;
  }
  switch (call) {
    case GuestQueueCall::Send: return sendGuest(queue,value,flag==1u);
    case GuestQueueCall::Receive: return recvGuest(queue,value,flag==1u);
  }
  active->context.gpr[2]=UINT64_MAX;
  return ThreadOpStatus::Invalid;
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
