#include "recomp.h"
#include "boot_thread_handoff.hpp"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
extern "C" {
void osViBlack_recomp(uint8_t*,recomp_context*);
void __osInitialize_common_recomp(uint8_t*,recomp_context*);
void osCreateThread_recomp(uint8_t*,recomp_context*);
void osStartThread_recomp(uint8_t*,recomp_context*);
}
static void word(std::vector<uint8_t>& r,std::size_t off,std::uint32_t value){std::memcpy(r.data()+off,&value,4);}
int main(){
    std::vector<uint8_t> ram(8u*1024u*1024u,0);
    cw::BootHandoff boot{ram.data(),ram.size()};cw::bind_boot_handoff(&boot);
    recomp_context c{};
    c.r4=1; osViBlack_recomp(ram.data(),&c);
    __osInitialize_common_recomp(ram.data(),&c);
    c.r29=0x801503E0;word(ram,0x1503f0,0x8035B130);word(ram,0x1503f4,8);
    c.r4=0x80359F80;c.r5=1;c.r6=0x802AD4C0;c.r7=0;
    osCreateThread_recomp(ram.data(),&c);
    assert(boot.errors==0 && boot.creates==1 && boot.first.entry==0x802AD4C0u);
    assert(boot.first.stack==0x8035B130u && boot.first.priority==8);
    c.r4=0x80359F80;osStartThread_recomp(ram.data(),&c);
    assert(boot.starts==1 && boot.first.started && boot.errors==0);
    osStartThread_recomp(ram.data(),&c);
    assert(boot.errors==1 && boot.starts==1);
    // An invalid guest-stack address cannot modify the registry.
    cw::BootHandoff other{ram.data(),ram.size()};cw::bind_boot_handoff(&other);
    c.r4=1;osViBlack_recomp(ram.data(),&c);__osInitialize_common_recomp(ram.data(),&c);
    c.r29=0x807ffff0;c.r4=0x80359f80;osCreateThread_recomp(ram.data(),&c);
    assert(other.creates==0 && other.errors==1);
    c.r29=0x801503e0;c.r6=0x812AD4C0;osCreateThread_recomp(ram.data(),&c);
    assert(other.creates==0 && other.errors==2);
    cw::bind_boot_handoff(nullptr);
    puts("PASS: guest thread create/start handoff, priority, stack, address validation");
}
