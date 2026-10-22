// ROM-free deterministic regression for original USA NTSC boot VI requests.
// This is a *recording harness*, not a real libultra implementation.
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
void osViSetMode_recomp(uint8_t*,recomp_context*);
void osViSetSpecialFeatures_recomp(uint8_t*,recomp_context*);
void osSetEventMesg_recomp(uint8_t*,recomp_context*);
void osViSetEvent_recomp(uint8_t*,recomp_context*);
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
    c.r4=253;osCreateViManager_recomp(mem.data(),&c);
    assert(t.errors==1 && t.vi_managers==0);
    c.r4=254;osCreateViManager_recomp(mem.data(),&c);
    assert(t.errors==1 && t.vi_managers==1);
    c.r4=0x8034C3D0u;osViSetMode_recomp(mem.data(),&c);
    assert(t.errors==2 && t.vi_mode_calls==0); // can't set mode before second blackout
    c.r4=1;osViBlack_recomp(mem.data(),&c);
    assert(t.errors==2 && t.vi_black_calls==2 && callbacks==0);
    c.r4=0x8034C3D0u;osViSetMode_recomp(mem.data(),&c);
    assert(t.errors==2 && t.vi_mode_calls==1 && t.vi_mode==c.r4);
    c.r4=0x52;osViSetSpecialFeatures_recomp(mem.data(),&c);
    assert(t.errors==3 && t.vi_special_calls==0); // third black required
    c.r4=1;osViBlack_recomp(mem.data(),&c);
    assert(t.errors==3 && t.vi_black_calls==3 && callbacks==1);
    c.r4=0x40;osViSetSpecialFeatures_recomp(mem.data(),&c);
    assert(t.errors==4 && t.vi_special_calls==0); // USA NTSC expects 0x52
    c.r4=0x52;osViSetSpecialFeatures_recomp(mem.data(),&c);
    assert(t.errors==4 && t.vi_special_calls==1);
    c.r4=1;osViBlack_recomp(mem.data(),&c);
    assert(t.errors==5 && callbacks==1); // no fourth blackout
    const std::uint32_t ids[3]={4,9,14},values[3]={0x29B,0x29C,0x29D};
    for(unsigned i=0;i<3;++i) {
        c.r4=ids[i];c.r5=qa[1];c.r6=values[i]+1;
        osSetEventMesg_recomp(mem.data(),&c);
        assert(t.errors==6+i && t.vi_event_count==i); // invalid message
        c.r6=values[i];osSetEventMesg_recomp(mem.data(),&c);
        assert(t.errors==6+i && t.vi_event_count==i+1);
        assert(t.vi_events[i].queue==qa[1]);
    }
    c.r4=qa[1];c.r5=0x29A;c.r6=0;
    osViSetEvent_recomp(mem.data(),&c);
    assert(t.errors==9 && t.vi_retrace_registrations==0);
    c.r6=1;osViSetEvent_recomp(mem.data(),&c);
    assert(t.errors==9 && t.vi_retrace_registrations==1);
    assert(t.vi_retrace_queue==qa[1] && t.vi_retrace_message==0x29A);
    osViSetEvent_recomp(mem.data(),&c);
    assert(t.errors==10 && t.vi_retrace_registrations==1);
    cw::bind_boot_multithread_probe(nullptr);
    std::puts("PASS: ROM-free USA NTSC VI mode, three event registrations, retrace, guards");
}
