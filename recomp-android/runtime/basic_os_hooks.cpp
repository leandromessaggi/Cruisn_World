// Initial ABI bridge from actual N64Recomp-generated C to a *bounded* subset
// of libultra entrypoints. RDRAM is in N64Recomp's word-swapped native layout.
// Does NOT implement blocking wait, interrupts, threads, device ownership,
// cartridge access, video, audio, or game boot.  Not safe to launch gameplay.
#include "recomp.h"
#include <cstdint>
#include <cstring>

namespace {
constexpr std::uint32_t kRamBytes = 8u * 1024u * 1024u;
constexpr std::uint32_t kMaxMessages = 1024;
constexpr std::uint32_t kQueueBytes = 24;

// Only KSEG0 and KSEG1 guest virtual pointers currently map to RDRAM.
bool guest_ptr(std::uint64_t reg, std::size_t bytes, std::uint32_t& offset) {
    const std::uint32_t addr = static_cast<std::uint32_t>(reg);
    if ((addr & 0xE0000000u) != 0x80000000u &&
        (addr & 0xE0000000u) != 0xA0000000u) return false;
    const std::uint32_t phys=addr & 0x1FFFFFFFu;
    if (phys > kRamBytes || bytes > kRamBytes - phys) return false;
    offset=phys;
    return true;
}
bool load_word(const std::uint8_t* rdram, std::uint64_t reg, std::uint32_t& value) {
    std::uint32_t off=0;
    if (!guest_ptr(reg,4,off) || (off & 3u)) return false;
    // N64Recomp MEM_W expects native-endian words, unlike big-endian .z64 data.
    std::memcpy(&value,rdram+off,4);
    return true;
}
bool store_word(std::uint8_t* rdram, std::uint64_t reg, std::uint32_t value) {
    std::uint32_t off=0;
    if (!guest_ptr(reg,4,off) || (off & 3u)) return false;
    std::memcpy(rdram+off,&value,4);
    return true;
}
bool add_addr(std::uint64_t base, std::uint32_t offset, std::uint64_t& out) {
    if (static_cast<std::uint32_t>(base) > UINT32_MAX-offset) return false;
    out=static_cast<std::uint32_t>(base)+offset;
    return true;
}
bool queue_read(const std::uint8_t* ram, std::uint64_t q, std::uint32_t& valid,
                std::uint32_t& first, std::uint32_t& capacity, std::uint32_t& messages) {
    std::uint32_t off=0;
    if (!guest_ptr(q,kQueueBytes,off) || (off & 3u)) return false;
    std::uint64_t addr=0;
    if (!add_addr(q,8,addr) || !load_word(ram,addr,valid) ||
        !add_addr(q,12,addr) || !load_word(ram,addr,first) ||
        !add_addr(q,16,addr) || !load_word(ram,addr,capacity) ||
        !add_addr(q,20,addr) || !load_word(ram,addr,messages)) return false;
    std::uint32_t physical=0;
    return capacity>0 && capacity<=kMaxMessages && valid<=capacity &&
        first<capacity && guest_ptr(messages,capacity*4,physical) && !(physical & 3u);
}
void result(recomp_context* ctx, bool ok) {
    ctx->r2=static_cast<std::uint64_t>(static_cast<std::int64_t>(ok ? 0 : -1));
}
}

extern "C" {
void osCreateMesgQueue_recomp(std::uint8_t* ram, recomp_context* ctx) {
    const std::uint32_t count=static_cast<std::uint32_t>(ctx->r6);
    std::uint32_t a=0,b=0;
    if (!guest_ptr(ctx->r4,kQueueBytes,a) || (a&3u) ||
        !count || count>kMaxMessages || !guest_ptr(ctx->r5,count*4,b) || (b&3u)) {
        // libultra returns void; record failure in r2 solely as diagnostic.
        result(ctx,false);
        return;
    }
    const std::uint32_t q=static_cast<std::uint32_t>(ctx->r4);
    for (std::uint32_t i=0;i<5;i++) store_word(ram,q+i*4, i==4 ? count : 0);
    store_word(ram,q+20,static_cast<std::uint32_t>(ctx->r5));
    result(ctx,true);
}
void osSendMesg_recomp(std::uint8_t* ram, recomp_context* ctx) {
    std::uint32_t valid,first,capacity,messages;
    if (!queue_read(ram,ctx->r4,valid,first,capacity,messages) || valid==capacity) {
        // Blocking queues need a scheduler; fail rather than fake a completed send.
        result(ctx,false);
        return;
    }
    const std::uint64_t slot=std::uint64_t(messages)+((first+valid)%capacity)*4;
    result(ctx,store_word(ram,slot,static_cast<std::uint32_t>(ctx->r5)) &&
               store_word(ram,static_cast<std::uint32_t>(ctx->r4)+8,valid+1));
}
void osRecvMesg_recomp(std::uint8_t* ram, recomp_context* ctx) {
    std::uint32_t valid,first,capacity,messages;
    if (!queue_read(ram,ctx->r4,valid,first,capacity,messages) || valid==0) {
        result(ctx,false);
        return;
    }
    const std::uint64_t slot=std::uint64_t(messages)+first*4;
    std::uint32_t msg=0;
    if (!load_word(ram,slot,msg) ||
        (ctx->r5 && !store_word(ram,ctx->r5,msg))) {
        result(ctx,false);
        return;
    }
    const std::uint32_t q=static_cast<std::uint32_t>(ctx->r4);
    result(ctx,store_word(ram,q+12,(first+1)%capacity) &&
               store_word(ram,q+8,valid-1));
}
void osVirtualToPhysical_recomp(std::uint8_t*, recomp_context* ctx) {
    const std::uint32_t addr=static_cast<std::uint32_t>(ctx->r4);
    const bool mapped=(addr&0xE0000000u)==0x80000000u ||
                      (addr&0xE0000000u)==0xA0000000u;
    const std::uint32_t result=mapped ? (addr&0x1FFFFFFFu) : UINT32_MAX;
    ctx->r2=static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(result)));
}
// Cache maintenance for host-coherent RDRAM, not for host device buffers.
void osInvalDCache_recomp(std::uint8_t*, recomp_context*) {}
void osWritebackDCache_recomp(std::uint8_t*, recomp_context*) {}
void osWritebackDCacheAll_recomp(std::uint8_t*, recomp_context*) {}
}
