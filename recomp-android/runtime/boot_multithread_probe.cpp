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
    if (active->video_black || ctx->r4!=1) {error();return;}
    active->video_black=true;
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
    if(active->mesg_queues || active->thread_count!=2 || active->starts!=2 ||
       !ram_pointer(ctx->r4,24,q) || (q&3u) ||
       ctx->r6==0 || ctx->r6>256 ||
       !ram_pointer(ctx->r5,ctx->r6*4,m) || (m&3u)) {error();return;}
    active->mesg_queues=1;
    active->queue_addr=static_cast<std::uint32_t>(ctx->r4);
    active->queue_msgs=static_cast<std::uint32_t>(ctx->r5);
    active->queue_capacity=static_cast<std::uint32_t>(ctx->r6);
    if (active->on_first_queue) active->on_first_queue();
    // Stop immediately after first queue creation in private bounded harness.
}
}
