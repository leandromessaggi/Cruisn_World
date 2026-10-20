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
  std::remove(path);
  std::cout<<"PASS: MIPS thread snapshot GPR/FPR/COP0/HI/LO/PC, blocking wakeup & priority preemption (no guest execution)\n";
}
