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
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <cstdlib>
extern "C" void func_802AD450(uint8_t*,recomp_context*);
extern "C" void func_802AD4C0(uint8_t*,recomp_context*);
extern "C" void func_802AD8C0(uint8_t*,recomp_context*);
extern "C" void func_802FB708(uint8_t*,recomp_context*);
namespace {
static std::jmp_buf boundary;
static uint8_t* ram=nullptr;
static unsigned state=0, pauses[3]{},seeds=0,dispatches=0,bad_dispatches=0;
static std::uint32_t seed_sp=0;
static unsigned worker_stop_count=0,worker_wait_stops=0,worker_recv_stops=0;
struct BoundedEndAfterResume {};
static thread_local unsigned host_wait_calls=0;
static void stop_after_second_native_wait() {
    // C recompilation continues in the SAME invocation after osRecvMesg.
    // Its next blocking receive is a safe bounded-stop point.
    if (++host_wait_calls==2) throw BoundedEndAfterResume{};
}
static void stop_worker_wait() {++worker_wait_stops;std::longjmp(boundary,9);}
static void stop_worker_recv() {++worker_recv_stops;std::longjmp(boundary,10);}
static void stop_on_worker_start() {++worker_stop_count;std::longjmp(boundary,8);}
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
    // The retail N64 boot sets osTvType at 0x80000300. USA NTSC = 1.
    // Earlier bounded probes left it 0xA5A5A5A5 and skipped mode setup.
    if(!seed_word(&w,0x80000300u,1u))return 15;
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
    boot.on_vi_black=nullptr;
    boot.on_vi_worker_start=stop_on_worker_start;
    const int third_boundary=setjmp(boundary);
    if(third_boundary==0) func_802AD8C0(memory,&thread3);
    boot.on_dma_received=nullptr;
    boot.on_vi_black=nullptr;
    boot.on_vi_worker_start=nullptr;
    bool actual_rom_bytes=true;
    for(std::uint32_t i=0;i<0xC0u;++i) {
        const std::uint32_t guest_addr=(0x003E7AE0u+i)^3u;
        if(memory[guest_addr]!=rom[0xA6F80u+i]) {actual_rom_bytes=false;break;}
    }
    std::uint32_t imem_result=0,dmem_result=0;
    std::memcpy(&imem_result,memory+0x333478u,4);
    std::memcpy(&dmem_result,memory+0x33347Cu,4);
    bool vi_queues_valid=true;
    const std::uint32_t expected_q[3]={0x80360760u,0x8035E4D0u,0x8035E508u};
    const std::uint32_t expected_m[3]={0x803607F0u,0x8035E4E8u,0x8035E520u};
    for(unsigned i=0;i<3;++i) {
        const auto& q=boot.queues[i];
        const auto capacity=(i==0)?1u:8u;
        if(q.address!=expected_q[i] || q.messages!=expected_m[i] || q.capacity!=capacity)
            vi_queues_valid=false;
        auto rd=[&](std::uint32_t addr) {std::uint32_t value=0;std::memcpy(&value,memory+(addr&0x1FFFFFFFu),4);return value;};
        if(rd(expected_q[i]+8)!=0 || rd(expected_q[i]+12)!=0 ||
           rd(expected_q[i]+16)!=capacity || rd(expected_q[i]+20)!=expected_m[i])
            vi_queues_valid=false;
    }
    const bool expected_vi=(boot.vi_mode_calls==1 && boot.vi_mode==0x8034C3D0u &&
        boot.vi_special_calls==1 && boot.vi_special_mask==0x52u &&
        boot.vi_event_count==3 && boot.vi_retrace_registrations==1 &&
        boot.vi_retrace_queue==0x8035E4D0u && boot.vi_retrace_message==0x29Au &&
        boot.vi_retrace_interval==1);
    bool event_details=true;
    const std::uint32_t ids[3]={4,9,14},values[3]={0x29B,0x29C,0x29D};
    for(unsigned i=0;i<3;++i) if(boot.vi_events[i].event!=ids[i] ||
        boot.vi_events[i].queue!=0x8035E4D0u ||
        boot.vi_events[i].message!=values[i]) event_details=false;
    const bool worker=(boot.thread_count==3 && boot.starts==3 &&
        boot.threads[2].id==6 && boot.threads[2].address==0x8035E540u &&
        boot.threads[2].entry==0x802FB708u && boot.threads[2].priority==40u);
    // Advance one actual original VI-worker call to the first blocking recv.
    // This stops at the libultra boundary, not a real host thread wait.
    boot.on_vi_worker_wait=stop_worker_wait;
    recomp_context worker_ctx{};
    worker_ctx.r29=boot.threads[2].stack-0x10u;
    worker_ctx.r4=boot.threads[2].argument;
    const int worker_wait_boundary=setjmp(boundary);
    if(worker_wait_boundary==0) func_802FB708(memory,&worker_ctx);
    boot.on_vi_worker_wait=nullptr;
    const bool wait_ok=worker_wait_boundary==9 && worker_wait_stops==1 &&
        boot.vi_retrace_waits==1 && boot.vi_retrace_received==0 &&
        boot.errors==0;
    // Trigger a *manual* deterministic VI tick, not a timer or graphics task.
    const bool delivered=cw::inject_vi_retrace(boot);
    std::uint32_t pending_vi=0;
    std::memcpy(&pending_vi,memory+(boot.queues[1].address&0x1FFFFFFFu)+8,4);
    const bool queue_ok=delivered && pending_vi==1 && boot.vi_retrace_ticks==1;
    // Re-entry at the beginning is deliberately a NEW test invocation,
    // not preservation or resumption of the original MIPS thread context.
    boot.on_vi_worker_received=stop_worker_recv;
    recomp_context recv_ctx{};
    recv_ctx.r29=boot.threads[2].stack-0x10u;
    recv_ctx.r4=boot.threads[2].argument;
    const int recv_boundary=setjmp(boundary);
    if(recv_boundary==0) func_802FB708(memory,&recv_ctx);
    boot.on_vi_worker_received=nullptr;
    std::uint32_t pending_after=~0u,seen_message=0;
    std::memcpy(&pending_after,memory+(boot.queues[1].address&0x1FFFFFFFu)+8,4);
    const auto slot_addr=static_cast<std::uint32_t>(recv_ctx.r29+0x54u);
    std::memcpy(&seen_message,memory+(slot_addr&0x1FFFFFFFu),4);
    const bool receive_ok=recv_boundary==10 && worker_recv_stops==1 &&
        boot.vi_retrace_received==1 && pending_after==0 && seen_message==0x29Au;
    // Experimental host wait: unlike the previous two bounded probes, this
    // invokes the original function ONCE on a persistent native thread.
    // A registered 0x29D event goes through osRecvMesg and original MIPS code
    // writes 1 at 0x80348664 BEFORE attempting the next receive.
    cw::ViHostWaitGate host_gate{};
    boot.vi_host_wait=&host_gate;
    boot.on_vi_worker_wait=stop_after_second_native_wait;
    const std::uint32_t worker_arg=boot.threads[2].argument;
    const std::uint32_t extra=(worker_arg & 0x1FFFFFFFu)+0x260u;
    if(extra+4u>8u*1024*1024u) return 16;
    const std::uint32_t zero=0;
    // Synthetic game linked list: no subscribers in this bounded proof.
    std::memcpy(memory+extra,&zero,4);
    std::memcpy(memory+0x348664,&zero,4);
    recomp_context same_ctx{};
    same_ctx.r29=boot.threads[2].stack-0x10u;
    same_ctx.r4=static_cast<std::int64_t>(static_cast<std::int32_t>(worker_arg));
    const unsigned previous_waits=boot.vi_retrace_waits;
    std::atomic<int> native_outcome{0};
    std::thread native_worker([&] {
        cw::bind_boot_multithread_probe(&boot);
        try {
            func_802FB708(memory,&same_ctx);
            native_outcome=2; // should not return: infinite guest worker loop
        } catch(const BoundedEndAfterResume&) {
            native_outcome=1;
        } catch(...) {
            native_outcome=-1;
        }
        cw::bind_boot_multithread_probe(nullptr);
    });
    bool first_same_stack_wait=false;
    {
        std::unique_lock<std::mutex> lk(host_gate.mutex);
        first_same_stack_wait=host_gate.changed.wait_for(
            lk,std::chrono::seconds(3),[&]{return boot.vi_retrace_waits>=previous_waits+1;});
    }
    const bool injected_event=first_same_stack_wait &&
        cw::inject_registered_vi_event(boot,0x29Du);
    bool second_same_stack_wait=false;
    if(injected_event) {
        std::unique_lock<std::mutex> lk(host_gate.mutex);
        second_same_stack_wait=host_gate.changed.wait_for(
            lk,std::chrono::seconds(3),[&]{return boot.vi_retrace_waits>=previous_waits+2;});
    }
    if(!second_same_stack_wait) {
        std::fprintf(stderr,"FAIL native wait timeout or missing second receive\n");
        std::abort();
    }
    native_worker.join();
    boot.vi_host_wait=nullptr;
    boot.on_vi_worker_wait=nullptr;
    std::uint32_t post_recv_marker=0;
    std::memcpy(&post_recv_marker,memory+0x348664u,4);
    const bool actually_resumed=injected_event && second_same_stack_wait &&
        native_outcome==1 && post_recv_marker==1u &&
        boot.vi_event_injections==1 && boot.errors==0 &&
        boot.vi_retrace_received==2;
    std::printf("NATIVE_TEST: single_original_function_invocation=%s post_recv_guest_store=%08X waits=%u event=0x29D\n",
        actually_resumed?"YES":"NO",post_recv_marker,boot.vi_retrace_waits);
    const bool ok=actually_resumed && wait_ok && queue_ok && receive_ok &&
        third_boundary==8 && worker_stop_count==1 && expected_vi && event_details && worker &&
        boot.vi_managers==1 && boot.vi_priority==0xFEu && boot.vi_black_calls==3 && vi_queues_valid &&
        imem_result==1 && dmem_result==1 && actual_rom_bytes &&
        boot.dma_reads==1 && boot.dma_completions==1 && boot.dma_receives==1 &&
        boot.dma_cart==0xA6F80u && boot.dma_ram==0x803E7AE0u &&
        boot.dma_length==0xC0u && boot.dma_queue==0x80360760u &&
        boot.cache_invalidations==1 && boot.queue_pending==0 &&
        boot.mesg_queues==3 && boot.queue_capacity==1 &&
        boot.queue_addr==0x80360760u && boot.queue_msgs==0x803607F0u &&
        boot.entered_count==2 && dispatches==1 && bad_dispatches==0 && seeds==1 && boot.errors==0;
    std::printf("%s: boot DMA & CIC checks, original VI init queued; bytes=%u; "
        "IMEM=%u DMEM=%u; VI_MODE=%08X; EVENT_REG=%u; RETRACE=%u; "
        "WORKER=%08X; WAIT=%s; TICK=%s; RECEIVED=%s; ERRORS=%u; BLACK=%u; COPY_MATCH=%s; "
        "scheduling=SEQUENTIAL; host_stack_resumed=%s; runtime_context_switch=NO; gameplay=NO\n",
        ok?"PASS":"FAIL",boot.dma_length,imem_result,dmem_result,
        boot.vi_mode,boot.vi_event_count,boot.vi_retrace_registrations,
        boot.threads[2].entry,wait_ok?"YES":"NO",queue_ok?"YES":"NO",
        receive_ok?"YES":"NO",boot.errors,boot.vi_black_calls,actual_rom_bytes?"YES":"NO",actually_resumed?"YES":"NO");
    cw::bind_boot_multithread_probe(nullptr);
    munmap(v,reserved);
    return ok?0:3;
}
