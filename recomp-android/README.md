# Cruis'n World — recompilação Android v0.10 (diagnóstico, não jogável)

O componente `recomp_thread_wait.hpp/.cpp` acrescenta um modelo **cooperativo**, orientado a prioridades, para operações bloqueantes de envio e recebimento em filas `OSMesgQueue` armazenadas na RAM guest. IDs de threads são endereços de 32 bits; nenhuma referência a ponteiros de host é escrita na memória N64. A fila continua possuindo seu layout big-endian libultra.

Quando um thread espera por mensagem ou por vaga na fila, o modelo registra o bloqueio e completa a operação posteriormente, sem interpretar instruções MIPS. Em caso de fila corrompida, a operação retorna erro. Prioridades maiores são atendidas primeiro, e a ordem de cadastro desempata prioridades iguais.

**O modelo não implementa `osCreateThread`, salvamento de registradores, escalonamento preemptivo ou chamadas reais das funções libultra recompiladas.** O jogo permanece não jogável, sem áudio/renderizador nem loop de gameplay. Não confundir o modo cooperativo de teste com a execução de threads MIPS reais.

Teste host: `tests/test_guest_thread_wait.cpp`; diagnóstico no APK: bloqueio e retomada de um recebimento guest por chegada de mensagem. A ROM comercial permanece fora do repositório.

# Cruis'n World — recompilação estática Android (marco v0.9)

**Status: diagnóstico Android nativo; NÃO é o jogo jogável.** Este projeto reconstrói, pouco a pouco, partes do **jogo real** em C++/ARM64, e não usa um emulador nem o jogo arcade original refeito visualmente. A ROM de Nintendo 64 não está no APK ou no repositório público.

## O que foi confirmado

A ROM USA normalizada para `.z64` tem 12 MiB, SHA-256 `5320594232f0c492a672464832425351bd06497c95fb243745e2493bc5df9c2c`. Todas as 14 primeiras instruções do boot são conferidas no APK, e todos os endereços foram conferidos com a ROM original. As pesquisas de [navjack/cruisn-world-recomp](https://github.com/navjack/cruisn-world-recomp) fornecem os endereços candidatos iniciais e a região de overlay.

| Região | Intervalo ROM | Endereço do N64 | Status da reconstrução |
|---|---|---|---|
| Boot + código e dados principais | `0x1000–0xA6F80` | `0x802AD400` | Cópia nativa para uma imagem RAM de 8 MiB |
| BSS | — | `0x80353380`, `0x94760` bytes | Zerada por código C++ nativo reconstruído |
| Overlay de código | `0xA7040–0xC12A0` | `0x80050400` | Carregamento nativo validado, FNV64 `617C6FFD93FA6347` |

### 18 rotinas MIPS realmente recompiladas (inclui um caller real)

Nesta etapa, 18 funções MIPS têm implementações C++ estáticas separadas em `recomp_functions.cpp` e verificadas por assinatura de todas as instruções conhecidas. Incluem setters, operações em estruturas, consulta de byte em tabela indexada, salto condicional, instrução *branch-likely* e conversão de inteiros em ponto flutuante. As quatro primeiras estão listadas abaixo como exemplo.

Estas instruções MIPS foram conferidas no dump US e seus efeitos traduzidos manualmente em C++ (sem interpretação de opcodes em tempo de execução):

| Função N64 | Endereço de dado | Efeito comprovado |
|---|---|---|
| `0x802AEE9C` | `0x803334C0` | escreve `1` |
| `0x802AEEAC` | `0x803334C0` | escreve `0` |
| `0x802AF5E4` | `0x803334C8` | escreve `1` |
| `0x802AF5F4` | `0x803334C8` | escreve `0` |

**Não atribuímos sem pesquisa uma função de gameplay a esses dados:** são apenas setters de memória do programa original. A tradução reproduz também o store do *branch delay slot* de `jr $ra`.

## Executar o diagnóstico no Android

1. Baixe o APK da execução verde **Android recomp boot checkpoint** na branch `recomp-android` em **Actions → Artifacts**.
2. Instale em dispositivo ARM64; abra e selecione sua ROM `Cruis'n World (USA).zip` ou outro dump no formato `.n64`, `.v64`, `.z64`.
3. A interface confere SHA-256 e exibe a carga de memória, hash do overlay e relatório das 18 assinaturas verificadas, testes de rotinas recompiladas e transferência PI DMA.

**Isso não inicia uma corrida, não emite gráficos ou áudio e não aplica emulação MIPS/RSP.** É o próximo passo testável rumo a um port estático, não uma conversão completa.

## Testes e builds

```
python3 -m unittest discover -v -s tests
python3 tools/recomp_map.py /caminho/CruisnWorld.n64 --output relatorio.json

g++ -std=c++17 -O2 -Wall -Wextra -Werror tests/test_native_memory.cpp android/app/src/main/cpp/recomp_memory.cpp -o /tmp/cw_test
(cd /tmp && ./cw_test)
```

O catálogo `tools/recompiled_functions.json` documenta os 18 endereços e SHA-256 dos corpos das rotinas (sem guardar bytes de ROM).

O teste host gera uma ROM **inteiramente sintética** e testa mapa, limites da RAM, BSS, overlay, chamadas nativas e rejeição de instruções alteradas. O teste de integração local (`tests/native_rom_integration.cpp`) foi executado também contra a ROM autêntica fornecida pelo usuário, mas **não é executado em GitHub Actions, onde a ROM não é enviada**.

O workflow compila um APK ARM64 com CMake/NDK e executa testes sem ROM antes da compilação. A biblioteca JNI usa **alinhamento de 16 KiB** para Android recente.

## Marco v0.6 — serviços de dispositivos N64 (ainda isolados do gameplay)

**Atenção:** este marco continua sendo um laboratório, não uma build jogável.

- `recomp_devices.hpp/.cpp`: relógio determinístico a ~59,94 VI/s com `COP0 Count` de 46,875 MHz, sem dependência do relógio do telefone; uma fila de eventos FIFO limitada, não bloqueante, para preparar as mensagens de VI; serialização correta dos botões `A/B/Z/Start/L/R/C`, D-pad e analógico do controle do N64.
- EEPROM N64 **16 Kbit = 2048 bytes**, não 16 KiB. Leitura e escrita em 256 blocos de 8 bytes, com arquivo persistente `.eep` no armazenamento privado do app e validação rigorosa de tamanho.
- A RAM agora reconhece os aliases `0x80000000–0x807FFFFF` (KSEG0) e `0xA0000000–0xA07FFFFF` (KSEG1), de acordo com o mapeamento de RDRAM do console; endereços fora desses dois intervalos continuam rejeitados.
- Testes `test_native_devices.cpp` cobrem transcurso de 600 frames, ordenação FIFO, limites de fila, codificação de botões, persistência EEPROM e rejeição de save corrompido. Os testes existentes de memória e recompilação permanecem ativos no CI.

**Limite comprovado:** estes serviços NÃO estão ligados a `osCreateThread`, `osSendMesg`, `osEepromLongRead`, às funções recompiladas do jogo, nem a RT64. A aplicação continua validando e relatando subsistemas, sem lançar a corrida. O próximo passo significativo é integrar um runtime N64ModernRuntime compatível com Android e gerar as funções restantes em C++ a partir da ROM, mantendo dados comerciais fora do GitHub.

## Marco v0.7 — PI DMA cartucho para RDRAM, com notificação de conclusão

O módulo novo `recomp_pi.cpp/.hpp` introduz uma **ponte de preparação** para a operação de DMA de dados da ROM de cartucho do N64 (`0x10000000`) para a RDRAM em `KSEG0` ou `KSEG1`:

- Um pedido pendente de cada vez; cópias estritamente limitadas pelos 12 MiB da ROM e 8 MiB de RAM. Pedidos inválidos são rejeitados sem alterar a RAM.
- O DMA **só é concluído depois de avançar pelo menos um VI** do relógio de teste, e então envia mensagem a uma fila FIFO de capacidade limitada.
- Quando a fila de conclusões está cheia, o pedido continua pendente e não modifica a RAM, até haver espaço para publicar a conclusão.
- O mesmo bloco real de overlay `0xA7040–0xC12A0` pode ser recopiado para `0x80050400` e retorna ao hash FNV64 `617C6FFD93FA6347` da ROM americana original. Isso foi verificado localmente com a cópia fornecida; o GitHub testa a mesma lógica com ROM sintética, sem distribuir dados comerciais.
- No aplicativo, o diagnóstico recupera quatro bytes originais do início do overlay por um pedido PI após um VI, comprovando a cópia e a mensagem de conclusão.

**Precisão e limites:** o atraso de um VI é uma regra explícita do laboratório, não uma simulação precisa dos ciclos da PI. `GuestPiDma` não é ainda a implementação de `osPiStartDma`, não configura registradores PI reais, não modela interrupções do hardware e não está conectado à função original `0x802E0098` que solicita o overlay. A recompilação do jogo e o runtime de threads e gráficos continuam pendentes. Um APK desse marco permanece **não jogável**.

Rodar os novos testes host localmente:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror tests/test_native_pi.cpp \
  android/app/src/main/cpp/recomp_pi.cpp android/app/src/main/cpp/recomp_memory.cpp \
  android/app/src/main/cpp/recomp_devices.cpp android/app/src/main/cpp/recomp_functions.cpp \
  -o /tmp/cw-pi-tests && (cd /tmp && ./cw-pi-tests)
```

A integração da ROM original pode ser testada com `tests/native_pi_rom_integration.cpp`, compilado com os mesmos arquivos de origem, informando o caminho da ROM `.z64` fornecida pelo usuário.


## Marco v0.8 — carregamento de overlay ligado ao código MIPS original

A rotina **`func_802E0098`** foi examinada no executável real. Sua assinatura completa de 33 instruções ocupa ROM `0x33C98–0x33D1C`. Foi identificada a cadeia:

1. Inicializa a região de alocação (`jal 0x802E0060`, ainda não portado).
2. Chama o alocador em `0x802E2430`, solicitando `0x1A260` bytes (tipo `0x3A`). A implementação de heap real não está pronta; o laboratório reserva o endereço observado `0x80050400`.
3. Salva esse endereço na variável global `0x80343C2C`.
4. Solicita a transferência da ROM `0xA7040` à memória `0x80050400` pelo helper `0x802AD560`, que opera em blocos de até `0x4000` bytes, com fila de conclusão PI.
5. Chama `0x802AFCD0` depois da cópia, cujo comportamento completo não foi reconstruído.

O módulo novo `recomp_loader.{hpp,cpp}` implementa uma **tradução nativa de alto nível apenas do contrato de transferência** desse caminho. A assinatura das 33 instruções é comparada byte a byte contra a ROM carregada. A transferência ocorre em **sete blocos** (`0x4000` × 6 + `0x2260`), em sete eventos VI simulados, usando o DMA PI nativo do marco anterior. O hash FNV64 final do overlay é conferido com os dados da ROM privada. **Não há ainda reinterpretação da função original, implementação do alocador nem chamada a `osPiStartDma` dentro de uma thread recompiled.** O fluxo pode ser rejeitado caso a assinatura da rotina original seja diferente.

Os testes host cobrem ROM sintética, DMA assíncrono, fila saturada, endereço do ponteiro, limites e hash integral, e um segundo teste verifica a ROM USA real local. O GitHub Actions executa apenas os testes sintéticos, sem publicar a ROM comercial.

Arquivo principal: `android/app/src/main/cpp/recomp_loader.cpp`; testes: `tests/test_native_loader.cpp`, `tests/native_loader_rom_integration.cpp`.

## Próximos módulos reais

- Construir a ponte entre as funções recompiladas e as interfaces do N64ModernRuntime, incluindo escalonamento de threads, interrupções, DMA e tempos de quadro.
- Recompilar e validar as 1.536 *funções candidatas* identificadas no projeto upstream (muitas ainda precisam de inspeção manual).
- Renderizar os comandos RSP/RDP do jogo em Vulkan/OpenGL ES e conectar o áudio RSP.
- Traduzir entrada de controle N64 para touch e controle físico; implementar EEPROM 16K e persistência de saves.
- Finalmente, validar o boot completo, as pistas e a jogabilidade com hardware real.

## Referências

- [cruisn-world-recomp](https://github.com/navjack/cruisn-world-recomp) — símbolos, overlay, boot, microcódigo RSP
- [N64Recomp](https://github.com/N64Recomp/N64Recomp) — geração de código C a partir de MIPS
- [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) — serviços do N64 para jogos estaticamente recompilados
- [splat](https://github.com/ethteck/splat) — separação de segmentos

Nenhum asset proprietário é publicado; as licenças dos repositórios upstream devem ser respeitadas em qualquer integração futura.

## Marco v0.5 — fluxo de chamadas real, sem interpretador MIPS

Esta etapa adiciona a função original `0x802AF6EC` (10 instruções) e reconstrói seu fluxo estático C++ de **chamada à função `0x802AF5F4`**, seguida da escrita de `1` no endereço `0x803334CC`. O `JAL` em `0x802AF6F4` foi conferido na ROM real (`0x0C0ABD7D`).

Um `CallTrace` limitado registra a sequência de duas chamadas (caller/callee), detecta orçamento insuficiente e valida os efeitos em RAM. O APK passa a exibir esse teste com a ROM selecionada. Não se trata de interpretar instruções MIPS dinamicamente.

A ferramenta `tools/call_graph.py` varre os segmentos de código MIPS e reporta os locais de `JAL` que apontam para as 18 rotinas reconstruídas, sem exportar dados de jogo. Esse relatório é **estático e parcial**, não um grafo completo de jogo nem comprovação de alcance em runtime.

## Marco v0.4 — escopo e limitações

- 18 funções reais com compilação estática para C++ (código-fonte e assinaturas verificáveis).
- `CallArgs` explicita argumentos de registradores MIPS (`a0`–`a3`, `v0` e bits crus dos FPRs `f12`/`f14`). O C++ reproduz diretamente efeitos comprovados em RAM.
- Suporte mínimo a escritas e leituras por byte, escrita de palavra big-endian, consultas a tabelas por índice e conversão float32.
- Testes verificam *delay slots*, *branch-likely*, ponteiros inválidos e assinatura das 18 funções com ROM sintética, além da integração local com a ROM autêntica fornecida.
- **Não há ainda escalonador de threads, DMA, processador de comandos gráficos, som ou lógica completa de corrida.** O APK continua sendo ferramenta de diagnóstico, não port jogável.


## Marco v0.9 — fila OSMesgQueue de 32 bits e ponte PI DMA -> RAM guest

A biblioteca nativa implementa as operacoes **nao bloqueantes** do ABI de `OSMesgQueue` do Nintendo 64: `osCreateMesgQueue`, envio e recebimento de mensagens com indices circulares, limites e ponteiros em RAM KSEG0/KSEG1. O layout é de 24 bytes, seis inteiros big-endian, com capacidade, indice, quantidade de mensagens e endereco do array em RAM N64. Nao ha ponteiros nativos de 64 bits nesse estado.

A nova `GuestPiOsBridge` conecta o DMA ja implementado a uma mensagem real na fila guest e **nao perde o evento** se a fila estiver cheia, adiando-o ate ser consumido o conteudo anterior. Foram adicionados testes para wrap, endereco invalido, corrupcao de header, alias KSEG1, backpressure, transicao de VI e uma prova com o overlay da ROM USA original. O CI executa os testes sinteticos sem distribuir a ROM.

**Limite:** isto nao executa as instrucoes MIPS de `osSendMesg` ou `osRecvMesg`; apenas uma implementacao compativel do contrato sem bloqueio. `OS_MESG_BLOCK`, escalonamento real de threads, RSP/RDP, audio e gameplay continuam pendentes. Nao existe corrida jogavel nesta build.
