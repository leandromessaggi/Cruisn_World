# v0.17 — Primeira execução real do entrypoint e ponte libultra

**Status: testes de infraestrutura; não jogável.** O jogo, os fontes gerados e a ROM não são distribuídos neste repositório público.

## Duas validações distintas

1. **Privada (original):** `tools/verify_native_entrypoint.py` usa o `funcs_0.c`, `funcs.h`, `recomp_decls.h` e `include/recomp.h` gerados pelo N64Recomp fixado a partir da ROM USA (SHA-256 `532059...`). O script exige o fingerprint do entrypoint gerado: `e8eb7931...` e executa o corpo C original `recomp_entrypoint`. O teste confirma exatamente o zero da BSS (offset físico `0x353380`, tamanho `0x94760`), o ponteiro de pilha `0x80150400` e a primeira resolução de função `0x802AD450`. **A própria `func_802AD450` NÃO é executada nesse teste**: o resolver devolve um callback que verifica o contexto e retorna. Resultado validado localmente (Linux host x86-64); não é uma inicialização Android.
2. **Pública (sintética):** o workflow `verify-native-runtime-bridge.yml` cria uma função fictícia, apenas para testar se o harness detecta zero incompleto, configurações incorretas ou fonte sem fingerprint. Relatórios sintéticos são explicitamente marcados com `original_code_executed=false`. Não é prova de execução do jogo.

## Ponte ABI real (`runtime/basic_os_hooks.cpp`)

Sete símbolos exatos que o C gerado referencia, com assinaturas C e estrutura `recomp_context` da versão fixada:

- `osCreateMesgQueue_recomp`
- `osSendMesg_recomp`
- `osRecvMesg_recomp`
- `osVirtualToPhysical_recomp`
- `osInvalDCache_recomp`
- `osWritebackDCache_recomp`
- `osWritebackDCacheAll_recomp`

O layout guest da fila é de seis palavras `uint32_t` (24 bytes), e os conteúdos ficam no RDRAM **word-swapped** esperado pelos macros `MEM_W` do N64Recomp, *não* no formato big-endian de um arquivo ROM. Validação de ponteiros KSEG0/KSEG1, contagem máxima 1.024 e extremidades impede gravações fora dos 8 MiB de RDRAM; os testes incluem FIFO, fila cheia/vazia, ponteiro nulo de resposta e endereços inválidos. Teste local compilado usando o cabeçalho `recomp.h` **real** e teste CI usando um cabeçalho mínimo marcado como sintético.

**Limitações essenciais:** `OS_MESG_BLOCK` sem condição disponível é atualmente tratado como erro (não agendado). Operações de cache são no-ops apenas para a memória de CPU virtualmente coerente, **não** para buffers de dispositivos ou RSP/RT64. Não há interrupções, threads reais ou chamadas do jogo executando continuamente. Esta ponte não deve ser usada para iniciar o jogo nem apresentada como substituta do N64ModernRuntime.

## Execução local privada

```sh
python3 recomp-android/tools/verify_native_entrypoint.py \
  --input /caminho/do/pacote_privado_v0_16 \
  --output entrypoint_report.json

g++ -std=c++17 -O2 -Wall -Wextra -Werror \
  -I /caminho/do/pacote_privado_v0_16/include \
  recomp-android/runtime/basic_os_hooks.cpp \
  recomp-android/tests/test_basic_os_hooks.cpp \
  -o /tmp/cw_hook_test && /tmp/cw_hook_test
```

## Próximo bloqueio principal

Integrar N64ModernRuntime/ultramodern, incluindo `osCreateThread`, `osStartThread`, `osRecvMesg(OS_MESG_BLOCK)`, PI, callbacks de interrupção, inicialização específica de World e ligação do `get_function` a todas as funções/overlays. Só então conectar renderização RT64/Vulkan, RSP áudio, gamepad e EEPROM para iniciar e jogar corridas reais.
