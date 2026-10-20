#include "../android/app/src/main/cpp/recomp_thread_wait.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>
using namespace cw;
static void set32(std::vector<std::uint8_t>& bytes,std::size_t offset,std::uint32_t value) {
  for (int i=0;i<4;++i) bytes[offset+i]=static_cast<std::uint8_t>(value>>(24-8*i));
}
int main() {
  std::vector<std::uint8_t> rom(kRomSize,0x42);
  set32(rom,0,0x80371240);set32(rom,0x10,0xDFE61153);set32(rom,0x14,0xD76118E6);
  for (const auto& signature:compiledSignatures()) {
    auto pos=kMainRomStart+signature.vram-kMainVram;
    for (auto word:signature.words) {set32(rom,pos,word);pos+=4;}
  }
  const char* path="synthetic_wait_scheduler.z64";
  {std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(rom.data()),static_cast<std::streamsize>(rom.size()));}
  NativeState state;std::string err;assert(state.load(path,&err));
  constexpr std::uint32_t qaddr=0x803D0000,arr=0x803D0100;
  GuestOsMesgQueue queue(state,qaddr);assert(queue.create(arr,1));
  GuestQueueWaiter scheduler(state);
  constexpr std::uint32_t low=0x803D2000, high=0x803D3000, rx=0x803D4000;
  assert(scheduler.registerThread(low,10));assert(scheduler.registerThread(high,90));
  assert(scheduler.registerThread(rx,50));
  assert(!scheduler.registerThread(rx,5));assert(!scheduler.registerThread(0,5));
  std::uint32_t msg=0;
  assert(scheduler.receive(rx,qaddr,&msg,false)==ThreadOpStatus::WouldBlock);
  assert(scheduler.receive(rx,qaddr,&msg,true)==ThreadOpStatus::Blocked);
  assert(scheduler.waiting()==1);
  assert(scheduler.send(low,qaddr,0xAABBCCDD,false)==ThreadOpStatus::Complete);
  assert(scheduler.waiting()==0);
  GuestWaitCompletion complete;
  assert(scheduler.takeCompletion(rx,&complete));
  assert(complete.status==ThreadOpStatus::Complete);
  assert(complete.operation==ThreadWaitKind::Receive&&complete.message==0xAABBCCDD);
  assert(!scheduler.takeCompletion(rx,&complete));
  assert(queue.canReceive()==OsMqStatus::Empty);
  assert(scheduler.send(low,qaddr,0x12345678,false)==ThreadOpStatus::Complete);
  assert(scheduler.send(low,qaddr,0x00000001,true)==ThreadOpStatus::Blocked);
  assert(scheduler.send(high,qaddr,0x00000002,true)==ThreadOpStatus::Blocked);
  assert(scheduler.waiting()==2);
  assert(scheduler.send(low,qaddr,0x00000003,false)==ThreadOpStatus::Invalid);
  assert(scheduler.receive(rx,qaddr,&msg,false)==ThreadOpStatus::Complete&&msg==0x12345678);
  // High priority sender must obtain the newly opened queue slot first.
  assert(scheduler.takeCompletion(high,&complete));
  assert(complete.status==ThreadOpStatus::Complete&&complete.operation==ThreadWaitKind::Send);
  assert(scheduler.waiting()==1);
  assert(scheduler.receive(rx,qaddr,&msg,false)==ThreadOpStatus::Complete&&msg==2);
  assert(scheduler.takeCompletion(low,&complete));
  assert(scheduler.receive(rx,qaddr,&msg,false)==ThreadOpStatus::Complete&&msg==1);
  // Read via KSEG1 alias; queue retains N64 address layout.
  assert(scheduler.send(low,0xA03D0000,0x8BADF00D,false)==ThreadOpStatus::Complete);
  assert(scheduler.receive(rx,qaddr,&msg,false)==ThreadOpStatus::Complete&&msg==0x8BADF00D);
  // Corrupt message array pointer: queue errors do not masquerade as waits.
  assert(state.writeU32(qaddr+kOsMqMsg,0xA9000000));
  assert(scheduler.receive(rx,qaddr,&msg,true)==ThreadOpStatus::Invalid);
  assert(scheduler.send(low,qaddr,0x1,true)==ThreadOpStatus::Invalid);
  assert(state.writeU32(qaddr+kOsMqMsg,arr));
  // Force invalidation after a thread blocks: it receives a failure completion.
  assert(scheduler.receive(rx,qaddr,&msg,true)==ThreadOpStatus::Blocked);
  assert(state.writeU32(qaddr+kOsMqMsg,0xA9000000));
  assert(scheduler.pump()==1);
  assert(scheduler.takeCompletion(rx,&complete)&&complete.status==ThreadOpStatus::Invalid);
  std::remove(path);
  std::cout<<"PASS: priority-aware cooperative N64 queue waits, guest FIFO, wakeups, KSEG1, and invalid headers\n";
}
