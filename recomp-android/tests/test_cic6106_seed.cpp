#include "cic6106_seed.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
struct Sink {
    struct Word {std::uint32_t at,value;};
    std::array<Word,8> words{};
    unsigned count=0;
    std::array<std::uint8_t,4096> ipl3{};
    bool got_ipl3=false;
    unsigned fail_at=100;
};
static bool write_word(void* u,std::uint32_t a,std::uint32_t v) {
    auto& s=*static_cast<Sink*>(u);
    if (s.count == s.fail_at || s.count>=s.words.size())return false;
    s.words[s.count++]={a,v};return true;
}
static bool write_bytes(void* u,std::uint32_t a,const std::uint8_t* p,std::size_t n) {
    auto& s=*static_cast<Sink*>(u);
    if (s.fail_at==8 || a!=0x90000000u || n!=s.ipl3.size()) return false;
    std::memcpy(s.ipl3.data(),p,n);s.got_ipl3=true;return true;
}
int main() {
    std::vector<std::uint8_t> rom(12u*1024u*1024u);
    rom[0]=0x80;rom[1]=0x37;rom[2]=0x12;rom[3]=0x40;
    const char* id="NCWE";for(unsigned i=0;i<4;i++)rom[0x3B+i]=id[i];
    constexpr std::array<std::uint8_t,8> sums{{0xDF,0xE6,0x11,0x53,0xD7,0x61,0x18,0xE6}};
    for(unsigned i=0;i<8;i++)rom[0x10+i]=sums[i];
    rom[0xFFC]=0xB7;
    Sink s;cw::Cic6106Writer w{&s,write_word,write_bytes};
    assert(cw::seed_world_us_cic6106(w,rom.data(),rom.size()));
    assert(s.count==8 && s.got_ipl3 && s.ipl3[0xFFC]==0xB7);
    assert(s.words[0].at==0x80000164u && s.words[0].value==0x01EC6021u);
    assert(s.words[6].at==0xA4000000u && s.words[6].value==0xFFFFFFFFu);
    assert(s.words[7].at==0xA4001000u && s.words[7].value==0x17D8u);
    rom[0x10]=0;Sink bad;w.user=&bad;
    assert(!cw::seed_world_us_cic6106(w,rom.data(),rom.size()) && bad.count==0);
    rom[0x10]=sums[0];bad.fail_at=3;
    assert(!cw::seed_world_us_cic6106(w,rom.data(),rom.size()) && bad.count==3);
    bad={};bad.fail_at=8;
    assert(!cw::seed_world_us_cic6106(w,rom.data(),rom.size()) && bad.count==8 && !bad.got_ipl3);
    assert(!cw::seed_world_us_cic6106({},rom.data(),rom.size()));
    assert(!cw::seed_world_us_cic6106({&s,write_word,write_bytes},rom.data(),1000));
    std::puts("PASS: CIC-6106 boot state, SP DMEM/IMEM, IPL3 and fail-closed paths");
}
