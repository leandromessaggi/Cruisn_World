# v0.30 — libultra queue call register ABI (ROM-free)

The cooperative guest context scheduler now accepts the MIPS o32 arguments
`a0` (guest queue address), `a1` (message value for send, guest destination
pointer for receive) and `a2` (0 = OS_MESG_NOBLOCK, 1 = OS_MESG_BLOCK).
`invokeQueueCall` routes to `sendGuest` or `recvGuest`. Immediate results
write the sign-extended libultra status to saved MIPS `v0`: 0 on success,
-1 on nonblocking failure or invalid arguments. A blocked sender retains
its continuation PC and updates `v0` only when a receiver opens a slot;
blocked receives still copy the value to guest RDRAM on completion.

The Android VI diagnostic now exercises both calls through MIPS argument
registers, rather than calling `recvGuest` directly. Host regression tests
cover priority wakeup, send backpressure, return values, invalid flags,
invalid destination, and receive/write semantics with synthetic memory.

**Limitations:** This is an entrypoint-neutral call adapter. It does not
resolve or patch original libultra symbol addresses, advance a guest PC,
execute original translated game functions, run N64ModernRuntime, or render
graphics. The translated caller must save its verified continuation PC
before calling this adapter. No ROM, game code, or derived assets are added.
