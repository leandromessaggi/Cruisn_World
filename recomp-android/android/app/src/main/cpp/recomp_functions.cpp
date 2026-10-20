#include "recomp_functions.hpp"
#include "recomp_memory.hpp"
#include <cstring>
#include <sstream>

namespace cw {
namespace {
std::uint32_t be32(const std::uint8_t* p) {
  return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
         (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]);
}
std::uint32_t asBits(float f) {
  std::uint32_t bits = 0;
  static_assert(sizeof(bits)==sizeof(f), "Expected 32-bit float");
  std::memcpy(&bits, &f, sizeof(bits));
  return bits;
}
} // namespace
const std::vector<FunctionSignature>& compiledSignatures() {
  static const std::vector<FunctionSignature> kFunctions = {
    {0x802AF6ECu, "wrapper_reset_flag_and_set_334cc", {0x27BDFFE8u, 0xAFBF0014u, 0x0C0ABD7Du, 0x00000000u, 0x8FBF0014u, 0x240E0001u, 0x3C018033u, 0xAC2E34CCu, 0x03E00008u, 0x27BD0018u}},
    {0x802AEE9Cu, "set_334c0_1", {0x240E0001u, 0x3C018033u, 0x03E00008u, 0xAC2E34C0u}},
    {0x802AEEACu, "set_334c0_0", {0x3C018033u, 0x03E00008u, 0xAC2034C0u}},
    {0x802AF5E4u, "set_334c8_1", {0x240E0001u, 0x3C018033u, 0x03E00008u, 0xAC2E34C8u}},
    {0x802AF5F4u, "set_334c8_0", {0x3C018033u, 0x03E00008u, 0xAC2034C8u}},
    {0x802AFCC8u, "noop", {0x03E00008u, 0x00000000u}},
    {0x802B251Cu, "object_store_word", {0x03E00008u, 0xAC85005Cu}},
    {0x802BF820u, "set_struct_byte_255", {0x3C0F803Cu, 0x8DEF42C4u, 0x240E00FFu, 0x03E00008u, 0xA1EE0018u}},
    {0x802AF038u, "get_334bc_bool", {0x3C0E8033u, 0x8DCE34BCu, 0x00001025u, 0x11C00003u, 0x00000000u, 0x03E00008u, 0x24020001u, 0x03E00008u, 0x00000000u}},
    {0x802B07F0u, "reset_395d88_395d8c", {0x240E0000u, 0x240F0000u, 0x3C018039u, 0xAC2E5D88u, 0x03E00008u, 0xAC2F5D8Cu}},
    {0x802AFE6Cu, "set_334648_and_33a4e8_a", {0x240E0001u, 0x3C018033u, 0xAC2E4648u, 0x3C018034u, 0x240F0020u, 0x03E00008u, 0xAC2FA4E8u}},
    {0x802AFE88u, "set_334648_and_33a4e8_b", {0x240E0001u, 0x3C018033u, 0xAC2E4648u, 0x3C018034u, 0x240F0020u, 0x03E00008u, 0xAC2FA4E8u}},
    {0x802B2710u, "select_indexed_global", {0x3C0E803Cu, 0x8DCEDBDCu, 0x3C188034u, 0x3C01803Cu, 0x000E7880u, 0x030FC021u, 0x8F18A4D0u, 0x03E00008u, 0xAC38DBB4u}},
    {0x802AF05Cu, "get_indexed_u8_flag", {0x3C0E8039u, 0x8DCE56C0u, 0x00047880u, 0x01E47823u, 0x000F7880u, 0x01CFC021u, 0x93190003u, 0x00001025u, 0x13200003u, 0x00000000u, 0x03E00008u, 0x24020001u, 0x03E00008u, 0x00000000u}},
    {0x802AF5A4u, "configure_float_flags_a", {0x240E0001u, 0x3C018033u, 0xAC2E34B8u, 0x3C018039u, 0xE42C56C8u, 0x3C018039u, 0x03E00008u, 0xE42E56CCu}},
    {0x802AF5C4u, "configure_float_flags_b", {0x240E0002u, 0x3C018033u, 0xAC2E34B8u, 0x3C018039u, 0xE42C56C8u, 0x3C018039u, 0x03E00008u, 0xE42E56CCu}},
    {0x802B2544u, "set_object_optional_word", {0x10A00007u, 0x00000000u, 0x50C00004u, 0x8CAE0004u, 0x03E00008u, 0xAC860060u, 0x8CAE0004u, 0xAC8E0060u, 0x03E00008u, 0x00000000u}},
    {0x802B2524u, "set_object_float_pair", {0x44862000u, 0x44874000u, 0xAC85005Cu, 0x468021A0u, 0x468042A0u, 0xE486003Cu, 0x03E00008u, 0xE48A0040u}},
  };
  return kFunctions;
}

bool verifyFunctionSignatures(const std::vector<std::uint8_t>& rom, std::string* error) {
  for (const auto& func: compiledSignatures()) {
    constexpr std::size_t kRomBase=0x1000u;
    constexpr std::uint32_t kVramBase=0x802AD400u;
    if (func.vram < kVramBase) return false;
    const std::size_t start = kRomBase + (func.vram-kVramBase);
    if (start > rom.size() || func.words.size() > (rom.size()-start)/4u) return false;
    for (std::size_t i=0; i<func.words.size();++i) {
      if (be32(rom.data()+start+i*4)!=func.words[i]) {
        if (error) { std::ostringstream oss; oss << "Instrucao divergente em " << func.label;
                     *error = oss.str(); }
        return false;
      }
    }
  }
  return true;
}

// Independently translated from the exact verified MIPS instructions for each
// address. Branches, including MIPS branch-likely, and jr delay slots are
// encoded directly as C++ control flow; no opcodes are interpreted at runtime.
bool invokeRecompiled(NativeState& mem, std::uint32_t address, CallArgs* args, CallTrace* trace) {
  if (!mem.valid() || (trace && !trace->record(address))) return false;
  CallArgs empty;
  CallArgs& c = args?*args:empty;
  std::uint32_t word=0, base=0;
  switch (address) {
    case 0x802AF6ECu:
      // Exact flow from the US ROM at 0x802AF6EC:
      //   jal 0x802AF5F4; nop (JAL delay slot)
      //   sw 1, 0x803334CC; jr ra; addiu sp, sp, 0x18.
      // The callee is another independently translated static C++ function.
      if (!mem.callRecompiled(0x802AF5F4u, &c, trace)) return false;
      return mem.writeU32(0x803334CCu, 1u);
    case 0x802AEE9Cu: return mem.writeU32(0x803334C0u,1);
    case 0x802AEEACu: return mem.writeU32(0x803334C0u,0);
    case 0x802AF5E4u: return mem.writeU32(0x803334C8u,1);
    case 0x802AF5F4u: return mem.writeU32(0x803334C8u,0);
    case 0x802AFCC8u: return true; // jr ra, nop: genuine empty function
    case 0x802B251Cu: // jr ra; delay-slot sw a1,0x5c(a0)
      return mem.writeU32(c.a0+0x5Cu,c.a1);
    case 0x802BF820u: // pointer from global, byte store in return delay slot
      if (!mem.readU32(0x803C42C4u,&base)) return false;
      return mem.writeU8(base+0x18u,0xFFu);
    case 0x802AF038u: // return !!global using beq and jr delay slots
      if (!mem.readU32(0x803334BCu,&word)) return false;
      c.v0=word?1:0;
      return true;
    case 0x802B07F0u: // two zero stores, second in return delay slot
      return mem.writeU32(0x80395D88u,0) && mem.writeU32(0x80395D8Cu,0);
    case 0x802AFE6Cu:
    case 0x802AFE88u: // identical code at two distinct ROM entrypoints
      return mem.writeU32(0x80334648u,1) && mem.writeU32(0x8033A4E8u,32);
    case 0x802B2710u: // selected pointer indexed by global integer * 4
      if (!mem.readU32(0x803CDBDCu,&word)) return false;
      if (!mem.readU32(0x8033A4D0u+word*4u,&base)) return false;
      return mem.writeU32(0x803CDBB4u,base);
    case 0x802AF05Cu: { // boolean result from byte 3 of 12-byte table element
      if (!mem.readU32(0x803956C0u,&base)) return false;
      std::uint8_t b=0;
      if (!mem.readU8(base+c.a0*12u+3u,&b)) return false;
      c.v0=b?1:0;
      return true;
    }
    case 0x802AF5A4u:
    case 0x802AF5C4u: // store the source f12/f14 raw bits, not float conversions
      return mem.writeU32(0x803334B8u,address==0x802AF5A4u?1:2) &&
             mem.writeU32(0x803956C8u,c.f12) &&
             mem.writeU32(0x803956CCu,c.f14);
    case 0x802B2544u: // beq and beql: delay-slot load only on a1!=0 && a2==0
      if (!c.a1) return true;
      if (c.a2) return mem.writeU32(c.a0+0x60u,c.a2);
      if (!mem.readU32(c.a1+4u,&word)) return false;
      return mem.writeU32(c.a0+0x60u,word);
    case 0x802B2524u: // mtc1, cvt.s.w, swc1; signed integer -> IEEE754 single
      return mem.writeU32(c.a0+0x5Cu,c.a1) &&
        mem.writeU32(c.a0+0x3Cu,asBits(static_cast<float>(static_cast<std::int32_t>(c.a2)))) &&
        mem.writeU32(c.a0+0x40u,asBits(static_cast<float>(static_cast<std::int32_t>(c.a3))));
    default: return false; // No fallback to interpretation of unknown MIPS.
  }
}
} // namespace cw
