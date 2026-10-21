// PRIVATE integration harness. Compiles against the user's ROM-derived funcs_0.c.
// Executes original guest CPU routines with a bounded real ROM DMA.
// Stops at receipt of the first PI DMA completion, before SP protection tests.
// The scheduling, security boot seed and device I/O remain simulated/omitted.
#include "recomp.h"
#include "funcs.h"
#include "boot_multithread_probe.hpp"
#include "cic6106_seed.hpp"
#include <sys/mman.h>
#include <cstring>
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
extern "C" void func_802AD450(uint8_t*,recomp_context*);
extern "C" void func_802AD4C0(uint8_t*,recomp_context*);
extern "C" void func_802AD8C0(uint8_t*,recomp_context*);
namespace {
static std::jmp_buf boundary;
static uint8_t* ram=nullptr;
static unsigned state=0, pauses[3]{},seeds=0,dispatches=0,bad_dispatches=0;
static std::uint32_t seed_sp=0;
static unsigned reached_next_call=0;
struct Window {uint8_t* memory;};
bool seed_word(void* u,std::uint32_t addr,std::uint32_t value) {
    auto* m=static_cast<Window*>(u);
    if(addr<0x80000000u || static_cast<std::uint64_t>(addr)-0x80000000u+4>0x24002000u)return false;
    std::memcpy(m->memory+(addr-0x80000000u),&value,4);return true;
}
bool seed_bytes(void* u,std::uint32_t addr,const uint8_t* bytes,std::size_t n) {
    auto* m=static_cast<Window*>(u);
    if(addr!=0x90000000u || n!=0x1000u)return false;
    std::memcpy(m->memory+0x10000000u,bytes,n);return true;
}
}
extern "C" recomp_func_t* get_function(int32_t target) {
    ++dispatches;
    if(static_cast<std::uint32_t>(target)==0x802AD450u) return func_802AD450;
    ++bad_dispatches;
    return nullptr;
}
extern "C" void cruisn_world_seed_boot_state(uint8_t* rdram,recomp_context* ctx) {
    if(rdram!=ram || !ctx) ++bad_dispatches;
    ++seeds;
    seed_sp=static_cast<std::uint32_t>(ctx->r29);
    // Deliberate bounded test: DOES NOT synthesize CIC, IPL3, SP protection.
}
extern "C" void pause_self(uint8_t* rdram) {
    if(rdram!=ram || state>2) {++bad_dispatches; std::longjmp(boundary,7);}
    ++pauses[state];
    std::longjmp(boundary,1); // Stop tight guest loop, no host scheduling here.
}
extern "C" void func_802FB3B0(uint8_t* rdram,recomp_context* ctx) {
    if(rdram!=ram || !ctx || state!=2) {++bad_dispatches; std::longjmp(boundary,7);}
    ++reached_next_call;
    std::longjmp(boundary,5); // Explicit stop at next unintegrated game routine.
}
int main(int argc,char** argv) {
    if (argc!=2) { std::fprintf(stderr,"Need private, normalized USA ROM\n");return 10; }
    std::ifstream file(argv[1],std::ios::binary);
    if(!file) { std::fprintf(stderr,"ROM not found\n");return 11; }
    std::vector<std::uint8_t> rom((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    if(rom.size()!=12582912u || rom[0]!=0x80 || rom[1]!=0x37 || rom[2]!=0x12 || rom[3]!=0x40)
        return 12;
    // Linux-only sparse view for guest SP DMEM/IMEM and cartridge windows.
    // No 608 MiB allocation is physically committed; never ship this to Android.
    constexpr std::size_t reserved=0x24002000u;
    void* v=mmap(nullptr,reserved,PROT_READ|PROT_WRITE,
                 MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE,-1,0);
    if(v==MAP_FAILED)return 13;
    uint8_t* memory=static_cast<uint8_t*>(v);
    std::memset(memory,0xA5,8u*1024u*1024u);
    Window w{memory};
    if(!cw::seed_world_us_cic6106({&w,seed_word,seed_bytes},rom.data(),rom.size()))return 14;
    recomp_context ctx{};
    cw::BootMultiTrace boot{};
    boot.rdram=memory; boot.rdram_bytes=8u*1024*1024; boot.rom=rom.data();boot.rom_bytes=rom.size();
    ram=memory;
    cw::bind_boot_multithread_probe(&boot);
    const int root_boundary=setjmp(boundary);
    if(root_boundary==0) recomp_entrypoint(memory,&ctx);
    bool root_ok = root_boundary==1 && pauses[0]==1 && boot.thread_count==1 && boot.starts==1 &&
        boot.threads[0].entry==0x802AD4C0u && boot.threads[0].stack==0x8035B130u &&
        seed_sp==0x80150400u && boot.errors==0;
    if(!root_ok) { std::fprintf(stderr,"FAIL root: b=%d errs=%u threads=%u starts=%u\n",root_boundary,boot.errors,boot.thread_count,boot.starts);return 1;}
    // N64 libultra sets guest entry SP to stack-0x10; osCreateThread stored argument.
    state=1;
    recomp_context thread1{};
    thread1.r29=boot.threads[0].stack-0x10u;
    thread1.r4=boot.threads[0].argument;
    boot.threads[0].entered=true;
    ++boot.entered_count;
    const int child_boundary=setjmp(boundary);
    if(child_boundary==0) func_802AD4C0(memory,&thread1);
    bool first_ok=child_boundary==1 && pauses[1]==1 && boot.pi_managers==1 &&
        boot.manager_capacity==8 && boot.thread_count==2 && boot.starts==2 &&
        boot.threads[1].address==0x8035B130u && boot.threads[1].id==3 &&
        boot.threads[1].entry==0x802AD8C0u && boot.threads[1].stack==0x8035D2E0u &&
        boot.threads[1].priority==10 && boot.priority_changes==1 && boot.threads[0].priority==0 &&
        boot.errors==0;
    if(!first_ok){std::fprintf(stderr,"FAIL child: b=%d errs=%u threads=%u starts=%u pri=%u pi=%u\n",child_boundary,boot.errors,boot.thread_count,boot.starts,boot.threads[0].priority,boot.pi_managers);return 2;}
    state=2;
    recomp_context thread3{};
    thread3.r29=boot.threads[1].stack-0x10u;
    thread3.r4=boot.threads[1].argument;
    boot.threads[1].entered=true;
    ++boot.entered_count;
    boot.on_dma_received=nullptr;
    const int third_boundary=setjmp(boundary);
    if(third_boundary==0) func_802AD8C0(memory,&thread3);
    boot.on_dma_received=nullptr;
    bool actual_rom_bytes=true;
    for(std::uint32_t i=0;i<0xC0u;++i) {
        const std::uint32_t guest_addr=(0x003E7AE0u+i)^3u;
        if(memory[guest_addr]!=rom[0xA6F80u+i]) {actual_rom_bytes=false;break;}
    }
    std::uint32_t imem_result=0,dmem_result=0;
    std::memcpy(&imem_result,memory+0x333478u,4);
    std::memcpy(&dmem_result,memory+0x33347Cu,4);
    const bool ok=third_boundary==5 && reached_next_call==1 &&
        imem_result==1 && dmem_result==1 && actual_rom_bytes &&
        boot.dma_reads==1 && boot.dma_completions==1 && boot.dma_receives==1 &&
        boot.dma_cart==0xA6F80u && boot.dma_ram==0x803E7AE0u &&
        boot.dma_length==0xC0u && boot.dma_queue==0x80360760u &&
        boot.cache_invalidations==1 && boot.queue_pending==0 &&
        boot.mesg_queues==1 && boot.queue_capacity==1 &&
        boot.queue_addr==0x80360760u && boot.queue_msgs==0x803607F0u &&
        boot.entered_count==2 && dispatches==1 && bad_dispatches==0 && seeds==1 && boot.errors==0;
    std::printf("%s: boot DMA and CIC checks before 0x802FB3B0; bytes=%u; "
        "IMEM=%u DMEM=%u; COPY_MATCH=%s; scheduling=SEQUENTIAL; gameplay=NO\n",
        ok?"PASS":"FAIL",boot.dma_length,imem_result,dmem_result,actual_rom_bytes?"YES":"NO");
    cw::bind_boot_multithread_probe(nullptr);
    munmap(v,reserved);
    return ok?0:3;
}
