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
  // Seed every translated function's exact instruction words in a fully
  // synthetic ROM, so tests do not require distributing game assets.
  for (const auto& fn: compiledSignatures()) {
    size_t offset=kMainRomStart + (fn.vram-kMainVram);
    for (auto w:fn.words) {be32(rom,offset,w);offset+=4;}
  }
  const std::string path="synthetic_rom_for_test.z64";
  {std::ofstream f(path,std::ios::binary);f.write((char*)rom.data(),rom.size());}
  NativeState state;
  std::string err;
  assert(state.load(path,&err) && err.empty());
  assert(compiledSignatures().size() == 17);
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
  // New native call ABI and branch-delay semantics.
  CallArgs c;
  c.a0=0x80390000u; c.a1=0xBEEFBABEu;
  assert(state.callRecompiled(0x802B251Cu,&c));
  assert(state.readU32(c.a0+0x5C,&value) && value==c.a1);
  assert(state.writeU32(0x803334BCu,0));
  assert(state.callRecompiled(0x802AF038u,&c) && c.v0==0);
  assert(state.writeU32(0x803334BCu,0x55));
  assert(state.callRecompiled(0x802AF038u,&c) && c.v0==1);
  // Double-store return delay slot: both globals must be reset.
  assert(state.writeU32(0x80395D88u,111));
  assert(state.writeU32(0x80395D8Cu,222));
  assert(state.callRecompiled(0x802B07F0u));
  assert(state.readU32(0x80395D88u,&value) && value==0);
  assert(state.readU32(0x80395D8Cu,&value) && value==0);
  for (auto pc: {0x802AFE6Cu,0x802AFE88u}) {
    assert(state.callRecompiled(pc));
    assert(state.readU32(0x80334648u,&value) && value==1);
    assert(state.readU32(0x8033A4E8u,&value) && value==32);
  }
  assert(state.writeU32(0x803C42C4u,0x80390000u));
  assert(state.callRecompiled(0x802BF820u));
  std::uint8_t small=0;
  assert(state.readU8(0x80390018u,&small) && small==255);
  assert(state.writeU32(0x803956C0u,0x80380000u));
  c.a0=2;
  assert(state.writeU8(0x80380000u+2*12+3,0));
  assert(state.callRecompiled(0x802AF05Cu,&c) && c.v0==0);
  assert(state.writeU8(0x80380000u+2*12+3,9));
  assert(state.callRecompiled(0x802AF05Cu,&c) && c.v0==1);
  assert(state.writeU32(0x803956C0u,0xFFFFFF00u));
  c.a0=0;
  assert(!state.callRecompiled(0x802AF05Cu,&c));
  assert(state.writeU32(0x803CDBDCu,2));
  assert(state.writeU32(0x8033A4D0u+8,0x80390000u));
  assert(state.callRecompiled(0x802B2710u));
  assert(state.readU32(0x803CDBB4u,&value) && value==0x80390000u);
  // Null a1 bypasses a2, while beql only loads source when a2 is zero.
  c.a0=0x80390080u; c.a1=0; c.a2=13;
  assert(state.writeU32(c.a0+0x60,42));
  assert(state.callRecompiled(0x802B2544u,&c));
  assert(state.readU32(c.a0+0x60,&value) && value==42);
  c.a1=0x80380200u; c.a2=87;
  assert(state.callRecompiled(0x802B2544u,&c));
  assert(state.readU32(c.a0+0x60,&value) && value==87);
  c.a2=0;
  assert(state.writeU32(c.a1+4,0xAABBCCDD));
  assert(state.callRecompiled(0x802B2544u,&c));
  assert(state.readU32(c.a0+0x60,&value) && value==0xAABBCCDD);
  // IEEE754 bit-correct stores, integer-to-single conversion, and sign.
  c.f12=0x3F800000; c.f14=0xBF800000;
  assert(state.callRecompiled(0x802AF5A4u,&c));
  assert(state.readU32(0x803334B8u,&value) && value==1);
  assert(state.readU32(0x803956C8u,&value) && value==0x3F800000);
  assert(state.readU32(0x803956CCu,&value) && value==0xBF800000);
  assert(state.callRecompiled(0x802AF5C4u,&c));
  assert(state.readU32(0x803334B8u,&value) && value==2);
  c.a0=0x80390080u;c.a1=0xF00DC0DEu;c.a2=3;c.a3=0xFFFFFFF9u;
  assert(state.callRecompiled(0x802B2524u,&c));
  assert(state.readU32(c.a0+0x5C,&value) && value==c.a1);
  assert(state.readU32(c.a0+0x3C,&value) && value==0x40400000u); // 3.0f
  assert(state.readU32(c.a0+0x40,&value) && value==0xC0E00000u); // -7.0f
  assert(state.callRecompiled(0x802AFCC8u));
  // Passing an absent ABI context is only valid for functions with no args.
  assert(state.callRecompiled(0x802AFCC8u,nullptr));
  assert(!state.callRecompiled(0xDEADBEEF));
  assert(!state.readU32(0x7fffffff,&value));
  assert(!state.writeU32(0x807ffffe,1));
  assert(!state.isZero(kRamBase,kRamSize+1));
  // Check a real source instruction mismatch is caught, rather than simulated.
  rom[kMainRomStart + (compiledSignatures()[0].vram-kMainVram)] ^= 1;
  {std::ofstream f(path,std::ios::binary);f.write((char*)rom.data(),rom.size());}
  NativeState modified;
  assert(!modified.load(path,&err));
  std::remove(path.c_str());
  std::cout << "PASS: mapping, 17 native routines, MIPS return delay slots, branch-likely, FPU conversions, strict signatures\n";
}
