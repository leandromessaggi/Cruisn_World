# Android native boot checkpoint

JNI library compiles to Android ARM64 (`libcw_boot.so`) and tests real US ROM
initialization data. It does **not** emulate MIPS or render game frames.

The actual portable part is the equivalent C++ behavior of the first MIPS
boot BSS-clear loop, verified against the user's canonical ROM.

Open in Android Studio, or run Gradle `assembleDebug` with Android SDK + NDK.
The on-device file picker accepts `.n64`, `.v64`, `.z64` and a `.zip` containing the ROM,
checks the full SHA-256 and stores the normalized file in app-private storage.

Do not call this a playable port. Runtime/renderer integration is missing.
