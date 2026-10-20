#include "recomp_dispatch.hpp"
#include "recomp_memory.hpp"

namespace cw {
namespace {
constexpr std::uint32_t kCaller = 0x802AF6ECu;
constexpr std::uint32_t kCallee = 0x802AF5F4u;
constexpr std::uint32_t kAfterJal = 0x802AF6FCu;
constexpr std::uint32_t kCallerFlag = 0x803334CCu;

bool returnAddressValid(NativeState& memory, std::uint32_t ra) {
  std::uint32_t ignored = 0;
  return ra == 0 || ((ra & 3u)==0 && memory.readU32(ra, &ignored));
}
} // namespace

RecompStepResult GuestRecompDispatcher::beginVerifiedWrapper(
    std::uint32_t id, MipsCpuContext& context, CallTrace* trace) {
  if (pending_frames_.count(id)!=0) return RecompStepResult::InvalidFrame;
  if (context.gpr[29]>UINT32_MAX || context.gpr[31]>UINT32_MAX)
    return RecompStepResult::InvalidFrame;
  const auto sp=static_cast<std::uint32_t>(context.gpr[29]);
  const auto saved_ra=static_cast<std::uint32_t>(context.gpr[31]);
  if (sp<24u || !returnAddressValid(memory_,saved_ra)) return RecompStepResult::InvalidReturn;
  const auto new_sp=sp-24u;
  std::uint32_t old_slot=0,callee_instr=0,continuation_instr=0;
  // Prove stack accessibility and both exact guest code targets BEFORE writes.
  if (!memory_.readU32(new_sp+20u,&old_slot) ||
      !memory_.readU32(kCallee,&callee_instr) ||
      !memory_.readU32(kAfterJal,&continuation_instr))
    return RecompStepResult::InvalidFrame;
  if (trace && !trace->record(kCaller)) return RecompStepResult::Failed;
  // Original MIPS instructions:
  // 802AF6EC addiu sp,sp,-24 ; 802AF6F0 sw ra,20(sp)
  // 802AF6F4 jal 802AF5F4  ; 802AF6F8 nop (delay slot)
  if (!memory_.writeU32(new_sp+20u,saved_ra)) return RecompStepResult::Failed;
  MipsCpuContext next=context;
  next.gpr[29]=new_sp;
  next.gpr[31]=kAfterJal;  // JAL produces PC+8 in RA
  next.pc=kCallee;
  next.next_pc=kCallee+4;
  next.in_delay_slot=false;
  if (!scheduler_.saveCurrent(next)) {
    memory_.writeU32(new_sp+20u,old_slot);
    return RecompStepResult::Failed;
  }
  pending_frames_.emplace(id,WrapperFrame{sp,saved_ra});
  return RecompStepResult::Called;
}

RecompStepResult GuestRecompDispatcher::resumeVerifiedWrapper(
    std::uint32_t id, MipsCpuContext& context, CallTrace* trace) {
  const auto it=pending_frames_.find(id);
  if (it==pending_frames_.end()) return RecompStepResult::InvalidFrame;
  const auto frame=it->second;
  if (context.gpr[29]!=frame.guest_sp-24u || context.gpr[31]!=kAfterJal)
    return RecompStepResult::InvalidFrame;
  std::uint32_t restored_ra=0;
  if (!memory_.readU32(frame.guest_sp-4u,&restored_ra) ||
      restored_ra!=frame.caller_ra ||
      !returnAddressValid(memory_,restored_ra)) return RecompStepResult::InvalidReturn;
  std::uint32_t flag_old=0;
  if (!memory_.readU32(kCallerFlag,&flag_old)) return RecompStepResult::Failed;
  if (trace && !trace->record(kAfterJal)) return RecompStepResult::Failed;
  // Original verified continuation at 802AF6FC:
  // lw ra,20(sp); li t6,1; lui at,8033; sw t6,34CC(at);
  // jr ra; addiu sp,sp,24 (JR delay slot)
  if (!memory_.writeU32(kCallerFlag,1u)) return RecompStepResult::Failed;
  MipsCpuContext next=context;
  next.gpr[31]=restored_ra;
  next.gpr[14]=1u; // t6, changed by the real compiled continuation
  next.gpr[1]=0x80330000u; // $at after LUI
  next.gpr[29]=frame.guest_sp;
  next.in_delay_slot=false;
  if (restored_ra==0) {
    next.pc=0;
    next.next_pc=0;
    if (!scheduler_.finishCurrent(next)) {
      memory_.writeU32(kCallerFlag,flag_old);
      return RecompStepResult::Failed;
    }
    pending_frames_.erase(it);
    return RecompStepResult::Returned;
  }
  next.pc=restored_ra;
  next.next_pc=restored_ra+4u;
  if (!scheduler_.saveCurrent(next)) {
    memory_.writeU32(kCallerFlag,flag_old);
    return RecompStepResult::Failed;
  }
  pending_frames_.erase(it);
  return RecompStepResult::Resumed;
}

RecompStepResult GuestRecompDispatcher::step(CallTrace* trace) {
  const auto thread=scheduler_.dispatch();
  if (!thread) return RecompStepResult::NoReadyThread;
  MipsCpuContext snapshot{};
  if (!scheduler_.getContext(thread,&snapshot)) return RecompStepResult::Failed;
  if (snapshot.pc==kCaller) return beginVerifiedWrapper(thread,snapshot,trace);
  if (snapshot.pc==kAfterJal) return resumeVerifiedWrapper(thread,snapshot,trace);
  // Only known, ROM-verified entrypoints are available; never interpret MIPS.
  bool found=false;
  for (const auto& sig:compiledSignatures()) {
    if (sig.vram==snapshot.pc) { found=true; break; }
  }
  if (!found) return RecompStepResult::NotRecompiled;
  if (snapshot.gpr[31]>UINT32_MAX) return RecompStepResult::InvalidReturn;
  const auto return_pc=static_cast<std::uint32_t>(snapshot.gpr[31]);
  if (!returnAddressValid(memory_,return_pc)) return RecompStepResult::InvalidReturn;
  CallArgs args{};
  args.a0=static_cast<std::uint32_t>(snapshot.gpr[4]);
  args.a1=static_cast<std::uint32_t>(snapshot.gpr[5]);
  args.a2=static_cast<std::uint32_t>(snapshot.gpr[6]);
  args.a3=static_cast<std::uint32_t>(snapshot.gpr[7]);
  args.v0=static_cast<std::uint32_t>(snapshot.gpr[2]);
  args.f12=static_cast<std::uint32_t>(snapshot.fpr[12]);
  args.f14=static_cast<std::uint32_t>(snapshot.fpr[14]);
  if (!memory_.callRecompiled(snapshot.pc,&args,trace)) return RecompStepResult::Failed;
  snapshot.gpr[0]=0;
  snapshot.gpr[2]=static_cast<std::uint64_t>(static_cast<std::int64_t>(
      static_cast<std::int32_t>(args.v0)));
  snapshot.in_delay_slot=false;
  if (return_pc==0) {
    snapshot.pc=0;
    snapshot.next_pc=0;
    return scheduler_.finishCurrent(snapshot) ? RecompStepResult::Returned
                                               : RecompStepResult::Failed;
  }
  snapshot.pc=return_pc;
  snapshot.next_pc=return_pc+4u;
  return scheduler_.saveCurrent(snapshot) ? RecompStepResult::Executed
                                          : RecompStepResult::Failed;
}

std::size_t GuestRecompDispatcher::run(std::size_t budget,CallTrace* trace) {
  const auto maximum=budget<256u?budget:256u;
  std::size_t completed=0;
  while (completed<maximum) {
    const auto result=step(trace);
    if (result!=RecompStepResult::Executed && result!=RecompStepResult::Called &&
        result!=RecompStepResult::Resumed && result!=RecompStepResult::Returned) break;
    ++completed;
  }
  return completed;
}
} // namespace cw
