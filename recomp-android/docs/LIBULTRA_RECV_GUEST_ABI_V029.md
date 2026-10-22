# v0.29 — Guest libultra osRecvMesg destination handoff

This ROM-free ABI bridge extends the existing cooperative context scheduler. A
blocking receive records the optional 32-bit guest OSMesg* destination, and a
VI queue wakeup copies the message into guest RDRAM and writes the sign-extended
libultra result into the saved MIPS v0 register. It preserves the saved guest
continuation PC and other registers. A nonblocking empty receive sets v0=-1;
a null message destination is allowed. Invalid guest destinations are rejected
before queue consumption. The Android JNI diagnostic now exercises the same
path using a synthetic queue after loading a locally verified ROM.

**Scope:** This is guest ABI bookkeeping, not a linked original osRecvMesg
symbol, not a host-stack continuation, and not a running N64ModernRuntime.
No original game code, ROM bytes, or ROM-derived assets are included.
