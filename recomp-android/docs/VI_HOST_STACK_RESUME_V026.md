# v0.26 — Original VI worker resumes after an actual blocking receive

**Scope:** PRIVATE bounded test on Linux with the owner's exact USA ROM. **Not a playable Android build**, not a replacement for N64ModernRuntime, and not a complete N64 thread/context scheduler.

Previously v0.25 stopped `func_802FB708` at `osRecvMesg`, then re-entered it from its beginning to verify that an injected VI retrace message was received. This could not verify continued execution of the original function.

The v0.26 test uses an **opt-in host synchronization gate** (`ViHostWaitGate`) that keeps one native thread and its stack alive while it waits on the guest `OSMesgQueue`. The queue still resides in mapped RDRAM. The host condition variable is not stored in RDRAM. A test controller explicitly inserts one *previously registered* `0x29D` message and notifies the waiting thread. Unregistered message values are rejected; queue-full and FIFO handling remain checked.

The recompiled **original** `func_802FB708` returns from `osRecvMesg`, dispatches the `0x29D` branch, performs `MEM_W(-0x799C, 0x80350000) = 1`, and reaches its next blocking receive **within the same original function invocation**. The original store is checked at guest `0x80348664` and the same native thread's second wait is observed. The test bounds execution by an exception at this second wait; the generated C is built with exception unwinding enabled solely for this private harness.

The precise guest thread argument must be sign-extended as a 32-bit MIPS address when installed in the 64-bit `recomp_context`. The original runtime's guest linked-list at `argument + 0x260` is explicitly initialized to an empty list for this bounded test; therefore the test does **not** demonstrate full downstream VI event handling.

## Private verification

```
python3 recomp-android/tools/verify_private_vi_continuation.py \
  --private /path/to/locally-generated-recompiler-output \
  --rom /path/to/owner-us-cruisn-world.z64 \
  --output vi-v026-private.json
```

The verifier rejects unexpected ROM hashes and generated source fingerprints; it emits fail-closed abort traps for unresolved original game entrypoints, never functional fake replacements. The ROM and all ROM-derived generated C remain private. Public GitHub Actions run ROM-free synthetic host wait/resume, FIFO, registration, DMA and boot-regression tests only.

## Not yet implemented

There is **no scheduler** starting game threads autonomously, no guest MIPS preemption/context switching, no 60Hz VI timer or actual interrupt source, no graphics/RSP/RT64, no game audio or Android runtime linkage, and no playable race. The host gate is for diagnosis only; production should use the N64ModernRuntime thread/event system.
