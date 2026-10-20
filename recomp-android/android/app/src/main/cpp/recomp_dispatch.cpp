#include "recomp_dispatch.hpp"
#include "recomp_memory.hpp"

namespace cw {
RecompStepResult GuestRecompDispatcher::step(CallTrace* trace) {
  const auto thread=scheduler_.dispatch();
  if (!thread) return RecompStepResult::NoReadyThread;
  MipsCpuContext snapshot{};
  if (!scheduler_.getContext(thread,&snapshot)) return RecompStepResult::Failed;
  // Explicitly refuse unknown MIPS routines. There is no instruction interpreter.
  bool found=false;
  for (const auto& sig:compiledSignatures()) {
    if (sig.vram==snapshot.pc) { found=true; break; }
  }
  if (!found) return RecompStepResult::NotRecompiled;
  const std::uint32_t return_pc=static_cast<std::uint32_t>(snapshot.gpr[31]);
  std::uint32_t unused=0;
  if (return_pc!=0 && ((return_pc&3u)!=0 ||
      !memory_.readU32(return_pc,&unused))) return RecompStepResult::InvalidReturn;
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
  // The statically compiled body includes its own MIPS JR RA delay slot.
  // The only supported continuation is its return address.
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
  // Bound this call to avoid accidental unbounded re-entry on guest addresses.
  const std::size_t maximum=budget<256u?budget:256u;
  std::size_t completed=0;
  while (completed<maximum) {
    const auto result=step(trace);
    if (result!=RecompStepResult::Executed && result!=RecompStepResult::Returned) break;
    ++completed;
  }
  return completed;
}
} // namespace cw
