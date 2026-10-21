// ROM-free byte-swizzle, queue, event, and bounds tests for first boot PI DMA.
#include "boot_multithread_probe.hpp"
#include "recomp.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
extern "C" {
void osCreateMesgQueue_recomp(uint8_t*,recomp_context*);
void osPiStartDma_recomp(uint8_t*,recomp_context*);
void osRecvMesg_recomp(uint8_t*,recomp_context*);
void osInvalDCache_recomp(uint8_t*,recomp_context*);
}
static void word(std::uint8_t* p,std::uint32_t off,std::uint32_t w) {std::memcpy(p+off,&w,4);}
static std::uint32_t wordat(const std::uint8_t* p,std::uint32_t off) {
    std::uint32_t v=0;std::memcpy(&v,p+off,4);return v;
}
int main(){
    std::vector<uint8_t> mem(8u*1024*1024);
    std::vector<uint8_t> rom(0x1000);
    for(unsigned i=0;i<rom.size();++i)rom[i]=static_cast<uint8_t>((i*59u+11u)&255u);
    cw::BootMultiTrace t{};
    t.rdram=mem.data();t.rdram_bytes=mem.size();t.rom=rom.data();t.rom_bytes=rom.size();
    t.thread_count=2;t.starts=2;
    cw::bind_boot_multithread_probe(&t);
    recomp_context c{};
    c.r4=0x80360760;c.r5=0x803607F0;c.r6=1;
    osCreateMesgQueue_recomp(mem.data(),&c);
    assert(t.errors==0 && t.mesg_queues==1);
    assert(wordat(mem.data(),0x360760+16)==1 && wordat(mem.data(),0x360760+20)==0x803607F0);
    c.r4=0x803E7AE0;c.r5=0xC0;
    osInvalDCache_recomp(mem.data(),&c);
    assert(t.cache_invalidations==1);
    c.r29=0x80020000;
    c.r4=0x80360860;c.r5=0;c.r6=0;c.r7=0x100;
    word(mem.data(),0x20010,0x803E7AE0);word(mem.data(),0x20014,0xC0);
    word(mem.data(),0x20018,0x80360760);
    osPiStartDma_recomp(mem.data(),&c);
    assert(t.errors==0 && t.dma_reads==1 && t.queue_pending==1 && c.r2==0);
    assert(wordat(mem.data(),0x360760+8)==1 && wordat(mem.data(),0x3607F0)==0x80360860);
    for(unsigned i=0;i<0xC0;++i)assert(mem[(0x3E7AE0u+i)^3u]==rom[0x100+i]);
    // Duplicate DMA while the single-slot queue is occupied MUST fail.
    osPiStartDma_recomp(mem.data(),&c);
    assert(t.errors==1 && t.dma_reads==1 && c.r2==~0ull);
    c.r4=0x80360760;c.r5=0x803607FC;c.r6=1;
    osRecvMesg_recomp(mem.data(),&c);
    assert(c.r2==0 && t.queue_pending==0 && t.dma_receives==1 && t.errors==1);
    assert(wordat(mem.data(),0x3607FC)==0x80360860 && wordat(mem.data(),0x360760+8)==0);
    // Reading an empty queue must fail in this nonblocking-only test bridge.
    osRecvMesg_recomp(mem.data(),&c);
    assert(t.errors==2 && t.dma_receives==1 && c.r2==~0ull);
    cw::bind_boot_multithread_probe(nullptr);
    std::puts("PASS: ROM-free first-PI-DMA byte-swizzle, queue completion, duplicate and empty rejection");
}
