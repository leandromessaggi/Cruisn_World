// ABI handlers for a bounded, single-thread original-boot handoff probe.
// Does not implement OS thread scheduling, blocking calls, or device I/O.
#include "boot_thread_handoff.hpp"
#include "recomp.h"
#include <cstring>
#include <cstdint>

namespace {
thread_local cw::BootHandoff* bound = nullptr;
constexpr std::size_t RDRAM_LIMIT = 8u * 1024u * 1024u;

bool guest_phys(std::uint64_t addr, std::size_t length, std::uint32_t& phys) {
    std::uint32_t guest=static_cast<std::uint32_t>(addr);
    const std::uint32_t region=guest & 0xE0000000u;
    if(region != 0x80000000u && region != 0xA0000000u) return false;
    phys=guest & 0x1FFFFFFFu;
    return phys <= RDRAM_LIMIT && length <= RDRAM_LIMIT - phys;
}
bool read_guest_word(const cw::BootHandoff& s, std::uint64_t addr, std::uint32_t& value) {
    std::uint32_t phys=0;
    if (!s.ram || s.size < RDRAM_LIMIT || !guest_phys(addr,4,phys) || (phys&3)) return false;
    std::memcpy(&value,s.ram+phys,4); // native word-swapped N64Recomp layout
    return true;
}
void invalid() noexcept { if(bound) bound->errors++; }
}
namespace cw { void bind_boot_handoff(BootHandoff* s) noexcept { bound=s; } }

extern "C" {
void osViBlack_recomp(std::uint8_t* rdram, recomp_context* ctx) {
    if (!bound || !ctx || !rdram || bound->ram!=rdram || bound->video_black || ctx->r4!=1) { invalid(); return; }
    bound->video_black=true;
}
void __osInitialize_common_recomp(std::uint8_t* rdram, recomp_context* ctx) {
    if (!bound || !ctx || bound->ram!=rdram || !bound->video_black || bound->initialized) { invalid(); return; }
    bound->initialized=true;
}
void osCreateThread_recomp(std::uint8_t* rdram, recomp_context* ctx) {
    if (!bound || !ctx || bound->ram!=rdram || !bound->initialized || bound->creates!=0) { invalid(); return; }
    auto& state=*bound;
    std::uint32_t thread_phys=0,stack=0,priority=0;
    if (!guest_phys(ctx->r4, 0x20, thread_phys) || (thread_phys&7u) ||
        !guest_phys(ctx->r6, 4, thread_phys) || (thread_phys&3u) ||
        !read_guest_word(state,ctx->r29 + 16,stack) ||
        !read_guest_word(state,ctx->r29 + 20,priority) ||
        !guest_phys(stack,4,thread_phys) || (thread_phys&7u) ||
        priority>255u) { invalid(); return; }
    state.first = {
        static_cast<std::uint32_t>(ctx->r4),
        static_cast<std::uint32_t>(ctx->r5),
        static_cast<std::uint32_t>(ctx->r6),
        static_cast<std::uint32_t>(ctx->r7),
        stack,priority,false
    };
    state.creates=1;
}
void osStartThread_recomp(std::uint8_t* rdram, recomp_context* ctx) {
    if (!bound || !ctx || bound->ram!=rdram || bound->creates!=1 ||
        bound->starts!=0 || static_cast<std::uint32_t>(ctx->r4)!=bound->first.thread_addr) {
        invalid(); return;
    }
    bound->first.started=true;
    bound->starts=1;
    // The guest thread is NOT executed here. A real N64ModernRuntime port
    // must preserve/restore a guest context and dispatch the guest entry.
}
}
