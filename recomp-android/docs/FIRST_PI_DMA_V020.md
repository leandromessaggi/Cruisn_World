# Cruis'n World — v0.20: primeira leitura real do cartucho no boot

**Status: teste delimitado de boot original, não jogo inicializado, não APK.**

A função MIPS original recompilada `0x802AD8C0`, após configurar sua primeira `OSMesgQueue` em `0x80360760`, chama `func_802AD560` para efetuar uma leitura PI DMA. O teste privado v0.20 passa o controle por essa chamada real e para somente após a recepção da mensagem de conclusão.

- `osInvalDCache_recomp` verifica uma janela de RDRAM (sem efeitos de cache real, que são responsabilidade do futuro runtime).
- `osPiStartDma_recomp` recebe os argumentos com o ABI real (a0–a3 + 3 argumentos na pilha), verifica limites da ROM, RAM e fila, e transfere bytes da **ROM original local** para a representação swizzled da memória usada pelo N64Recomp. Nesta prova, a conclusão é síncrona; a implementação final deverá ter PI manager e agendamento reais.
- `osRecvMesg_recomp` consome exatamente a mensagem de conclusão da fila `0x80360760`; não inventa uma mensagem em fila vazia.
- A primeira transferência observada lê **0xC0 bytes do offset 0xA6F80** do cartucho e grava em **0x803E7AE0**, confirmando igualdade byte a byte com a ROM.
- O teste privado exige SHA-256 da ROM USA (`53205942...`) e da unidade C gerada (`e8eb7931...`), e é encerrado em `osRecvMesg` antes das verificações de SP/CIC-6106.

## Reproduzir em ambiente privado

```bash
python3 recomp-android/tools/verify_private_dma_boot.py \
  --private /private/compiled-inputs \
  --rom /private/cruisn-world-usa.z64 \
  --output /private/cw-v020-report.json
```

O diretório `--private` contém `cpu/funcs_0.c`, `cpu/funcs.h`, `cpu/recomp_decls.h` e `include/recomp.h`, produzidos pela ferramenta da v0.14.

**Não publicar a ROM, a unidade C gerada nem a biblioteca derivada do jogo** no repositório público. Os testes públicos usam dados fabricados e verificam erros de DMA, fila vazia e limites. O workflow compila os hooks para Android ARM64, **não** vincula o jogo ao N64ModernRuntime.

### Bloqueios que persistem

- Execução das threads é sequencial no *harness*, sem escalonamento N64, preempção ou interrupções.
- A transferência PI e a mensagem são síncronas, não um dispositivo PI completo.
- O estado CIC-6106/IPL3/SP necessário à proteção do jogo não é reproduzido nesta prova.
- Ainda faltam N64ModernRuntime Android, RT64/Vulkan, RSP áudio, entrada e persistência.
- Não há menu, corrida nem APK novo jogável nesta versão.
