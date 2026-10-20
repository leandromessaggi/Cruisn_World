#include "../android/app/src/main/cpp/recomp_loader.hpp"
#include <cstdint>
#include <iostream>
#include <string>
int main(int argc,char** argv){
 if(argc!=2)return 1;
 using namespace cw;
 NativeState state;std::string error;
 if(!state.load(argv[1],&error)){std::cerr<<error<<"\n";return 2;}
 if(state.overlayFNV64()!=UINT64_C(0x617C6FFD93FA6347))return 3;
 GuestClock clock;GuestMessageQueue queue(1);NativeOverlayLoad loader(state,clock,queue);
 if(!state.writeU32(kOverlayVram,0xDEADBEEF)||!loader.begin())return 4;
 for(std::size_t i=0;i<kLoaderChunkCount;++i){
   if(loader.service())return 5;
   clock.tickVi();
   if(!loader.service()||loader.failed())return 6;
 }
 std::uint32_t ptr=0;
 if(!loader.done()||loader.completedChunks()!=7||loader.committedBytes()!=kOverlayBytes||
    !state.readU32(kOverlayPointerGlobal,&ptr)||ptr!=kOverlayVram||
    state.overlayFNV64()!=UINT64_C(0x617C6FFD93FA6347))return 7;
 std::cout<<"PASS: Cruis'n World USA original loader contract, full overlay transferred natively\n";
}
