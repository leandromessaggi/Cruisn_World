# Marco v0.23 — execução original alcança inicialização de vídeo

**Estado: teste de execução parcial no Linux, não jogo funcional e não APK novo.**

O ensaio offline privado agora compila `funcs_0.c` **e** `funcs_14.c` gerados da ROM americana verificada, invoca `recomp_entrypoint` e avança pelas três rotinas de boot já analisadas. A rotina original `func_802FB3B0` é **executada**, ao contrário da v0.22, que parava imediatamente em sua entrada.

O teste verifica: transferência PI DMA dos mesmos 192 bytes da ROM, duas verificações originais SP IMEM/DMEM, criação de três estruturas `OSMesgQueue` na RAM guest (uma para PI e duas de capacidade 8), chamada do gerenciador VI com prioridade `0xFE` e segunda solicitação de apagamento de vídeo. Para antes da implementação de `osViBlack`; nenhum quadro é renderizado.

**Limitações explícitas:**

- PI DMA continua síncrono e a sequência das threads é conduzida pelo teste, sem escalonamento real.
- O estado CIC-6106 é preparado por um helper de teste, sem executar o firmware IPL3.
- `osCreateViManager_recomp` observa os argumentos, **não cria** um relógio de vídeo, interrupções ou thread de vídeo real.
- `osViBlack_recomp` registra o chamado e aciona um callback de parada delimitada; **não implementa** a renderização.
- A reserva de endereços com `mmap` é exclusiva do teste Linux e não funciona como gerenciador de RDRAM no Android.
- Símbolos ainda não implementados resultam em armadilhas explícitas que abortam (fail-closed).

## Reproduzir, mantendo a ROM privada

```
python3 recomp-android/tools/verify_private_vi_setup.py \
  --private /caminho/do/pacote_privado_extraido \
  --rom /caminho/CruisnWorld_USA.z64 \
  --output /tmp/cw_vi_setup_report.json
```

Use somente a ROM americana com SHA-256 `5320594232f0c492a672464832425351bd06497c95fb243745e2493bc5df9c2c` e os arquivos `funcs_0.c` e `funcs_14.c` gerados pelo N64Recomp. O script valida seus fingerprints. Nunca envie a ROM nem o código derivado do jogo para o repositório público.

## Próximo bloqueio real

Substituir os hooks observacionais pelas rotinas N64ModernRuntime de VI/threads, progredir após `osViBlack` e ligar o backend RT64/Vulkan no Android; o primeiro frame e a primeira corrida permanecem pendentes.
