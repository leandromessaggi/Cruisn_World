#pragma once
#include <cstddef>
#include <cstdint>


namespace cw {
// A bounded host-side observation/validation bridge for the original
// recompiled entrypoint's first osCreateThread -> osStartThread sequence.
// This is deliberately NOT a scheduler or a libultra replacement runtime.
struct GuestThreadStart {
    std::uint32_t thread_addr = 0;
    std::uint32_t thread_id = 0;
    std::uint32_t entry = 0;
    std::uint32_t argument = 0;
    std::uint32_t stack = 0;
    std::uint32_t priority = 0;
    bool started = false;
};
struct BootHandoff {
    std::uint8_t* ram = nullptr;
    std::size_t size = 0;
    bool video_black = false;
    bool initialized = false;
    unsigned creates = 0;
    unsigned starts = 0;
    unsigned errors = 0;
    GuestThreadStart first{};
};
void bind_boot_handoff(BootHandoff* state) noexcept;
}
