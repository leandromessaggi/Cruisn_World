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
    // Private integration runs only. ROM bytes are not part of the public test
    // data and are never embedded in a binary or public repository.
    const std::uint8_t* rom=nullptr;
    std::size_t rom_bytes=0;
    unsigned dma_reads=0, dma_completions=0, dma_receives=0, cache_invalidations=0;
    std::uint32_t dma_cart=0,dma_ram=0,dma_length=0,dma_queue=0;
    std::uint32_t message_value=0;
    unsigned queue_pending=0;
    void (*on_dma_received)()=nullptr; // bounded-stop test callback

    ObservedThread threads[3]{};
    unsigned thread_count=0, starts=0, errors=0;
    unsigned pi_managers=0, priority_changes=0, mesg_queues=0;
    unsigned entered_count=0, parked_count=0;
    bool video_black=false, initialized=false;
    // Bounded boot probe only. Additional queues used by the original VI setup.
    struct QueueObservation { std::uint32_t address=0, messages=0, capacity=0; };
    QueueObservation queues[3]{};
    unsigned vi_managers=0, vi_black_calls=0;
    std::uint32_t vi_priority=0;
    // Original-code VI initialization. These record *requested* operations;
    // they do not drive scanout, interrupts or rendering on Android.
    std::uint32_t vi_mode=0, vi_special_mask=0, vi_retrace_queue=0;
    std::uint32_t vi_retrace_message=0, vi_retrace_interval=0;
    unsigned vi_mode_calls=0, vi_special_calls=0;
    struct EventObservation { std::uint32_t event=0, queue=0, message=0; };
    EventObservation vi_events[3]{};
    unsigned vi_event_count=0, vi_retrace_registrations=0;
    // Explicit VI retrace injection for the bounded *test*; no clock or native
    // interrupt exists. Metadata is kept in guest OSMesgQueue, not a host FIFO.
    unsigned vi_retrace_ticks=0, vi_retrace_received=0, vi_retrace_waits=0;
    unsigned vi_retrace_drops=0;
    void (*on_vi_worker_wait)()=nullptr; // stop at first empty, blocking recv
    void (*on_vi_worker_received)()=nullptr; // stop after first successful recv
    void (*on_vi_worker_start)()=nullptr; // stop *after* third guest osStartThread
    void (*on_vi_black)()=nullptr; // harness stops immediately after the second call
    void (*on_first_queue)()=nullptr; // harness-only bounded stop; never production scheduler
    std::uint32_t manager_queue=0, manager_msgs=0, manager_capacity=0;
    std::uint32_t queue_addr=0, queue_msgs=0, queue_capacity=0;
};
void bind_boot_multithread_probe(BootMultiTrace*) noexcept;
ObservedThread* find_thread(BootMultiTrace&, std::uint32_t guest_addr) noexcept;
// Deliver one deterministic test VI retrace event to the registered guest queue.
// Returns false without corrupting RAM when full or not registered.
bool inject_vi_retrace(BootMultiTrace&) noexcept;
// Prepare only register inputs; the test harness decides which original
// recompiled function to invoke. Never advances unbounded guest code here.
}
