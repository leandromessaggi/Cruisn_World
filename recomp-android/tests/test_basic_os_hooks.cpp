#include "recomp.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
void osCreateMesgQueue_recomp(std::uint8_t*,recomp_context*);
void osSendMesg_recomp(std::uint8_t*,recomp_context*);
void osRecvMesg_recomp(std::uint8_t*,recomp_context*);
void osVirtualToPhysical_recomp(std::uint8_t*,recomp_context*);
void osInvalDCache_recomp(std::uint8_t*,recomp_context*);
void osWritebackDCache_recomp(std::uint8_t*,recomp_context*);
void osWritebackDCacheAll_recomp(std::uint8_t*,recomp_context*);
}
static std::uint32_t word(const std::vector<std::uint8_t>& ram,unsigned offset) {
    std::uint32_t v=0;std::memcpy(&v,ram.data()+offset,4);return v;
}
int main(){
    std::vector<std::uint8_t> ram(8*1024*1024,0);
    recomp_context c{};
    c.r4=0x80002000;c.r5=0xA0002100;c.r6=2;
    osCreateMesgQueue_recomp(ram.data(),&c);
    assert(c.r2==0 && word(ram,0x2010)==2 && word(ram,0x2014)==0xA0002100u);
    c.r4=0xA0002000;c.r5=0x80301234;c.r6=0;
    osSendMesg_recomp(ram.data(),&c);
    assert(c.r2==0 && word(ram,0x2100)==0x80301234 && word(ram,0x2008)==1);
    c.r5=0x80009988;osSendMesg_recomp(ram.data(),&c);
    assert(c.r2==0 && word(ram,0x2104)==0x80009988 && word(ram,0x2008)==2);
    c.r5=123; c.r6=1;osSendMesg_recomp(ram.data(),&c);
    assert((std::int64_t)c.r2==-1 && word(ram,0x2008)==2);
    c.r5=0x80002500;osRecvMesg_recomp(ram.data(),&c);
    assert(c.r2==0 && word(ram,0x2500)==0x80301234 && word(ram,0x2008)==1);
    c.r5=0;osRecvMesg_recomp(ram.data(),&c);
    assert(c.r2==0 && word(ram,0x2008)==0);
    osRecvMesg_recomp(ram.data(),&c);
    assert((std::int64_t)c.r2==-1);
    c.r4=0x80002000;c.r5=0x80003000;c.r6=0;
    osCreateMesgQueue_recomp(ram.data(),&c);
    assert((std::int64_t)c.r2==-1 && word(ram,0x2010)==2); // Invalid create does not mutate
    c.r4=0x80002000;c.r5=0x80003000;c.r6=2;
    osCreateMesgQueue_recomp(ram.data(),&c);
    assert(c.r2==0);
    c.r4=0x8033ABCD;osVirtualToPhysical_recomp(ram.data(),&c);
    assert(c.r2==0x0033ABCD);
    c.r4=0xA0123456;osVirtualToPhysical_recomp(ram.data(),&c);
    assert(c.r2==0x00123456);
    c.r4=0x1000;osVirtualToPhysical_recomp(ram.data(),&c);
    assert((std::int64_t)c.r2==-1);
    c.r4=0x807FFFF0; c.r5=0x807FFFFC;c.r6=5;
    osCreateMesgQueue_recomp(ram.data(),&c);
    assert((std::int64_t)c.r2==-1); // Out of bounds message array
    osInvalDCache_recomp(ram.data(),&c);
    osWritebackDCache_recomp(ram.data(),&c);
    osWritebackDCacheAll_recomp(ram.data(),&c);
    std::puts("PASS: seven real N64Recomp ABI hook symbols; nonblocking FIFO; KSEG0/KSEG1; bounds");
}
