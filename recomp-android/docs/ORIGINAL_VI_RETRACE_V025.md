# Marco v0.25 — primeiro recebimento de evento VI pelo código original

**Status:** teste privado no Linux com código recompilado de Cruis’n World USA.
**Ainda não é uma aplicação Android jogável.**

A v0.24 verificou a criação da thread `0x802FB708` sem executar seu corpo.
Neste marco, a função original `func_802FB708` é chamada pelo harness com
os valores originais de argumento e pilha da terceira thread.

1. A execução original chega ao `osRecvMesg` bloqueante na fila do VI
   (`0x8035E4D0`), sem mensagens. O teste termina exatamente nesse ponto.
2. Uma função pública de teste `cw::inject_vi_retrace` insere `0x29A` no
   buffer de mensagens da RDRAM, atualizando `validCount` da `OSMesgQueue`.
3. Uma **nova invocação desde o início** da função recompilada alcança
   `osRecvMesg`, consome a mensagem `0x29A` e termina imediatamente via
   callback de teste. **Não houve retomada do contexto da primeira invocação.**

Além do ensaio privado, o teste público sem ROM verifica registradores VI,
filas circulares de oito entradas, ordem FIFO, fila cheia, wraparound,
rejeição de mensagens ausentes e ausência de entrega automática. A regressão
anterior de PI DMA também é executada.

## Teste privado do proprietário da ROM

```sh
python3 recomp-android/tools/verify_private_vi_setup.py \
  --private /caminho/para/fontes_gerados_privados \
  --rom /caminho/para/CruisnWorld-USA.z64 \
  --output resultado-v025.json
```

O script verifica o hash SHA-256 da ROM USA, os fingerprints dos arquivos C
recompilados e compila o teste em diretório temporário. Nenhum binário, ROM
ou código C derivado da ROM é gravado no repositório público. O teste usa
um mapeamento Linux de memória guest; **não é portável como runtime Android**.

## Limitações para evitar conclusões incorretas

- `inject_vi_retrace` é uma injeção manual de evento em teste, não VI a 60 Hz.
- A chamada `osRecvMesg_recomp` usa callbacks delimitados; **não substitui**
  o serviço bloqueante nem o escalonador do N64ModernRuntime.
- O código original executa até o primeiro recebimento, mas a rotina não
  processa o evento após o retorno do `osRecvMesg`: o teste para antes.
- A função recomeça em um novo contexto; nenhuma thread é retomada.
- Ainda não há processamento RSP/RDP, RT64/Vulkan, imagem, áudio, entrada,
  menu ou corrida jogável no Android.

**Próximo bloqueio:** integrar `osRecvMesg` com o escalonador real do
N64ModernRuntime, preservando contexto/threads, e ligar o backend de vídeo
Android; só assim poderemos executar os efeitos posteriores ao retrace.
