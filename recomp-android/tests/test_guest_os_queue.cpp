#include "../android/app/src/main/cpp/recomp_os_queue.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

static void set32(std::vector<std::uint8_t>& a,std::size_t i,std::uint32_t v) {
 for (int b=0;b<4;++b) a[i+b]=static_cast<std::uint8_t>(v>>(24-8*b));
}
int main(){
 using namespace cw;
 std::vector<std::uint8_t> rom(kRomSize,0x42);
 set32(rom,0,0x80371240);set32(rom,0x10,0xDFE61153);
 set32(rom,0x14,0xD76118E6);
 for(const auto& f:compiledSignatures()) {
  auto at=kMainRomStart+f.vram-kMainVram;
  for(const auto v:f.words) {set32(rom,at,v);at+=4;}
 }
 set32(rom,kOverlayRomStart,0x12345678u);
 const char* path="synthetic_osmq_n64.z64";
 {std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(rom.data()),static_cast<std::streamsize>(rom.size()));}
 NativeState memory;std::string error;
 assert(memory.load(path,&error));
 // Two OSMessageQueues, one at KSEG0, one addressed from KSEG1 alias.
 constexpr std::uint32_t qaddr=0x803E0000, arr=0x803E0080;
 GuestOsMesgQueue queue(memory,qaddr);
 assert(!queue.create(0x90000000,2));
 assert(!queue.create(arr,0));
 assert(!queue.create(arr,100000));
 assert(queue.create(arr,2));
 std::uint32_t result=0;assert(memory.readU32(qaddr+0x14,&result)&&result==arr);
 assert(memory.readU32(qaddr+0x10,&result)&&result==2);
 assert(queue.canReceive()==OsMqStatus::Empty);
 assert(queue.receive(&result)==OsMqStatus::Empty);
 assert(queue.send(0x11223344u)==OsMqStatus::Ok);
 assert(queue.send(0xAABBCCDDu)==OsMqStatus::Ok);
 assert(queue.canSend()==OsMqStatus::Full);
 assert(queue.send(0)==OsMqStatus::Full);
 assert(queue.size(&result)&&result==2);
 assert(memory.readU32(arr,&result)&&result==0x11223344u);
 assert(memory.readU32(arr+4,&result)&&result==0xAABBCCDDu);
 assert(queue.receive(&result)==OsMqStatus::Ok && result==0x11223344u);
 assert(queue.send(0xDEADBEEFu)==OsMqStatus::Ok); // circular wrap
 assert(queue.receive(&result)==OsMqStatus::Ok&&result==0xAABBCCDDu);
 assert(queue.receive(&result)==OsMqStatus::Ok&&result==0xDEADBEEFu);
 assert(queue.receive(&result)==OsMqStatus::Empty);
 assert(queue.size(&result)&&result==0);
 assert(queue.send(0xCAFEBABEu)==OsMqStatus::Ok);
 assert(memory.writeU32(qaddr+0x14,0xFFFF0000u));
 assert(queue.canSend()==OsMqStatus::Invalid);
 assert(queue.receive(&result)==OsMqStatus::Invalid);
 assert(queue.send(0)==OsMqStatus::Invalid);
 assert(memory.writeU32(qaddr+0x14,arr));
 assert(queue.receive(&result)==OsMqStatus::Ok&&result==0xCAFEBABEu);
 assert(queue.create(arr,1));
 // Guest-ABI layout and KSEG0/KSEG1 alias correctness.
 GuestOsMesgQueue alias(memory,0xA03E0000u);
 assert(alias.send(0x50100AAAu)==OsMqStatus::Ok);
 assert(queue.receive(&result)==OsMqStatus::Ok&&result==0x50100AAAu);
 // DMA -> guest osCreateMesgQueue ring: no lost completion on full queue.
 constexpr std::uint32_t dma_queue=0x803E0200, dma_msgs=0x803E0220;
 GuestOsMesgQueue os_queue(memory,dma_queue);
 assert(os_queue.create(dma_msgs,1));
 assert(os_queue.send(0xAAAABBBBu)==OsMqStatus::Ok); // backpressure
 GuestClock clock;
 GuestPiOsBridge bridge(memory,clock,os_queue);
 assert(memory.writeU32(kOverlayVram,0xFFFFFFFFu));
 assert(bridge.startRead(kCartBase + kOverlayRomStart,kOverlayVram,4,0xABCDEF12u));
 bridge.service();
 assert(bridge.busy() && !bridge.pendingMessage());
 clock.tickVi();
 bridge.service();
 assert(bridge.pendingMessage() && bridge.completed()==1 && bridge.notifications()==0);
 assert(memory.readU32(kOverlayVram,&result)&&result==0x12345678u);
 assert(!bridge.startRead(kCartBase+kOverlayRomStart,kOverlayVram,4,1));
 bridge.service(); // full ring, must retain event
 assert(bridge.pendingMessage() && bridge.notifications()==0);
 assert(os_queue.receive(&result)==OsMqStatus::Ok && result==0xAAAABBBBu);
 bridge.service();
 assert(!bridge.busy() && bridge.notifications()==1);
 assert(os_queue.receive(&result)==OsMqStatus::Ok&&result==0xABCDEF12u);
 assert(os_queue.receive(&result)==OsMqStatus::Empty);
 // Second transfer back-to-back must be queued only after first completion.
 assert(bridge.startRead(kCartBase+kOverlayRomStart,kOverlayVram,4,0x12340000));
 bridge.service();
 assert(bridge.busy());
 clock.tickVi();bridge.service();
 assert(!bridge.busy()&&bridge.completed()==2&&bridge.notifications()==2);
 assert(os_queue.receive(&result)==OsMqStatus::Ok&&result==0x12340000u);
 std::remove(path);
 std::cout<<"PASS: libultra OSMesgQueue o32 guest ABI, FIFO wrap, KSEG1 aliases, corrupt pointers rejected, PI DMA/VI notification backpressure\n";
}
