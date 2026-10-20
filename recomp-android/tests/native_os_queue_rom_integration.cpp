// Integration from user-provided ROM. No copyrighted bytes in the repository.
#include "../android/app/src/main/cpp/recomp_os_queue.hpp"
#include <iostream>
#include <string>
int main(int argc,char** argv) {
 if(argc!=2) return 1;
 using namespace cw;
 NativeState state;std::string error;
 if(!state.load(argv[1],&error)) {std::cerr<<error<<'\n';return 2;}
 const std::uint64_t expected=UINT64_C(0x617C6FFD93FA6347);
 if(state.overlayFNV64()!=expected) return 3;
 GuestOsMesgQueue queue(state,0x803E0000);
 if(!queue.create(0x803E0040,1))return 4;
 GuestClock clock;
 GuestPiOsBridge bridge(state,clock,queue);
 if(!state.writeU32(kOverlayVram,0xDEADBEEF) ||
    !bridge.startRead(kCartBase+kOverlayRomStart,kOverlayVram,kOverlayRomEnd-kOverlayRomStart,0x50514952))return 5;
 clock.tickVi();bridge.service();
 std::uint32_t message=0;
 if(bridge.busy() || bridge.completed()!=1 || bridge.notifications()!=1 ||
    queue.receive(&message)!=OsMqStatus::Ok || message!=0x50514952 ||
    state.overlayFNV64()!=expected) return 6;
 std::cout<<"PASS: original World US overlay restored by PI DMA, guest libultra OSMesgQueue completion visible to recompiled code\n";
}
