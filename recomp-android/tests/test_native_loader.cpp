#include "../android/app/src/main/cpp/recomp_loader.hpp"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
static void be32(std::vector<std::uint8_t>& r,std::size_t off,std::uint32_t x){
 for(int i=0;i<4;++i)r[off+i]=static_cast<std::uint8_t>(x>>(24-i*8));
}
int main(){
 using namespace cw;
 static_assert(kOverlayBytes==0x1A260u,"ROM overlay contract changed");
 static_assert(kLoaderChunkCount==7,"expected 7 PI blocks");
 std::vector<std::uint8_t> rom(kRomSize,0x42);
 be32(rom,0,0x80371240);be32(rom,0x10,0xDFE61153);be32(rom,0x14,0xD76118E6);
 for(const auto& fn:compiledSignatures()){
   std::size_t offset=kMainRomStart+(fn.vram-kMainVram);
   for(auto w:fn.words){be32(rom,offset,w);offset+=4;}
 }
 std::size_t offset=kLoaderRomOffset;
 for(auto w:originalLoaderInstructions()){be32(rom,offset,w);offset+=4;}
 for(std::size_t i=0;i<kOverlayBytes;++i)rom[kOverlayRomStart+i]=static_cast<std::uint8_t>(i*17+3);
 const char* path="cw_loader_synthetic.z64";
 {std::ofstream f(path,std::ios::binary);f.write((char*)rom.data(),rom.size());}
 NativeState state;std::string error;assert(state.load(path,&error));
 const auto& sig=originalLoaderInstructions();
 assert(state.romMatches(kLoaderRomOffset,sig.data(),sig.size()));
 assert(!state.romMatches(kRomSize-2,sig.data(),sig.size()));
 assert(!state.romMatches(kLoaderRomOffset,nullptr,sig.size()));
 std::uint64_t expected=0;
 assert(state.romRangeFNV64(kOverlayRomStart,kOverlayBytes,&expected));
 assert(state.writeU32(kOverlayVram,0xDEADBEEF));
 assert(state.writeU32(kOverlayPointerGlobal,0xDEADC0DE));
 GuestClock clock;GuestMessageQueue queue(1);NativeOverlayLoad loader(state,clock,queue);
 assert(loader.begin()&&!loader.begin()&&!loader.service());
 for(std::size_t i=0;i<kLoaderChunkCount;++i){
   if(i==1)assert(queue.send(42));
   clock.tickVi();
   if(i==1){assert(!loader.service());std::uint32_t msg=0;assert(queue.receive(&msg)&&msg==42);}
   assert(loader.service()&&!loader.failed()&&loader.completedChunks()==i+1);
 }
 std::uint32_t ptr=0;
 assert(loader.done()&&!loader.busy()&&loader.committedBytes()==kOverlayBytes);
 assert(state.readU32(kOverlayPointerGlobal,&ptr)&&ptr==kOverlayVram);
 assert(state.overlayFNV64()==expected&&!loader.service()&&!loader.begin());
 std::remove(path);
 std::cout<<"PASS: real loader MIPS signature + 7 PI transfers + queue backpressure + hash\n";
}
