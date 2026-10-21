// Synthetic negative and positive validation, without game code or ROM.
#include "boot_multithread_probe.hpp"
#include "recomp.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
extern "C" {
void osViBlack_recomp(uint8_t*,recomp_context*);
void __osInitialize_common_recomp(uint8_t*,recomp_context*);
void osCreateThread_recomp(uint8_t*,recomp_context*);
void osStartThread_recomp(uint8_t*,recomp_context*);
void osCreatePiManager_recomp(uint8_t*,recomp_context*);
void osSetThreadPri_recomp(uint8_t*,recomp_context*);
void osCreateMesgQueue_recomp(uint8_t*,recomp_context*);
}
static void word(uint8_t* p,std::uint32_t off,std::uint32_t w) {std::memcpy(p+off,&w,4);}
int main(){
 std::vector<std::uint8_t> ram(8u*1024*1024);
 cw::BootMultiTrace t{};t.rdram=ram.data();t.rdram_bytes=ram.size();
 cw::bind_boot_multithread_probe(&t);
 recomp_context c{};
 c.r4=1;osViBlack_recomp(ram.data(),&c);__osInitialize_common_recomp(ram.data(),&c);
 c.r29=0x80020000;c.r4=0x80359F80;c.r5=1;c.r6=0x802AD4C0;c.r7=0;
 word(ram.data(),0x20010,0x8035B130);word(ram.data(),0x20014,8);
 osCreateThread_recomp(ram.data(),&c);assert(t.thread_count==1 && t.errors==0);
 osCreateThread_recomp(ram.data(),&c);assert(t.thread_count==1 && t.errors==1);
 c.r4=0x80359F80;osStartThread_recomp(ram.data(),&c);assert(t.starts==1);
 c.r4=0x96;c.r5=0x80360748;c.r6=0x80360728;c.r7=8;
 osCreatePiManager_recomp(ram.data(),&c);assert(t.pi_managers==1);
 c.r4=0x8035B130;c.r5=3;c.r6=0x802AD8C0;c.r7=0;
 word(ram.data(),0x20010,0x8035D2E0);word(ram.data(),0x20014,10);
 osCreateThread_recomp(ram.data(),&c);assert(t.thread_count==2 && t.errors==1);
 c.r4=0x8035B130;osStartThread_recomp(ram.data(),&c);
 c.r4=0x80359F80;c.r5=0;osSetThreadPri_recomp(ram.data(),&c);
 assert(t.starts==2 && t.priority_changes==1 && t.threads[0].priority==0);
 c.r4=0x80360760;c.r5=0x803607F0;c.r6=1;
 osCreateMesgQueue_recomp(ram.data(),&c);
 assert(t.mesg_queues==1 && t.queue_addr==0x80360760 && t.errors==1);
 cw::bind_boot_multithread_probe(nullptr);
 std::puts("PASS: synthetic ROM-free thread handoff + PI manager + third-thread queue, invalid duplicate rejected");
}
