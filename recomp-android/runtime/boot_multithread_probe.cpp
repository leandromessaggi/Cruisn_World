#include "boot_multithread_probe.hpp"
#include "recomp.h"
#include <cstdint>
#include <cstring>

namespace {
thread_local cw::BootMultiTrace* active=nullptr;
constexpr std::size_t RAM_BYTES=8u*1024*1024;
void error() noexcept { if (active) ++active->errors; }
bool ram_pointer(std::uint64_t address,std::size_t length,std::uint32_t& offset) noexcept {
    const std::uint32_t addr=static_cast<std::uint32_t>(address);
    const std::uint32_t segment=addr & 0xE0000000u;
    if (!active || !active->rdram || active->rdram_bytes<RAM_BYTES ||
        (segment!=0x80000000u && segment!=0xA0000000u))return false;
    offset=addr & 0x1FFFFFFFu;
    return offset<=RAM_BYTES && length<=RAM_BYTES-offset;
}
bool write_word(std::uint64_t address,std::uint32_t value) noexcept {
    std::uint32_t p=0;
    if (!ram_pointer(address,4,p) || (p&3)) return false;
    std::memcpy(active->rdram+p,&value,4);
    return true;
}
bool get_word(std::uint64_t address,std::uint32_t& out) noexcept {
    std::uint32_t p=0;
    if (!ram_pointer(address,4,p) || (p&3))return false;
    std::memcpy(&out,active->rdram+p,4);
    return true;
}
bool check(std::uint8_t* rdram,recomp_context* ctx) noexcept {
    if (!active || !ctx || !rdram || rdram!=active->rdram) { error();return false; }
    return true;
}
}
namespace cw {
void bind_boot_multithread_probe(BootMultiTrace* trace) noexcept {active=trace;}
ObservedThread* find_thread(BootMultiTrace& t,std::uint32_t addr) noexcept {
    for(unsigned i=0;i<t.thread_count;++i)if(t.threads[i].address==addr)return &t.threads[i];
    return nullptr;
}
}
extern "C" {
void osViBlack_recomp(std::uint8_t* rdram,recomp_context* ctx) {
    if (!check(rdram,ctx))return;
    if (ctx->r4!=1 || active->vi_black_calls>=2 ||
        (active->vi_black_calls==1 && (active->vi_managers!=1 || active->mesg_queues!=3))) {
        error();return;
    }
    ++active->vi_black_calls;
    active->video_black=true;
    if (active->vi_black_calls==2 && active->on_vi_black) active->on_vi_black();
}
void osCreateViManager_recomp(std::uint8_t* rdram,recomp_context* ctx) {
    if(!check(rdram,ctx)) return;
    if(active->vi_managers || active->mesg_queues!=3 ||
       !active->video_black || ctx->r4!=0xFE) { error();return; }
    active->vi_managers=1;
    active->vi_priority=static_cast<std::uint32_t>(ctx->r4);
    // No actual VI timer, interrupts or worker thread in this bounded probe.
}
void __osInitialize_common_recomp(std::uint8_t* rdram,recomp_context* ctx) {
    if(!check(rdram,ctx))return;
    if (!active->video_black || active->initialized) {error();return;}
    active->initialized=true;
}
void osCreateThread_recomp(std::uint8_t* rdram,recomp_context* ctx) {
    if(!check(rdram,ctx))return;
    std::uint32_t thread_off=0,entry_off=0,sp_off=0,stack=0,priority=0;
    const auto addr=static_cast<std::uint32_t>(ctx->r4);
    const auto entry=static_cast<std::uint32_t>(ctx->r6);
    if (!active->initialized || active->thread_count>=3 ||
        !ram_pointer(addr,0x20,thread_off) || (thread_off&7u) ||
        !ram_pointer(entry,4,entry_off) || (entry_off&3u) ||
        !get_word(ctx->r29+16,stack) || !get_word(ctx->r29+20,priority) ||
        !ram_pointer(stack,4,sp_off) || (sp_off&7u) ||
        priority>255 || cw::find_thread(*active,addr)) {error();return;}
    active->threads[active->thread_count++]={addr,static_cast<std::uint32_t>(ctx->r5),
        entry,static_cast<std::uint32_t>(ctx->r7),stack,priority,false,false};
}
void osStartThread_recomp(std::uint8_t* rdram,recomp_context* ctx) {
    if(!check(rdram,ctx))return;
    auto* t=cw::find_thread(*active,static_cast<std::uint32_t>(ctx->r4));
    if(!t || t->started) {error();return;}
    t->started=true;
    ++active->starts;
}
void osCreatePiManager_recomp(std::uint8_t* rdram,recomp_context* ctx) {
    if(!check(rdram,ctx))return;
    std::uint32_t q=0,msg=0;
    if(active->pi_managers || active->thread_count!=1 || active->starts!=1 ||
       !ram_pointer(ctx->r5,24,q) || (q&3u) ||
       ctx->r7==0 || ctx->r7>256 ||
       !ram_pointer(ctx->r6,ctx->r7*4,msg) || (msg&3u)) {error();return;}
    active->pi_managers=1;
    active->manager_queue=static_cast<std::uint32_t>(ctx->r5);
    active->manager_msgs=static_cast<std::uint32_t>(ctx->r6);
    active->manager_capacity=static_cast<std::uint32_t>(ctx->r7);
    // PI task/device management NOT actually started. Observational only.
}
void osSetThreadPri_recomp(std::uint8_t* rdram,recomp_context* ctx) {
    if(!check(rdram,ctx))return;
    auto* t=cw::find_thread(*active,static_cast<std::uint32_t>(ctx->r4));
    if(!t || !t->started || active->priority_changes || ctx->r5>255) {error();return;}
    t->priority=static_cast<std::uint32_t>(ctx->r5);
    ++active->priority_changes;
}
void osCreateMesgQueue_recomp(std::uint8_t* rdram,recomp_context* ctx) {
    if(!check(rdram,ctx))return;
    std::uint32_t q=0,m=0;
    if(active->mesg_queues>=3 || active->thread_count!=2 || active->starts!=2 ||
       !ram_pointer(ctx->r4,24,q) || (q&3u) ||
       ctx->r6==0 || ctx->r6>256 ||
       !ram_pointer(ctx->r5,ctx->r6*4,m) || (m&3u)) {error();return;}
    const auto addr=static_cast<std::uint32_t>(ctx->r4);
    for(unsigned i=0;i<active->mesg_queues;++i)
        if(active->queues[i].address==addr) {error();return;}
    // Reject overlapping guest queue and message storage (including prior queues).
    if (m<q+24 && q<m+static_cast<std::uint32_t>(ctx->r6)*4) {error();return;}
    active->queues[active->mesg_queues]={addr,
        static_cast<std::uint32_t>(ctx->r5),static_cast<std::uint32_t>(ctx->r6)};
    ++active->mesg_queues;
    // Preserve the first queue for the bounded DMA/RecvMesg bridge.
    if(active->mesg_queues==1) {
        active->queue_addr=addr;
        active->queue_msgs=static_cast<std::uint32_t>(ctx->r5);
        active->queue_capacity=static_cast<std::uint32_t>(ctx->r6);
    }
    // N64 OSMesgQueue: mtqueue, fullqueue, validCount, first, msgCount, msg.
    // Host-side word layout of N64Recomp uses little-endian 32-bit words.
    const std::uint32_t guest=addr;
    if (!(write_word(guest+0,0) && write_word(guest+4,0) &&
          write_word(guest+8,0) && write_word(guest+12,0) &&
          write_word(guest+16,static_cast<std::uint32_t>(ctx->r6)) &&
          write_word(guest+20,static_cast<std::uint32_t>(ctx->r5)))) { error();return; }
    if (active->mesg_queues==1 && active->on_first_queue) active->on_first_queue();
    // Stop immediately after first queue creation in private bounded harness.
}
}

// Bounded, synchronous PI ROM-read bridge for the PRIVATE original-code probe.
// Production will require asynchronous PI manager threads and interrupts.
extern "C" void osInvalDCache_recomp(std::uint8_t* rdram,recomp_context* ctx) {
    if(!check(rdram,ctx))return;
    std::uint32_t off=0;
    if(ctx->r5==0 || ctx->r5>0x10000 ||
       !ram_pointer(ctx->r4,static_cast<std::size_t>(ctx->r5),off)) {error();return;}
    ++active->cache_invalidations;
}
extern "C" void osPiStartDma_recomp(std::uint8_t* rdram,recomp_context* ctx) {
    if(!check(rdram,ctx))return;
    std::uint32_t dst=0,ramoff=0,length=0;
    // a0=OSIoMesg, a1=priority, a2=OS_READ, a3=cart offset;
    // 5th, 6th and 7th args are guest stack words.
    if(!active->rom || active->rom_bytes==0 || active->mesg_queues!=1 ||
       active->dma_reads || active->queue_pending || ctx->r5!=0 || ctx->r6!=0 ||
       !get_word(ctx->r29+16,dst) || !get_word(ctx->r29+20,length) ||
       !get_word(ctx->r29+24,ramoff) ||
       ramoff!=active->queue_addr || length==0 || length>0x4000) {error();ctx->r2=~0ull;return;}
    const std::uint32_t source=static_cast<std::uint32_t>(ctx->r7);
    std::uint32_t start=0;
    if(source>active->rom_bytes || length>active->rom_bytes-source ||
       !ram_pointer(dst,length,start)) {error();ctx->r2=~0ull;return;}
    // N64Recomp uses word-swizzled guest RAM: a guest byte at A resides at
    // host RAM[(A & physical_mask) ^ 3] on little-endian hosts.
    for(std::uint32_t i=0;i<length;++i)
        active->rdram[(start+i)^3u]=active->rom[source+i];
    // The test PI device completes synchronously and enqueues the IO message.
    if(!write_word(active->queue_msgs,static_cast<std::uint32_t>(ctx->r4)) ||
       !write_word(active->queue_addr+8,1)) {error();ctx->r2=~0ull;return;}
    active->dma_cart=source;active->dma_ram=dst;active->dma_length=length;
    active->dma_queue=ramoff;active->message_value=static_cast<std::uint32_t>(ctx->r4);
    active->queue_pending=1;
    ++active->dma_reads; ++active->dma_completions;
    ctx->r2=0;
}
extern "C" void osRecvMesg_recomp(std::uint8_t* rdram,recomp_context* ctx) {
    if(!check(rdram,ctx))return;
    if(static_cast<std::uint32_t>(ctx->r4)!=active->queue_addr ||
       ctx->r6!=1 || !active->queue_pending || !active->dma_reads) {
        error();ctx->r2=~0ull;return;
    }
    // Optional a1: the game's first DMA waiter passes null.
    if(ctx->r5 && !write_word(ctx->r5,active->message_value)) {
        error();ctx->r2=~0ull;return;
    }
    if(!write_word(active->queue_addr+8,0)) {error();ctx->r2=~0ull;return;}
    active->queue_pending=0;
    ++active->dma_receives;
    ctx->r2=0;
    if(active->on_dma_received) active->on_dma_received();
}
