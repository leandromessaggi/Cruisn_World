// ROM-free regression of guest VI-init queue arguments and fail-closed hooks.
#include "boot_multithread_probe.hpp"
#include "recomp.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
extern "C" {
void osCreateMesgQueue_recomp(uint8_t*,recomp_context*);
void osCreateViManager_recomp(uint8_t*,recomp_context*);
void osViBlack_recomp(uint8_t*,recomp_context*);
}
static std::uint32_t load(const std::vector<std::uint8_t>& m, std::uint32_t off) {
    std::uint32_t v=0;std::memcpy(&v,&m[off & 0x1FFFFFFFu],4);return v;
}
static unsigned callbacks=0;
static void on_black() {++callbacks;}
int main() {
    std::vector<std::uint8_t> mem(8u*1024*1024);
    cw::BootMultiTrace t{};
    t.rdram=mem.data();t.rdram_bytes=mem.size();t.thread_count=2;t.starts=2;
    t.on_vi_black=on_black;
    cw::bind_boot_multithread_probe(&t);
    recomp_context c{};
    // Root thread blacks out video before spawning thread 1.
    c.r4=1;osViBlack_recomp(mem.data(),&c);
    assert(t.video_black && t.vi_black_calls==1 && t.errors==0);
    const std::uint32_t qa[3]={0x80360760,0x8035E4D0,0x8035E508};
    const std::uint32_t ma[3]={0x803607F0,0x8035E4E8,0x8035E520};
    for(unsigned i=0;i<3;++i) {
        c.r4=qa[i];c.r5=ma[i];c.r6=i==0?1:8;
        osCreateMesgQueue_recomp(mem.data(),&c);
        assert(t.errors==0 && t.mesg_queues==i+1);
        assert(load(mem,qa[i]+0)==0 && load(mem,qa[i]+4)==0);
        assert(load(mem,qa[i]+8)==0 && load(mem,qa[i]+12)==0);
        assert(load(mem,qa[i]+16)==c.r6 && load(mem,qa[i]+20)==ma[i]);
    }
    c.r4=qa[2];c.r5=ma[2];c.r6=8;
    osCreateMesgQueue_recomp(mem.data(),&c);
    assert(t.errors==1 && t.mesg_queues==3); // full/duplicate
    c.r4=253;osCreateViManager_recomp(mem.data(),&c);
    assert(t.errors==2 && t.vi_managers==0); // wrong priority
    c.r4=254;osCreateViManager_recomp(mem.data(),&c);
    assert(t.errors==2 && t.vi_managers==1 && t.vi_priority==254);
    c.r4=1;osViBlack_recomp(mem.data(),&c);
    assert(t.errors==2 && t.vi_black_calls==2 && callbacks==1);
    osViBlack_recomp(mem.data(),&c);
    assert(t.errors==3 && callbacks==1); // third black forbidden by bounded probe
    cw::bind_boot_multithread_probe(nullptr);
    std::puts("PASS: ROM-free original VI setup queue layout, manager priority and fail-closed calls");
}
