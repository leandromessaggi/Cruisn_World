// ROM-free regression for the Android-native VI -> guest queue -> priority
// wakeup path. No game ROM, graphics, or host interrupt is involved.
#include "../android/app/src/main/cpp/recomp_vi_events.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>
using namespace cw;
static void be32(std::vector<std::uint8_t>& d,std::size_t i,std::uint32_t v) {
  for(unsigned b=0;b<4;++b)d[i+b]=static_cast<std::uint8_t>(v>>(24-8*b));
}
int main() {
  std::vector<std::uint8_t> rom(kRomSize,0x42);
  be32(rom,0,0x80371240);be32(rom,0x10,0xDFE61153);be32(rom,0x14,0xD76118E6);
  for(const auto& sign:compiledSignatures()) {
    auto pos=kMainRomStart+sign.vram-kMainVram;
    for(auto word:sign.words) {be32(rom,pos,word);pos+=4;}
  }
  const char* path="synthetic_vi_events.z64";
  {std::ofstream f(path,std::ios::binary);
   f.write(reinterpret_cast<const char*>(rom.data()),static_cast<std::streamsize>(rom.size()));}
  NativeState state;std::string error;assert(state.load(path,&error));
  constexpr std::uint32_t q=0x803D0000,arr=0x803D0100;
  GuestOsMesgQueue queue(state,q);assert(queue.create(arr,1));
  GuestClock clock;
  GuestCpuContextScheduler scheduler(state);
  GuestViEventBridge vi(state,clock,scheduler);
  assert(vi.tick()==ViTickResult::Unregistered && clock.viCount()==1);
  assert(!vi.setRetrace(0x12345678,0x29A,1));
  assert(!vi.setRetrace(q,0x29A,0));
  assert(!vi.setRetrace(q,0x29A,1025));
  assert(!vi.registered());
  constexpr std::uint32_t low=0x803D2000,high=0x803D3000;
  assert(scheduler.createThread(low,20,kMainVram,0x803CF000));
  assert(scheduler.createThread(high,80,kMainVram+0x50,0x803CF100));
  assert(scheduler.dispatch()==high);
  MipsCpuContext snapshot{};
  assert(scheduler.getContext(high,&snapshot));
  snapshot.gpr[2]=0x12345678;
  snapshot.pc=kMainVram+0x54;
  snapshot.next_pc=snapshot.pc+4;
  assert(scheduler.saveCurrent(snapshot));
  std::uint32_t value=0;
  assert(scheduler.recv(q,&value,true)==ThreadOpStatus::Blocked);
  assert(scheduler.current()==low);
  assert(vi.setRetrace(q,0x29A,2) && vi.interval()==2);
  assert(vi.tick()==ViTickResult::NotDue && clock.viCount()==2);
  assert(scheduler.current()==low);
  assert(vi.tick()==ViTickResult::Delivered && clock.viCount()==3);
  assert(scheduler.current()==high); // VI wakes the higher-priority guest context
  GuestWaitCompletion completion{};
  assert(scheduler.takeCompletion(high,&completion));
  assert(completion.status==ThreadOpStatus::Complete &&
         completion.operation==ThreadWaitKind::Receive && completion.message==0x29A);
  MipsCpuContext restored{};
  assert(scheduler.getContext(high,&restored) &&
         restored.pc==snapshot.pc && restored.gpr[2]==snapshot.gpr[2]);
  assert(queue.canReceive()==OsMqStatus::Empty);
  assert(vi.tick()==ViTickResult::NotDue);
  assert(vi.tick()==ViTickResult::Delivered); // nobody waiting: stays in guest queue
  assert(vi.tick()==ViTickResult::NotDue);
  assert(vi.tick()==ViTickResult::DroppedFull); // nonblocking interrupt, no overwrite
  assert(vi.delivered()==2 && vi.dropped()==1);
  assert(queue.receive(&value)==OsMqStatus::Ok && value==0x29A);
  assert(queue.canReceive()==OsMqStatus::Empty);
  // Failed re-registration must not replace a valid interval or message.
  assert(!vi.setRetrace(0x12345678,0x29B,1) && vi.interval()==2);
  assert(vi.tick()==ViTickResult::NotDue);
  assert(vi.tick()==ViTickResult::Delivered);
  assert(queue.receive(&value)==OsMqStatus::Ok && value==0x29A);
  // Invalidated guest queue is an error, not an event dropped due to capacity.
  assert(state.writeU32(q+kOsMqMsg,0xA9000000));
  assert(vi.tick()==ViTickResult::NotDue);
  assert(vi.tick()==ViTickResult::InvalidQueue);
  assert(vi.invalid()==1 && vi.dropped()==1);
  assert(state.writeU32(q+kOsMqMsg,arr));
  // Re-register from a KSEG1 alias: interval restarts on current VI count.
  assert(vi.setRetrace(0xA03D0000,0x29B,1));
  assert(vi.tick()==ViTickResult::Delivered);
  assert(queue.receive(&value)==OsMqStatus::Ok && value==0x29B);
  assert(vi.delivered()==4 && vi.dropped()==1 && vi.invalid()==1);
  assert(clock.viCount()==12);
  assert(GuestViHostPacer::runTicks(vi,0)==0);
  assert(GuestViHostPacer::runTicks(vi,601)==0);
  const auto started=std::chrono::steady_clock::now();
  assert(GuestViHostPacer::runTicks(vi,3)==3);
  assert(std::chrono::steady_clock::now()-started>=std::chrono::milliseconds(35));
  assert(clock.viCount()==15 && vi.delivered()==5 && vi.dropped()==3);
  assert(queue.receive(&value)==OsMqStatus::Ok && value==0x29B);
  std::remove(path);
  std::puts("PASS: deterministic VI interval, guest OSMesgQueue, priority wakeup, full drop, invalid queue, KSEG1 alias; no graphics/scheduler execution");
}
