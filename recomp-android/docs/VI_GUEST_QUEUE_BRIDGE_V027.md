# v0.27 — Android VI retrace -> guest queue -> priority wakeup

This adds a concrete connection between the Android-native `GuestClock`,
`GuestOsMesgQueue` (in guest RDRAM), and `GuestCpuContextScheduler`.
`GuestViEventBridge` advances the VI clock on explicit ticks, honors a
registered retrace interval, posts a message to the guest queue, and wakes a
higher-priority waiting guest context through the existing queue waiter.

Full queues drop nonblocking retrace notifications without overwriting unread
messages. Invalid queue metadata is reported separately. Failed registration
preserves the previous valid registration; KSEG1 aliases work.

## Verification

`tests/test_guest_vi_events.cpp` uses a synthetic cartridge and checks
interval=2, priority wakeup, saved PC/GPR, full queue, invalid queue, and
KSEG1 alias. A private Linux test also loaded the user's verified USA ROM
and passed the same Android-native queue/wakeup path, **without running the
original game worker**. No proprietary ROM or generated code is committed.

`recomp_vi_jni.cpp` exports an opt-in diagnostic JNI function and is compiled
into the Android library. The existing Activity does **not** call this function
yet; the Android build is not proof that it runs on a physical device.

## Remaining blockers

The tick is manual, not a 60Hz Android VI interrupt loop. This does not
connect the original game's `osViSetEvent_recomp` or `osRecvMesg_recomp`
calls to Android's production scheduler. The original `0x802FB708` worker
does not execute in the Android APK. There is no N64ModernRuntime context
switch, RT64/Vulkan, audio integration, or playable race.

Next: connect original libultra hook entrypoints to a real scheduler and
VI source, preserving the blocked native stack/continuation safely.
