#include "../android/app/src/main/cpp/recomp_dispatch.hpp"
#include "../android/app/src/main/cpp/recomp_memory.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>
using namespace cw;

static void put32(std::vector<std::uint8_t>& b,std::size_t pos,std::uint32_t x) {
  for (unsigned i=0;i<4;++i) b[pos+i]=std::uint8_t(x>>(24u-8u*i));
}
int main(int argc,char** argv) {
  const bool original=argc==2;
  const char* filename=original?argv[1]:"synthetic_call_frames.z64";
  if (!original) {
    std::vector<std::uint8_t> rom(kRomSize,0x42u);
    put32(rom,0,0x80371240u);
    put32(rom,0x10,0xDFE61153u);
    put32(rom,0x14,0xD76118E6u);
    for (const auto& sig:compiledSignatures()) {
      std::size_t rom_offset=kMainRomStart+sig.vram-kMainVram;
      for(auto instruction:sig.words) { put32(rom,rom_offset,instruction);rom_offset+=4; }
    }
    std::ofstream f(filename,std::ios::binary);
    f.write(reinterpret_cast<const char*>(rom.data()),rom.size());
  }
  NativeState ram;
  std::string error;
  assert(ram.load(filename,&error));
  constexpr std::uint32_t thread=0x803D3000u;
  constexpr std::uint32_t sp=0x803DF000u;
  GuestCpuContextScheduler sched(ram);
  assert(sched.createThread(thread,20,0x802AF6ECu,sp));
  assert(sched.dispatch()==thread);
  MipsCpuContext c{};
  assert(sched.getContext(thread,&c));
  c.gpr[31]=0;
  c.hi=0x1234;
  c.fpr[5]=0x012345;
  assert(sched.saveCurrent(c));
  GuestRecompDispatcher d(ram,sched);
  CallTrace trace{};
  assert(ram.writeU32(kFlag2Address,42));
  assert(ram.writeU32(0x803334CCu,99));
  std::uint32_t word=0;
  // A real JAL edge: original prologue stores old RA and adjusts SP.
  assert(d.step(&trace)==RecompStepResult::Called);
  assert(d.pendingCallFrames()==1);
  assert(sched.getContext(thread,&c));
  assert(c.pc==0x802AF5F4u && c.gpr[29]==sp-24u && c.gpr[31]==0x802AF6FCu);
  assert(ram.readU32(sp-4u,&word)&&word==0);
  assert(ram.readU32(kFlag2Address,&word)&&word==42);
  assert(ram.readU32(0x803334CCu,&word)&&word==99);

  // Caller continuation cannot be executed until callee returns.
  assert(d.step(&trace)==RecompStepResult::Executed);
  assert(sched.getContext(thread,&c));
  assert(c.pc==0x802AF6FCu && c.gpr[29]==sp-24u);
  assert(ram.readU32(kFlag2Address,&word)&&word==0);
  assert(ram.readU32(0x803334CCu,&word)&&word==99);

  // Detect tampering with the saved guest RA without modifying the flag.
  assert(ram.writeU32(sp-4u,0x802AF5F4u));
  assert(d.step(&trace)==RecompStepResult::InvalidReturn);
  assert(d.pendingCallFrames()==1);
  assert(ram.readU32(0x803334CCu,&word)&&word==99);
  assert(ram.writeU32(sp-4u,0));
  assert(d.step(&trace)==RecompStepResult::Returned);
  assert(d.pendingCallFrames()==0);
  assert(ram.readU32(0x803334CCu,&word)&&word==1);
  assert(sched.getContext(thread,&c));
  assert(c.pc==0 && c.next_pc==0 && c.gpr[29]==sp && c.gpr[31]==0);
  assert(c.hi==0x1234 && c.fpr[5]==0x012345);
  assert(c.gpr[14]==1 && c.gpr[1]==0x80330000);
  assert(trace.entries.size()==3 && trace.entries[0]==0x802AF6ECu &&
         trace.entries[1]==0x802AF5F4u && trace.entries[2]==0x802AF6FCu);
  assert(d.step()==RecompStepResult::NoReadyThread);

  // No arbitrary PC injection into the internal continuation address.
  GuestCpuContextScheduler illegal(ram);
  assert(illegal.createThread(0x803D4000u,60,0x802AF6FCu,sp));
  GuestRecompDispatcher forged(ram,illegal);
  assert(forged.step()==RecompStepResult::InvalidFrame);
  assert(forged.pendingCallFrames()==0);

  // Check invalid saved return pointer before touching guest stack or data.
  GuestCpuContextScheduler malformed(ram);
  assert(malformed.createThread(0x803D5000u,50,0x802AF6ECu,sp));
  malformed.dispatch();
  assert(malformed.getContext(0x803D5000u,&c));
  c.gpr[31]=3;
  assert(malformed.saveCurrent(c));
  GuestRecompDispatcher invalid(ram,malformed);
  assert(ram.writeU32(sp-4u,0x12345678u));
  assert(invalid.step()==RecompStepResult::InvalidReturn);
  assert(ram.readU32(sp-4u,&word)&&word==0x12345678u);
  assert(invalid.pendingCallFrames()==0);
  if (!original) std::remove(filename);
  std::cout<<"PASS: ROM-verified three-step JAL / guest stack frame / RA restoration / corruption safeguards\n";
}
