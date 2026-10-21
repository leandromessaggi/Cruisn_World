// PRIVATE integration harness. Compiles against the user's ROM-derived funcs_0.c.
// Executes original guest CPU routines with a bounded real ROM DMA.
// Stops at receipt of the first PI DMA completion, before SP protection tests.
// The scheduling, security boot seed and device I/O remain simulated/omitted.
#include "recomp.h"
#include "funcs.h"
#include "boot_multithread_probe.hpp"
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
void stop_after_dma() { if(state==2)std::longjmp(boundary,4); }
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
int main(int argc,char** argv) {
    if (argc!=2) { std::fprintf(stderr,"Need private, normalized USA ROM\n");return 10; }
    std::ifstream file(argv[1],std::ios::binary);
    if(!file) { std::fprintf(stderr,"ROM not found\n");return 11; }
    std::vector<std::uint8_t> rom((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    if(rom.size()!=12582912u || rom[0]!=0x80 || rom[1]!=0x37 || rom[2]!=0x12 || rom[3]!=0x40)
        return 12;
    std::vector<uint8_t> memory(8u*1024*1024,0xA5);
    recomp_context ctx{};
    cw::BootMultiTrace boot{};
    boot.rdram=memory.data(); boot.rdram_bytes=memory.size(); boot.rom=rom.data();boot.rom_bytes=rom.size();
    ram=memory.data();
    cw::bind_boot_multithread_probe(&boot);
    const int root_boundary=setjmp(boundary);
    if(root_boundary==0) recomp_entrypoint(memory.data(),&ctx);
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
    if(child_boundary==0) func_802AD4C0(memory.data(),&thread1);
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
    boot.on_dma_received=stop_after_dma;
    const int third_boundary=setjmp(boundary);
    if(third_boundary==0) func_802AD8C0(memory.data(),&thread3);
    boot.on_dma_received=nullptr;
    bool actual_rom_bytes=true;
    for(std::uint32_t i=0;i<0xC0u;++i) {
        const std::uint32_t guest_addr=(0x003E7AE0u+i)^3u;
        if(memory[guest_addr]!=rom[0xA6F80u+i]) {actual_rom_bytes=false;break;}
    }
    const bool ok=third_boundary==4 && actual_rom_bytes &&
        boot.dma_reads==1 && boot.dma_completions==1 && boot.dma_receives==1 &&
        boot.dma_cart==0xA6F80u && boot.dma_ram==0x803E7AE0u &&
        boot.dma_length==0xC0u && boot.dma_queue==0x80360760u &&
        boot.cache_invalidations==1 && boot.queue_pending==0 &&
        boot.mesg_queues==1 && boot.queue_capacity==1 &&
        boot.queue_addr==0x80360760u && boot.queue_msgs==0x803607F0u &&
        boot.entered_count==2 && dispatches==1 && bad_dispatches==0 && seeds==1 && boot.errors==0;
    std::printf("%s: original code boot->first PI DMA: cart=%08X ram=%08X bytes=%u queue=%08X completions=%u; ROM_BYTES_MATCH=%s; protection=MOCKED; scheduling=SEQUENTIAL; gameplay=NO\n",
        ok?"PASS":"FAIL",boot.dma_cart,boot.dma_ram,boot.dma_length,boot.dma_queue,boot.dma_receives,actual_rom_bytes?"YES":"NO");
    cw::bind_boot_multithread_probe(nullptr);
    return ok?0:3;
}
