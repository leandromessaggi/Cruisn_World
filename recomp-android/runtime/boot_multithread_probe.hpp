#pragma once
// BOUNDED ORIGINAL-CODE PROBE, not a runnable libultra scheduler.
// No original ROM bytes or recompilation output belong in this header.
#include <cstddef>
#include <cstdint>
namespace cw {
struct ObservedThread {
    std::uint32_t address=0, id=0, entry=0, argument=0, stack=0, priority=0;
    bool started=false, entered=false;
};
struct BootMultiTrace {
    std::uint8_t* rdram=nullptr;
    std::size_t rdram_bytes=0;
    ObservedThread threads[3]{};
    unsigned thread_count=0, starts=0, errors=0;
    unsigned pi_managers=0, priority_changes=0, mesg_queues=0;
    unsigned entered_count=0, parked_count=0;
    bool video_black=false, initialized=false;
    void (*on_first_queue)()=nullptr; // harness-only bounded stop; never production scheduler
    std::uint32_t manager_queue=0, manager_msgs=0, manager_capacity=0;
    std::uint32_t queue_addr=0, queue_msgs=0, queue_capacity=0;
};
void bind_boot_multithread_probe(BootMultiTrace*) noexcept;
ObservedThread* find_thread(BootMultiTrace&, std::uint32_t guest_addr) noexcept;
// Prepare only register inputs; the test harness decides which original
// recompiled function to invoke. Never advances unbounded guest code here.
}
