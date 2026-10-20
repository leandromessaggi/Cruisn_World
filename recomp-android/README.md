# Cruis'n World — recompilacao nativa Android (marco v0.12)

**Estado: APK de diagnostico, sem gameplay.** A nova `GuestRecompDispatcher` utiliza o PC do `GuestCpuContextScheduler` para selecionar e executar **somente** funcoes ja traduzidas estaticamente em C++ e previamente verificadas contra instrucoes da ROM USA. Converte argumentos a0–a3, v0 e f12/f14, preserva outros registradores e transfere PC ao endereco de retorno RA, ou encerra a thread quando RA=0. Um limite de 256 corpos por rodada impede execucao descontrolada. Entradas nao recompiladas sao rejeitadas, sem interpretacao dos opcodes MIPS.

Os testes sinteticos cobrem prioridade, atualizacao de estruturas do jogo, chamada real encadeada `0x802AF6EC -> 0x802AF5F4`, retorno a RA, fim de thread, enderecos invalidos e recusa de instrucoes desconhecidas. O APK repete um dos testes com a ROM original importada pelo usuario.

**Nao se trata de um runtime funcional do N64**: cada funcao recompilada e tratada como uma operacao indivisivel e so 18 entradas sao reconhecidas, sem CPU thread switching real, RSP/RDP nem corrida. A recompilacao existente para macOS ja demonstra uma stack mais adiantada; integrar o runtime completo ao Android e o bloqueio principal.

---

# Cruis’n World — Android recomp, marco v0.11

**Estado: APK de diagnóstico, não jogável.** As implementações anteriores (memória da ROM USA, 18 rotinas recompiladas isoladas, overlay, DMA PI, OSMesgQueue e espera cooperativa) permanecem.

## Novo na v0.11: contextos de threads MIPS

- `recomp_context.hpp/.cpp` implementa um registro de até 64 contextos host-side, identificados apenas pelos endereços guest de 32 bits. Não grava ponteiros C++ em RDRAM.
- Captura e restaura imagens dos 32 registradores gerais de 64 bits, 32 FPRs em formato bruto, HI/LO, PC, próximo PC, status/causa/EPC/Count do COP0, FCSR e a indicação de delay slot. `$zero` é sempre forçado a zero.
- Seleção determinística da thread READY de maior prioridade, preempção quando uma thread prioritária é acordada e yield cooperativo; não executa as instruções MIPS nem salva contextos OSThread em RDRAM.
- Integração com `GuestQueueWaiter`: bloqueio em `osRecvMesg` e retomada ao chegar uma mensagem de fila `OSMesgQueue` já representada na memória do N64. A mensagem de conclusão fica disponível para o futuro dispatcher nativo.
- Teste `tests/test_guest_context.cpp` cobre validação de endereços guest, criação, restauração de contexto, FPR/HI/LO/COP0/PC/delay slot, `$zero`, bloqueio, despertar, preempção e fila cheia. O JNI executa um diagnóstico equivalente após importar uma ROM reconhecida.

## Como compilar

O workflow `.github/workflows/android-recomp-boot.yml` executa os testes host C++ com ROM sintética e gera o APK ARM64 com CMake/NDK. A ROM comercial **não** deve ser enviada ao repositório GitHub público. Na primeira execução, o APK solicita a ROM local USA e verifica SHA-256 `5320594232f0c492a672464832425351bd06497c95fb243745e2493bc5df9c2c`.

## Próximo grande bloqueio

O projeto ainda carece de um **dispatcher completo das aproximadamente 1.536 funções MIPS identificadas** e do runtime N64ModernRuntime adaptado a Android. Renderização RSP/RDP, áudio, controle do jogo e corrida não são inicializados. A classe de contexto v0.11 não realiza troca de contexto do processador real nem constitui uma emulação.

Referências upstream: https://github.com/navjack/cruisn-world-recomp e https://github.com/navjack/cruisn-recomp (GPL-3.0). Nenhum código comercial ou dado de ROM incluído aqui.

## Marco v0.9 — fila OSMesgQueue de 32 bits e ponte PI DMA -> RAM guest

A biblioteca nativa implementa as operacoes **nao bloqueantes** do ABI de `OSMesgQueue` do Nintendo 64: `osCreateMesgQueue`, envio e recebimento de mensagens com indices circulares, limites e ponteiros em RAM KSEG0/KSEG1. O layout é de 24 bytes, seis inteiros big-endian, com capacidade, indice, quantidade de mensagens e endereco do array em RAM N64. Nao ha ponteiros nativos de 64 bits nesse estado.

A nova `GuestPiOsBridge` conecta o DMA ja implementado a uma mensagem real na fila guest e **nao perde o evento** se a fila estiver cheia, adiando-o ate ser consumido o conteudo anterior. Foram adicionados testes para wrap, endereco invalido, corrupcao de header, alias KSEG1, backpressure, transicao de VI e uma prova com o overlay da ROM USA original. O CI executa os testes sinteticos sem distribuir a ROM.

**Limite:** isto nao executa as instrucoes MIPS de `osSendMesg` ou `osRecvMesg`; apenas uma implementacao compativel do contrato sem bloqueio. `OS_MESG_BLOCK`, escalonamento real de threads, RSP/RDP, audio e gameplay continuam pendentes. Nao existe corrida jogavel nesta build.
