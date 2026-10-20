#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "recomp_functions.hpp"

namespace cw {
// This is a memory image for a static recompilation project, NOT an N64 emulator.
// A real runtime and renderer are necessary before this can execute gameplay.
constexpr std::size_t kRamSize = 8u * 1024u * 1024u;
constexpr std::size_t kRomSize = 0xC00000u;
constexpr std::uint32_t kRamBase = 0x80000000u;
constexpr std::uint32_t kMainVram = 0x802AD400u;
constexpr std::size_t kMainRomStart = 0x1000u;
constexpr std::size_t kMainRomEnd = 0xA6F80u;
constexpr std::uint32_t kBssStart = 0x80353380u;
constexpr std::size_t kBssSize = 0x94760u;
constexpr std::size_t kOverlayRomStart = 0xA7040u;
constexpr std::size_t kOverlayRomEnd = 0xC12A0u;
constexpr std::uint32_t kOverlayVram = 0x80050400u;
constexpr std::uint32_t kFlag1Address = 0x803334C0u;
constexpr std::uint32_t kFlag2Address = 0x803334C8u;

class NativeState final {
 public:
  NativeState();
  // Reads exact 12 MiB .z64 and maps the real ROM ranges into guest-addressed RAM.
  bool load(const std::string& rom_path, std::string* error);
  bool valid() const { return valid_; }
  bool readU8(std::uint32_t vram, std::uint8_t* out) const;
  bool writeU8(std::uint32_t vram, std::uint8_t value);
  bool readU32(std::uint32_t vram, std::uint32_t* out) const;
  bool writeU32(std::uint32_t vram, std::uint32_t value);
  bool isZero(std::uint32_t address, std::size_t size) const;
  std::uint64_t overlayFNV64() const;
  // Bounded static native dispatch; CallTrace records exactly which bodies execute.
  bool callRecompiled(std::uint32_t function_vram, CallArgs* args = nullptr, CallTrace* trace = nullptr);
  std::string diagnostics() const;
 private:
  bool offset(std::uint32_t vram, std::size_t size, std::size_t* out) const;
  std::vector<std::uint8_t> ram_;
  bool valid_ = false;
};
}
