#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace cw {
class NativeState;
// Minimal native-call ABI. Only the registers used by supported leaf functions
// are passed. This is NOT a guest CPU interpreter.
struct CallArgs {
  std::uint32_t a0=0, a1=0, a2=0, a3=0;
  std::uint32_t v0=0;
  // Raw bit representation of floating point argument registers f12 and f14.
  std::uint32_t f12=0, f14=0;
};
struct FunctionSignature {
  std::uint32_t vram;
  const char* label;
  std::vector<std::uint32_t> words;
};
const std::vector<FunctionSignature>& compiledSignatures();
bool verifyFunctionSignatures(const std::vector<std::uint8_t>& rom, std::string* error);
bool invokeRecompiled(NativeState& state, std::uint32_t vram, CallArgs* args);
} // namespace cw
