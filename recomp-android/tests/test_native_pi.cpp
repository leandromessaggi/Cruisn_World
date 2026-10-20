#include "../android/app/src/main/cpp/recomp_pi.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>
static void set_be32(std::vector<uint8_t>& b,size_t p,uint32_t w) {
  for(int i=0;i<4;i++) b[p+i]=static_cast<uint8_t>(w>>(24-8*i));
}
int main() {
  using namespace cw;
  std::vector<uint8_t> rom(kRomSize,0x42);
  set_be32(rom,0,0x80371240);
  set_be32(rom,0x10,0xDFE61153);
  set_be32(rom,0x14,0xD76118E6);
  for(const auto& fn:compiledSignatures()) {
    size_t off=kMainRomStart+(fn.vram-kMainVram);
    for(auto word:fn.words){set_be32(rom,off,word);off+=4;}
  }
  for(size_t i=0;i<32;i++)rom[kOverlayRomStart+i]=static_cast<uint8_t>(i+10);
  const char* path="synthetic_pi_rom.z64";
  {std::ofstream f(path,std::ios::binary);
    f.write(reinterpret_cast<const char*>(rom.data()),static_cast<std::streamsize>(rom.size()));}
  NativeState ram;std::string err;
  assert(ram.load(path,&err));
  GuestClock clock;GuestMessageQueue queue(1);
  GuestPiDma pi(ram,clock,queue);
  const uint32_t dst=0xA0050400u;
  uint32_t word=0,msg=0;
  assert(ram.writeU32(dst,0xDEADBEEFu));
  assert(!pi.startRead(kCartBase-1,dst,4,1));
  assert(!pi.startRead(kCartBase+static_cast<uint32_t>(kRomSize)-1,dst,4,1));
  assert(!pi.startRead(kCartBase+static_cast<uint32_t>(kOverlayRomStart),0x807FFFFEu,4,1));
  assert(!pi.startRead(kCartBase+static_cast<uint32_t>(kOverlayRomStart),dst,0,1));
  assert(pi.startRead(kCartBase+static_cast<uint32_t>(kOverlayRomStart),dst,32,0xC001u));
  assert(!pi.startRead(kCartBase,dst,1,4));
  assert(!pi.service());
  assert(ram.readU32(dst,&word)&&word==0xDEADBEEFu);
  assert(queue.send(0x44));
  clock.tickVi();
  assert(!pi.service()&&pi.busy());
  assert(ram.readU32(dst,&word)&&word==0xDEADBEEFu);
  assert(queue.receive(&msg)&&msg==0x44);
  assert(pi.service()&&!pi.busy()&&pi.completed()==1);
  assert(ram.readU32(kOverlayVram,&word)&&word==0x0A0B0C0Du);
  assert(queue.receive(&msg)&&msg==0xC001u);
  assert(pi.startRead(kCartBase+static_cast<uint32_t>(kOverlayRomStart+4),dst+8,4,0xC002));
  assert(!pi.service());
  clock.tickVi();
  assert(pi.service()&&pi.completed()==2);
  assert(ram.readU32(dst+8,&word)&&word==0x0E0F1011u);
  assert(queue.receive(&msg)&&msg==0xC002u);
  std::remove(path);
  std::cout<<"PASS: cart ROM DMA, bounds, KSEG1, VI completion and event backpressure\n";
}
