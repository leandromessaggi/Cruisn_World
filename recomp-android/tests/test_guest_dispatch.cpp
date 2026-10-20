#include "../android/app/src/main/cpp/recomp_dispatch.hpp"
#include "../android/app/src/main/cpp/recomp_memory.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>
using namespace cw;
static void be32(std::vector<std::uint8_t>& bytes,std::size_t pos,std::uint32_t word) {
  for(unsigned b=0;b<4;++b) bytes[pos+b]=static_cast<std::uint8_t>(word>>(24-8*b));
}
int main(int argc,char** argv) {
  const bool real_rom=argc==2;
  std::vector<std::uint8_t> rom(kRomSize,0x42);
  be32(rom,0,0x80371240u);be32(rom,0x10,0xDFE61153u);be32(rom,0x14,0xD76118E6u);
  for(const auto& f:compiledSignatures()) {
    std::size_t pos=kMainRomStart+(f.vram-kMainVram);
    for(const auto op:f.words) {be32(rom,pos,op);pos+=4;}
  }
  const char* path=real_rom?argv[1]:"synthetic_recomp_dispatch.z64";
  if (!real_rom) {std::ofstream f(path,std::ios::binary); f.write(reinterpret_cast<const char*>(rom.data()),rom.size());}
  NativeState mem;std::string error;assert(mem.load(path,&error));
  GuestCpuContextScheduler scheduler(mem);
  constexpr std::uint32_t a=0x803D4000u,b=0x803D5000u;
  assert(scheduler.createThread(a,10,0x802AF6ECu,0x803CF000u));
  assert(scheduler.createThread(b,30,0x802B251Cu,0x803CF100u));
  assert(scheduler.dispatch()==b);
  MipsCpuContext ctx{};
  assert(scheduler.getContext(b,&ctx));
  ctx.gpr[4]=0x803E0200u;ctx.gpr[5]=0xCAFE1234u;ctx.gpr[31]=0;
  ctx.hi=0xABCDEFu;ctx.fpr[5]=0x123456u;ctx.gpr[0]=UINT64_MAX;
  assert(scheduler.saveCurrent(ctx));
  GuestRecompDispatcher dispatcher(mem,scheduler);
  CallTrace trace{};
  assert(dispatcher.step(&trace)==RecompStepResult::Returned);
  assert(scheduler.current()==a);
  std::uint32_t val=0;
  assert(mem.readU32(0x803E025Cu,&val)&&val==0xCAFE1234u);
  assert(scheduler.getContext(b,&ctx) && ctx.hi==0xABCDEFu && ctx.fpr[5]==0x123456u);
  assert(ctx.gpr[0]==0 && ctx.pc==0 && ctx.next_pc==0);
  GuestCpuPhase phase{};assert(scheduler.getPhase(b,&phase)&&phase==GuestCpuPhase::Finished);
  assert(scheduler.getContext(a,&ctx));
  ctx.gpr[31]=0;assert(scheduler.saveCurrent(ctx));
  assert(mem.writeU32(kFlag2Address,7));
  assert(dispatcher.run(20,&trace)==1);
  assert(mem.readU32(kFlag2Address,&val)&&val==0);
  assert(mem.readU32(0x803334CCu,&val)&&val==1);
  assert(trace.entries.size()==3 && trace.entries[0]==0x802B251Cu &&
         trace.entries[1]==0x802AF6ECu && trace.entries[2]==0x802AF5F4u);
  assert(dispatcher.step()==RecompStepResult::NoReadyThread);
  GuestCpuContextScheduler unknown(mem);
  assert(unknown.createThread(0x803D6000u,1,kMainVram,0x803CF200u));
  GuestRecompDispatcher missing(mem,unknown);
  assert(missing.step()==RecompStepResult::NotRecompiled);
  assert(unknown.getContext(0x803D6000u,&ctx)&&ctx.pc==kMainVram);
  ctx.pc=0x802AF5E4u;ctx.next_pc=ctx.pc+4;ctx.gpr[31]=3;
  assert(unknown.saveCurrent(ctx));
  assert(mem.writeU32(kFlag2Address,51));
  assert(missing.step()==RecompStepResult::InvalidReturn);
  assert(mem.readU32(kFlag2Address,&val)&&val==51);
  ctx.gpr[31]=0x802AF5F4u;
  assert(unknown.saveCurrent(ctx));
  assert(missing.step()==RecompStepResult::Executed);
  assert(unknown.getContext(0x803D6000u,&ctx)&&ctx.pc==0x802AF5F4u);
  assert(mem.readU32(kFlag2Address,&val)&&val==1);
  ctx.gpr[31]=0; assert(unknown.saveCurrent(ctx));
  assert(missing.step()==RecompStepResult::Returned);
  assert(mem.readU32(kFlag2Address,&val)&&val==0);
  assert(unknown.current()==0);
  if (!real_rom) std::remove(path);
  std::cout<<"PASS: bounded verified C++ function dispatch, o32 args, priority, RA continuation and safe rejection (no dynamic MIPS execution)\n";
}
