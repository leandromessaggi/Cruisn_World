// PRIVATE integration harness. Compiles against the user's ROM-derived funcs_0.c.
// Executes 3 original guest CPU routines sequentially on the host, bounded at
// guest root/child loops and the grandchild's FIRST message-queue creation.
// The scheduling, security boot seed and device I/O remain simulated/omitted.
#include "recomp.h"
#include "funcs.h"
#include "boot_multithread_probe.hpp"
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <vector>
extern "C" void func_802AD450(uint8_t*,recomp_context*);
extern "C" void func_802AD4C0(uint8_t*,recomp_context*);
extern "C" void func_802AD8C0(uint8_t*,recomp_context*);
namespace {
static std::jmp_buf boundary;
static uint8_t* ram=nullptr;
static unsigned state=0, pauses[3]{},seeds=0,dispatches=0,bad_dispatches=0;
static std::uint32_t seed_sp=0;
void stop_at_queue() { if(state==2)std::longjmp(boundary,3); }
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
int main() {
    std::vector<uint8_t> memory(8u*1024*1024,0xA5);
    recomp_context ctx{};
    cw::BootMultiTrace boot{};
    boot.rdram=memory.data(); boot.rdram_bytes=memory.size();
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
    boot.on_first_queue=stop_at_queue;
    const int third_boundary=setjmp(boundary);
    if(third_boundary==0) func_802AD8C0(memory.data(),&thread3);
    boot.on_first_queue=nullptr;
    const bool ok=third_boundary==3 && boot.mesg_queues==1 && boot.queue_capacity==1 &&
        boot.queue_addr==0x80360760u && boot.queue_msgs==0x803607F0u &&
        boot.entered_count==2 && dispatches==1 && bad_dispatches==0 && seeds==1 && boot.errors==0;
    std::printf("%s: original entry->main->thread1->thread3; child entry=%08x PI manager=%u; grandchild entry=%08x first queue=%08x; protection=MOCKED; scheduling=SEQUENTIAL; gameplay=NO\n",
        ok?"PASS":"FAIL",boot.threads[0].entry,boot.pi_managers,boot.threads[1].entry,boot.queue_addr);
    cw::bind_boot_multithread_probe(nullptr);
    return ok?0:3;
}
