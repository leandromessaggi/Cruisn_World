# Marco v0.15 — código recompilado para Android ARM64

**Objetivo:** transformar os 30 arquivos C (1.473 funções) gerados na v0.14 pelo N64Recomp em objetos Android ARM64, preservando-os fora do repositório público. O resultado é uma **biblioteca estática**, não um jogo nem um APK.

## Usar com os fontes privados da ROM

Com um Android NDK instalado no Linux, execute:

```bash
export ANDROID_NDK_HOME="$HOME/Android/Sdk/ndk/27.2.12479018"
python3 recomp-android/tools/build_android_cpu.py \
  --input "CruisnWorld_Recomp_C_RSP_v0_14_Privado.zip" \
  --ndk "$ANDROID_NDK_HOME" \
  --output private-arm64 \
  --api 28 --jobs 4
```

O script valida cada objeto com `llvm-readelf` e gera `libcw_recompiled_cpu_arm64.a`, `report.json` e `unresolved_symbols.txt`. Não faz upload da ROM nem de nenhum artefato derivado. O script requer os diretórios `cpu/` e `include/` gerados pela recompilação.

## CI

O workflow **Validate Android ARM64 recompilation (no ROM)** exercita o compilador do Android NDK com apenas duas funções sintéticas e verifica que a biblioteca resultante contém objetos ARM64 válidos. CI verde **não prova** que os 1.473 corpos compilam com o NDK, nem que o runtime os executa.

## Bloqueios restantes

1. Cross-compilar os 30 arquivos reais num ambiente com Android NDK e examinar o relatório de símbolos.
2. Portar e vincular N64ModernRuntime (libultra, threads, DMA, VI, SP, EEPROM), incluindo os hooks de inicialização e proteção de Cruis'n World.
3. Integrar o código RSP de áudio, RT64/Vulkan no Android, entrada e interface.
4. Testar inicialização, título, menus e uma corrida real em hardware.

O jogo e a ROM são comerciais; por isso **não incluí fontes gerados do jogo no GitHub**. Base técnica: navjack/cruisn-world-recomp e navjack/cruisn-recomp, GPL-3.0. Nenhum APK jogável foi gerado nesta etapa.
