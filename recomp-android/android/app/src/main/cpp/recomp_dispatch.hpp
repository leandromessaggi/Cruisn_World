#pragma once
#include <cstdint>
#include <cstddef>
#include "recomp_context.hpp"
#include "recomp_functions.hpp"

namespace cw {
// Dispatch ONLY verified C++ translations at exact N64 VRAM entrypoints.
// Each call is atomic: no interpretation of opcodes, recursive guest stack,
// libultra thread implementation, or real CPU time slicing is provided.
enum class RecompStepResult { Executed, Returned, NotRecompiled, NoReadyThread, InvalidReturn, Failed };
class GuestRecompDispatcher final {
 public:
  GuestRecompDispatcher(NativeState& memory, GuestCpuContextScheduler& scheduler)
      : memory_(memory), scheduler_(scheduler) {}
  RecompStepResult step(CallTrace* trace=nullptr);
  std::size_t run(std::size_t budget, CallTrace* trace=nullptr);
 private:
  NativeState& memory_;
  GuestCpuContextScheduler& scheduler_;
};
}
