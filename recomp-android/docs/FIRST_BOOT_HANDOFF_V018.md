# v0.18 — verificacao do primeiro handoff real de threads do jogo

**Ainda nao jogavel.** Este experimento nao e um port do N64ModernRuntime.

## Avanco validado com codigo compilado da ROM americana original

No ambiente privado (nunca enviado ao GitHub publico), `tools/verify_private_boot_handoff.py`
compila a unidade `cpu/funcs_0.c` que o N64Recomp gerou, apos conferir o
SHA-256 exato. O teste executa as duas funcoes originais `recomp_entrypoint`
e `func_802AD450` e observa as chamadas reais:

1. `osViBlack(1)`;
2. `__osInitialize_common`;
3. `osCreateThread(thread=0x80359F80, id=1, entry=0x802AD4C0,
   stack=0x8035B130, priority=8)`;
4. `osStartThread(0x80359F80)`;
5. parada limitada em `pause_self`.

O codificador de mensagens `N64Recomp` usa RDRAM *word-swapped*; este teste le
os argumentos `stack` e `priority` gravados pela propria funcao recompiled
na pilha original (registrador `sp` + offsets 16/20), sem dados hardcoded
no modulo de runtime. A tabela de threads do teste e host-side.

**Observacao decisiva:** o hook `cruisn_world_seed_boot_state` e um marcador
sem efeitos de hardware neste teste; nao reproduz o CIC-6106, IPL3, SP DMEM,
SP IMEM nem as verificacoes de copia. `osCreateThread` apenas valida e registra
o pedido, e `osStartThread` apenas marca-o como iniciado; **a funcao filha
`0x802AD4C0` nao e executada**. A parada em `pause_self` e instrumentada por
um salto limitado a fim de evitar um loop infinito. O teste, portanto, prova
somente a sequencia de handoff, nao que um jogo inicialize.

## CI sem ROM

O GitHub Actions compila `runtime/boot_thread_handoff.cpp` para ARM64 com o
Android NDK e executa um teste sintetico das estruturas, argumentos e
rejeicao de ponteiros invalidos. Como o jogo gerado permanece privado, a CI
publica **nao** roda `func_802AD450` de fato.

## Como executar o teste privado (Linux)

```bash
python3 recomp-android/tools/verify_private_boot_handoff.py \
  --private /pasta/dos/arquivos-gerados \
  --output private-first-thread-report.json
```

Essa pasta deve conter `cpu/funcs_0.c`, `cpu/funcs.h`,
`cpu/recomp_decls.h` e `include/recomp.h` gerados do ROM USA verificado.

## Proximo bloqueio real

Substituir esta observacao limitada pelo N64ModernRuntime/ultramodern adaptado
para Android. O thread startup precisa transferir o controle para o corpo
recompilado de `0x802AD4C0`, com interrupt/timer/event/PI/RSP corretos, sem
simulacoes. Apos boot real: RT64/Vulkan, audio RSP, controle, EEPROM e
validacao de corridas. Nao anunciar uma versao jogavel ate que isso seja testado.
