#include "../android/app/src/main/cpp/recomp_context.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>
using namespace cw;
static void be32(std::vector<std::uint8_t>& data,std::size_t off,std::uint32_t v) {
  for (unsigned j=0;j<4;++j) data[off+j]=static_cast<std::uint8_t>(v>>(24-8*j));
}
int main() {
  std::vector<std::uint8_t> rom(kRomSize,0x42);
  be32(rom,0,0x80371240);be32(rom,0x10,0xDFE61153);be32(rom,0x14,0xD76118E6);
  for (const auto& sign:compiledSignatures()) {
    std::size_t off=kMainRomStart+sign.vram-kMainVram;
    for (auto word:sign.words) {be32(rom,off,word);off+=4;}
  }
  const char* path="synthetic_cpu_context.z64";
  {std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(rom.data()),static_cast<std::streamsize>(rom.size()));}
  NativeState mem;std::string err;assert(mem.load(path,&err));
  GuestOsMesgQueue queue(mem,0x803D0000);assert(queue.create(0x803D0100,1));
  GuestCpuContextScheduler s(mem);
  constexpr std::uint32_t low=0x803D2000,high=0x803D3000;
  assert(!s.createThread(0,5,kMainVram,0x803F0000));
  assert(!s.createThread(low,5,0xFFFFFFFF,0x803F0000));
  assert(!s.createThread(low,5,kMainVram,0x803F0003));
  assert(s.createThread(low,30,kMainVram,0x803F0000,0xAABBCCDD));
  assert(!s.createThread(low,30,kMainVram,0x803F0000));
  assert(s.createThread(high,90,kMainVram+0x50,0x803F1000,0x11223344));
  assert(s.dispatch()==high);
  MipsCpuContext saved{};assert(s.getContext(high,&saved));
  assert(saved.pc==kMainVram+0x50 && saved.next_pc==saved.pc+4);
  assert(saved.gpr[4]==0x11223344 && saved.gpr[29]==0x803F1000);
  saved.gpr[0]=UINT64_MAX;
  saved.gpr[2]=UINT64_C(0xABCD123498765432);
  saved.gpr[31]=0x802AD490;
  saved.fpr[12]=UINT64_C(0x7FF0000000000000);
  saved.hi=UINT64_C(0x1122334455667788);
  saved.lo=UINT64_C(0x8877665544332211);
  saved.cop0_epc=0x802AD450;
  saved.cop0_count=0xDEADBEEF;
  saved.cop0_status=0x34000000;
  saved.pc=kMainVram+0x54;
  saved.next_pc=saved.pc+4;
  saved.in_delay_slot=true;
  assert(s.saveCurrent(saved));
  saved.pc=3;assert(!s.saveCurrent(saved));
  saved.pc=kMainVram+0x54;
  std::uint32_t message=0;
  assert(s.recv(0x803D0000,&message,true)==ThreadOpStatus::Blocked);
  assert(s.current()==low);
  GuestCpuPhase phase{};
  assert(s.getPhase(high,&phase)&&phase==GuestCpuPhase::Waiting);
  MipsCpuContext persisted{};assert(s.getContext(high,&persisted));
  assert(persisted.gpr[0]==0 && persisted.gpr[2]==UINT64_C(0xABCD123498765432));
  assert(persisted.fpr[12]==UINT64_C(0x7FF0000000000000));
  assert(persisted.hi==UINT64_C(0x1122334455667788));
  assert(persisted.lo==UINT64_C(0x8877665544332211));
  assert(persisted.pc==kMainVram+0x54 && persisted.in_delay_slot);
  assert(persisted.cop0_count==0xDEADBEEF);
  assert(s.postMessage(0x803D0000,0xDEADC0DE)==OsMqStatus::Ok);
  assert(s.current()==high); // higher priority awakened & preempts low
  GuestWaitCompletion completion{};
  assert(s.takeCompletion(high,&completion));
  assert(completion.operation==ThreadWaitKind::Receive && completion.message==0xDEADC0DE);
  assert(!s.takeCompletion(high,&completion));
  assert(s.getContext(high,&persisted) && persisted.pc==kMainVram+0x54);
  assert(s.getPhase(low,&phase)&&phase==GuestCpuPhase::Ready);
  assert(s.yield() && s.current()==high); // high remains highest priority
  assert(s.recv(0x803D0000,&message,false)==ThreadOpStatus::WouldBlock);
  assert(s.current()==high);
  assert(s.postMessage(0x803D0000,0xBADCAFE)==OsMqStatus::Ok);
  assert(s.postMessage(0x803D0000,0x1)==OsMqStatus::Full);
  assert(s.recv(0x803D0000,&message,false)==ThreadOpStatus::Complete);
  assert(message==0xBADCAFE);
  assert(!s.getContext(0x803D8000,&persisted));
  // v0.30: o32 a0/a1/a2 -> libultra send/receive, including a blocked
  // sender's v0 update after the other guest thread opens a queue slot.
  constexpr std::uint32_t qaddr=0x803D0000u, guest_dst=0x803D0400u;
  MipsCpuContext abi{};
  assert(s.current()==high && s.getContext(high,&abi));
  abi.pc=kMainVram+0x58u;
  abi.next_pc=abi.pc+4u;
  abi.gpr[4]=qaddr;
  abi.gpr[5]=0xFACEFEEDu;
  abi.gpr[6]=1u; // OS_MESG_BLOCK
  abi.gpr[2]=0xDEADBEEFu;
  assert(s.saveCurrent(abi));
  assert(s.postMessage(qaddr,0x11112222u)==OsMqStatus::Ok);
  assert(s.invokeQueueCall(GuestQueueCall::Send)==ThreadOpStatus::Blocked);
  assert(s.current()==low);
  assert(s.recv(qaddr,&message,false)==ThreadOpStatus::Complete &&
         message==0x11112222u);
  assert(s.current()==high);
  assert(s.takeCompletion(high,&completion) &&
         completion.operation==ThreadWaitKind::Send &&
         completion.status==ThreadOpStatus::Complete);
  assert(s.getContext(high,&persisted) && persisted.pc==abi.pc &&
         persisted.gpr[2]==0u && persisted.gpr[5]==0xFACEFEEDu);
  assert(queue.receive(&message)==OsMqStatus::Ok && message==0xFACEFEEDu);
  abi=persisted;
  abi.gpr[5]=0xCAFE1234u;
  abi.gpr[6]=0u; // OS_MESG_NOBLOCK
  assert(s.saveCurrent(abi));
  assert(s.invokeQueueCall(GuestQueueCall::Send)==ThreadOpStatus::Complete);
  assert(s.getContext(high,&persisted) && persisted.gpr[2]==0u);
  assert(s.invokeQueueCall(GuestQueueCall::Send)==ThreadOpStatus::WouldBlock);
  assert(s.getContext(high,&persisted) && persisted.gpr[2]==UINT64_MAX);
  abi=persisted;
  abi.gpr[6]=2u; // invalid flag: do not modify the queued message
  assert(s.saveCurrent(abi));
  assert(s.invokeQueueCall(GuestQueueCall::Send)==ThreadOpStatus::Invalid);
  assert(queue.receive(&message)==OsMqStatus::Ok && message==0xCAFE1234u);
  abi.gpr[5]=guest_dst;
  abi.gpr[6]=0u;
  assert(s.saveCurrent(abi));
  assert(s.invokeQueueCall(GuestQueueCall::Receive)==ThreadOpStatus::WouldBlock);
  assert(s.getContext(high,&persisted) && persisted.gpr[2]==UINT64_MAX);
  assert(s.postMessage(qaddr,0xDEADBEEFu)==OsMqStatus::Ok);
  assert(s.invokeQueueCall(GuestQueueCall::Receive)==ThreadOpStatus::Complete);
  assert(mem.readU32(guest_dst,&message) && message==0xDEADBEEFu);
  assert(s.getContext(high,&persisted) && persisted.gpr[2]==0u);
  abi=persisted;
  abi.gpr[6]=1u;
  assert(s.saveCurrent(abi));
  assert(s.invokeQueueCall(GuestQueueCall::Receive)==ThreadOpStatus::Blocked);
  assert(s.current()==low);
  assert(s.send(qaddr,0xABCDEF12u,false)==ThreadOpStatus::Complete);
  assert(s.current()==high);
  assert(s.takeCompletion(high,&completion) &&
         completion.operation==ThreadWaitKind::Receive &&
         completion.status==ThreadOpStatus::Complete);
  assert(mem.readU32(guest_dst,&message) && message==0xABCDEF12u);
  assert(s.getContext(high,&persisted) && persisted.gpr[2]==0u);
  assert(s.postMessage(qaddr,0x9999u)==OsMqStatus::Ok);
  abi=persisted;
  abi.gpr[5]=0xA9000000u; // invalid guest destination
  abi.gpr[6]=0u;
  assert(s.saveCurrent(abi));
  assert(s.invokeQueueCall(GuestQueueCall::Receive)==ThreadOpStatus::Invalid);
  assert(queue.receive(&message)==OsMqStatus::Ok && message==0x9999u);
  std::remove(path);
  std::cout<<"PASS: MIPS snapshot, libultra o32 send/receive ABI, blocking wakeup, v0, priority and queue integrity (no guest execution)\n";
}
