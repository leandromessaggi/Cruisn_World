// PRIVATE integration test. Runs two ROM-derived MIPS protection functions,
// statically recompiled to C. Linux test only: reserve a sparse address view
// to accommodate the N64 SP peripheral windows used by N64Recomp's MEM_W.
// Not an Android runtime and not a full CIC-6106 implementation.
#include "cic6106_seed.hpp"
#include "recomp.h"
#include "funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sys/mman.h>
#include <vector>
extern "C" void func_802AD870(std::uint8_t*,recomp_context*);
extern "C" void func_802AD898(std::uint8_t*,recomp_context*);
namespace {
struct View { std::uint8_t* base; std::size_t length; };
constexpr std::size_t kGuestWindow=0x24002000u;
bool word(void* user,std::uint32_t addr,std::uint32_t value) {
    auto* v=static_cast<View*>(user);
    if(addr<0x80000000u || std::uint64_t(addr)-0x80000000u+4>v->length) return false;
    std::memcpy(v->base+(addr-0x80000000u),&value,4);return true;
}
bool bytes(void* user,std::uint32_t addr,const std::uint8_t* data,std::size_t size) {
    auto* v=static_cast<View*>(user);
    if(addr!=0x90000000u || std::uint64_t(addr)-0x80000000u+size>v->length)return false;
    std::memcpy(v->base+(addr-0x80000000u),data,size);return true;
}
}
int main(int argc,char** argv) {
    if(argc!=2) return 10;
    std::ifstream file(argv[1],std::ios::binary);
    if(!file)return 11;
    const std::vector<std::uint8_t> rom((std::istreambuf_iterator<char>(file)),{});
    void* mapping=mmap(nullptr,kGuestWindow,PROT_READ|PROT_WRITE,
                       MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE,-1,0);
    if(mapping==MAP_FAILED) {std::perror("mmap");return 12;}
    View view{static_cast<std::uint8_t*>(mapping),kGuestWindow};
    const cw::Cic6106Writer writer{&view,word,bytes};
    if(!cw::seed_world_us_cic6106(writer,rom.data(),rom.size()))return 13;
    if(std::memcmp(view.base+0x10000000u,rom.data(),0x1000u))return 14;
    recomp_context ctx{};
    func_802AD870(view.base,&ctx);
    bool imem_pass=ctx.r2==1;
    func_802AD898(view.base,&ctx);
    bool dmem_pass=ctx.r2==1;
    if(!word(&view,0xA4001000u,0))return 15;
    func_802AD870(view.base,&ctx);
    bool imem_reject=ctx.r2==0;
    if(!word(&view,0xA4000000u,0))return 16;
    func_802AD898(view.base,&ctx);
    bool dmem_reject=ctx.r2==0;
    const bool ok=imem_pass && dmem_pass && imem_reject && dmem_reject;
    std::printf("%s CIC-6106 original funcs 0x802AD870 and 0x802AD898: "
        "IMEM_pass=%d DMEM_pass=%d IMEM_reject=%d DMEM_reject=%d "
        "IPL3_copy=4096; CPU_boot_complete=NO\n",ok?"PASS":"FAIL",
        imem_pass,dmem_pass,imem_reject,dmem_reject);
    munmap(mapping,kGuestWindow);
    return ok?0:17;
}
