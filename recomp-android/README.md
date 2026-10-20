# Cruis'n World — reverse engineering and Android static recompilation

**Stage:** real ROM reverse engineering + first MIPS boot behavior reproduced in native Android C++ (ARM64). **NOT** the complete playable game. No N64 emulator is included.

## Real ROM findings (USA)

The supplied `Cruis'n World (USA).zip` contains a 12 MiB byte-swapped ROM (initial 4 bytes `37 80 40 12`). Converting to standard `.z64` big endian yields:

- SHA-256 `5320594232f0c492a672464832425351bd06497c95fb243745e2493bc5df9c2c`
- SHA-1 `6da1a6a2bda687d50e798d80c342948ad1738202`
- Header entry `0x804AD400`, CRC1 `DFE61153`, CRC2 `D76118E6`, game code `NCWE`.
- Code entry loaded by boot `0x802AD400`; BSS clearing loop starts at ROM offset `0x1000`.
- Main MIPS execution `0x802AD450`; BSS of `0x94760` bytes starting at `0x80353380`.
- Additional dynamically loaded code at ROM `0xA7040`, mapped to `0x80050400`; audio RSP microcode at ROM `0x86230`.

The addresses are cross-checked with an existing *Cruis'n World* recompilation [navjack/cruisn-world-recomp](https://github.com/navjack/cruisn-world-recomp), whose upstream README notes that its macOS version boots, loads gameplay code and plays audio, but has not been played through. **These are research findings, not claims of a working Android port.**

## Reproduce the analysis

```
python3 tools/rom_probe.py "/path/to/Cruis'n World (USA).zip" --output reports --llvm-objdump llvm-objdump
python3 -m unittest discover -v -s tests
```

The probe normalizes `.n64`/`.v64`/`.z64`, verifies full hashes, validates entry instructions, reports known main/overlay/audio boundaries, and disassembles the entry with LLVM. No ROM data is distributed.

## Android ARM64 JNI checkpoint

`android/` is a tiny diagnostic Android application to test native code from the *actual* game boot:

1. Select your own ROM using Android's document picker.
2. App normalizes byte ordering and checks SHA-256 before private storage.
3. JNI verifies exact 14 MIPS boot words of this game.
4. Equivalent boot BSS-zero behavior is executed in compiled native ARM64 C++ (`libcw_boot.so`).
5. App displays the decoded entry addresses and explicit future work.

**Important:** the APK is not a game yet; it does not render graphics, play audio, or execute gameplay. This is a verifiable native bootstrap, not a ROM wrapper.

## How to reach a playable port

Use N64Recomp symbol map from the project above (1,536 *candidate* functions, still requiring verification), generate C for code and overlay, integrate N64ModernRuntime (`ultramodern`/`librecomp`), port or implement N64 graphics commands to Android Vulkan/OpenGL ES, connect RSP audio microcode, controls (touch/physical remap), EEPROM and complete copy-protection initialization. Validate graphics, audio, gameplay and save on physical device. This is significant work beyond the initial boot checkpoint.

## Sources & licensing

- https://github.com/navjack/cruisn-world-recomp (GPL-3.0)
- https://github.com/N64Recomp/N64Recomp (static recompilation tool)
- https://github.com/N64Recomp/N64ModernRuntime (libultra runtime)
- https://github.com/ethteck/splat (ROM disassembly)
- https://www.retroreversing.com/N64MipsAssembly (N64 header and MIPS references)

The code here reimplements one verified initialization routine in C++ and supplies public build tooling; it does not redistribute proprietary ROM bytes. Reverse engineering results are licensed GPL-3.0-compatible for integration with the upstream runtime; respective third-party licenses remain applicable. Use your own copy of the original game.
