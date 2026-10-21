#pragma once
// Original Cruis'n World US boot-state values. This module does not emulate
// the CIC boot ROM and does not include copyrighted game data.
#include <cstddef>
#include <cstdint>

namespace cw {
using GuestWriteWord = bool (*)(void* user, std::uint32_t guest_address,
                                std::uint32_t value);
using GuestWriteBytes = bool (*)(void* user, std::uint32_t guest_address,
                                 const std::uint8_t* bytes, std::size_t length);
struct Cic6106Writer {
    void* user = nullptr;
    GuestWriteWord write_word = nullptr;
    GuestWriteBytes write_bytes = nullptr;
};

// Called only after the user's ROM has been verified by the enclosing program.
// Returns false on incompatible ROM or any guest-memory write error. No
// fallback-to-success path: failed initialization prevents boot.
bool seed_world_us_cic6106(const Cic6106Writer& dst,
                           const std::uint8_t* rom, std::size_t rom_size) noexcept;
} // namespace cw
