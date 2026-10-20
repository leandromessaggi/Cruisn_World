# v0.13 — guest-call continuation recompilada

O APK inclui um teste de PC+JAL que avança por três blocos estáticos originais. Não é jogável.

# Android native boot checkpoint

JNI library compiles to Android ARM64 (`libcw_boot.so`) and tests real US ROM
initialization data. It does **not** emulate MIPS or render game frames.

The actual portable part is the equivalent C++ behavior of the first MIPS
boot BSS-clear loop, verified against the user's canonical ROM.

Open in Android Studio, or run Gradle `assembleDebug` with Android SDK + NDK.
The on-device file picker accepts `.n64`, `.v64`, `.z64` and a `.zip` containing the ROM,
checks the full SHA-256 and stores the normalized file in app-private storage.

Do not call this a playable port. Runtime/renderer integration is missing.

## v0.7 PI DMA

O diagnóstico também valida cópia assíncrona controlada de quatro bytes originais do overlay (ROM cartucho -> RDRAM), disparo após um VI e publicação da conclusão em fila. Não há gameplay nesta etapa.


### Marco v0.8: overlay loader nativo em alto nível

`NativeOverlayLoad` verifica 33 instruções da rotina MIPS `0x802E0098`, reserva o destino fixo conhecido `0x80050400`, grava o ponteiro em `0x80343C2C` e recopia ROM `0xA7040–0xC12A0` em sete blocos pelo `GuestPiDma` com eventos VI. O alocador original, as threads e as chamadas reais à libultra ainda **não** estão recompilados. O APK ainda não apresenta jogabilidade.


## v0.9 guest OSMesgQueue

`recomp_os_queue.cpp/.hpp` implements six-word big-endian libultra `OSMesgQueue` structures in guest RAM with bounded nonblocking FIFO send/receive. `GuestPiOsBridge` hands PI DMA completion to a guest queue at VI, retaining the notification if the queue is full. JNI verifies a real-ROM DMA completion in guest memory. Blocking queue operations and native libultra function dispatch remain unimplemented.
