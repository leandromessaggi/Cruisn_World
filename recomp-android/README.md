# Cruis'n World — recompilação estática Android (marco v0.8)

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
3. A interface confere SHA-256 e exibe a carga de memória, hash do overlay e relatório das 17 assinaturas verificadas e testes de rotinas recompiladas.

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


## Marco v0.6 — serviços N64 reconstruídos, ainda não ligados ao gameplay

- Memória RDRAM KSEG0/KSEG1 com alias coerente (endereços 0x80000000 e 0xA0000000).
- Relógio determinístico COP0 Count de 46,875 MHz e 60000/1001 interrupções VI por segundo.
- Fila FIFO de eventos não bloqueante com capacidade limitada e testes de saturação.
- Representação serializada dos botões e do direcional analógico do controle N64.
- EEPROM de **16 kilobits (2048 bytes)** em 256 blocos de 8 bytes; persistência no armazenamento privado e rejeição de arquivos corrompidos.

Novos testes host e integração JNI ARM64. **As primitivas ainda não estão conectadas aos símbolos libultra nem executam uma corrida.** A conversão integral exige integração do runtime N64ModernRuntime (threads, PI/SI, RSP/RDP, RT64 e áudio) e recompilação das demais funções da ROM. Nenhuma ROM comercial incluída.


## Marco v0.8 — carregamento verificado do overlay original

A função MIPS 0x802E0098 (ROM 0x33C98–0x33D1C) foi inspecionada:
aloca 0x1A260 bytes por 0x802E2430, salva o ponteiro observado em
0x80343C2C e chama 0x802AD560 para copiar ROM 0xA7040–0xC12A0
para RAM 0x80050400. O helper opera em blocos de 0x4000 bytes.

A nova classe NativeOverlayLoad confere **todas as 33 instruções originais**
e traduz somente o contrato de transferência em C++ nativo: sete DMA PI
sequenciais, um evento VI por bloco e validação do hash integral do overlay.
Testes de ROM sintética executam no CI; ROM original fica privada.

**Não é ainda** uma recompilação do alocador, scheduler, libultra ou gráficos,
nem executa gameplay. Este APK é um laboratório de diagnóstico.


## Marco v0.9 — fila libultra em RDRAM e notificacao PI DMA

OSMesgQueue agora usa estrutura nativa de 24 bytes em RAM guest big-endian,
anel FIFO e indices de 32 bits compativeis com libultra. A ponte PI DMA publica
uma notificacao nesta fila e conserva eventos quando a fila esta cheia.
Testes com ROM sintetica sao executados no CI; um teste local com a ROM
original confirmou integridade do overlay e a notificacao recebida.

**Limite:** nao ha chamadas libultra do executavel ligadas a essa ponte;
OS_MESG_BLOCK, threads, RSP/RDP, audio e gameplay continuam pendentes.
