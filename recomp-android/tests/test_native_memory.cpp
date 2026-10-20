#include "../android/app/src/main/cpp/recomp_memory.hpp"
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// A synthetic 12 MiB ROM; no Nintendo game assets are included in source control.
static void be32(std::vector<uint8_t>& rom, size_t p, uint32_t w) {
  for (int i=0;i<4;i++) rom[p+i]=static_cast<uint8_t>(w>>(24-8*i));
}
int main() {
  using namespace cw;
  std::vector<uint8_t> rom(kRomSize,0x42);
  be32(rom,0,0x80371240);be32(rom,0x10,0xDFE61153);be32(rom,0x14,0xD76118E6);
  const struct Fn {uint32_t addr; std::vector<uint32_t> words;} functions[] = {
    {0x802AEE9C,{0x240E0001,0x3C018033,0x03E00008,0xAC2E34C0}},
    {0x802AEEAC,{0x3C018033,0x03E00008,0xAC2034C0}},
    {0x802AF5E4,{0x240E0001,0x3C018033,0x03E00008,0xAC2E34C8}},
    {0x802AF5F4,{0x3C018033,0x03E00008,0xAC2034C8}}
  };
  for (auto f: functions) {
    size_t offset=kMainRomStart + (f.addr-kMainVram);
    for (auto w:f.words) {be32(rom,offset,w);offset+=4;}
  }
  const std::string path="synthetic_rom_for_test.z64";
  {std::ofstream f(path,std::ios::binary);f.write((char*)rom.data(),rom.size());}
  NativeState state;
  std::string err;
  assert(state.load(path,&err) && err.empty());
  uint32_t value = 0;
  assert(state.readU32(0x802AEE9C,&value) && value==0x240E0001);
  assert(state.isZero(kBssStart,kBssSize));
  assert(state.readU32(kOverlayVram,&value) && value==0x42424242);
  assert(state.readU32(kOverlayVram + (kOverlayRomEnd-kOverlayRomStart)-4,&value) && value==0x42424242);
  assert(state.callRecompiled(0x802AEE9C));
  assert(state.readU32(kFlag1Address,&value) && value==1);
  assert(state.callRecompiled(0x802AEEAC));
  assert(state.readU32(kFlag1Address,&value) && value==0);
  assert(state.callRecompiled(0x802AF5E4));
  assert(state.readU32(kFlag2Address,&value) && value==1);
  assert(state.callRecompiled(0x802AF5F4));
  assert(state.readU32(kFlag2Address,&value) && value==0);
  assert(!state.callRecompiled(0xDEADBEEF));
  assert(!state.readU32(0x7fffffff,&value));
  assert(!state.writeU32(0x807ffffe,1));
  assert(!state.isZero(kRamBase,kRamSize+1));
  // Check a real source instruction mismatch is caught, rather than simulated.
  rom[kMainRomStart + (functions[0].addr-kMainVram)] ^= 1;
  {std::ofstream f(path,std::ios::binary);f.write((char*)rom.data(),rom.size());}
  NativeState modified;
  assert(!modified.load(path,&err));
  std::remove(path.c_str());
  std::cout << "PASS: native state, overlay copy, four static setters, RAM bounds, ROM instruction guard\n";
}
