# Cruis'n World — recompilação estática Android (marco v0.3)

**Status: diagnóstico Android nativo; NÃO é o jogo jogável.** Este projeto reconstrói, pouco a pouco, partes do **jogo real** em C++/ARM64, e não usa um emulador nem o jogo arcade original refeito visualmente. A ROM de Nintendo 64 não está no APK ou no repositório público.

## O que foi confirmado

A ROM USA normalizada para `.z64` tem 12 MiB, SHA-256 `5320594232f0c492a672464832425351bd06497c95fb243745e2493bc5df9c2c`. Todas as 14 primeiras instruções do boot são conferidas no APK, e todos os endereços foram conferidos com a ROM original. As pesquisas de [navjack/cruisn-world-recomp](https://github.com/navjack/cruisn-world-recomp) fornecem os endereços candidatos iniciais e a região de overlay.

| Região | Intervalo ROM | Endereço do N64 | Status da reconstrução |
|---|---|---|---|
| Boot + código e dados principais | `0x1000–0xA6F80` | `0x802AD400` | Cópia nativa para uma imagem RAM de 8 MiB |
| BSS | — | `0x80353380`, `0x94760` bytes | Zerada por código C++ nativo reconstruído |
| Overlay de código | `0xA7040–0xC12A0` | `0x80050400` | Carregamento nativo validado, FNV64 `617C6FFD93FA6347` |

### Quatro rotinas MIPS realmente recompiladas

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
3. A interface confere SHA-256 e exibe a carga de memória, hash do overlay e resultados das quatro rotinas recompiladas.

**Isso não inicia uma corrida, não emite gráficos ou áudio e não aplica emulação MIPS/RSP.** É o próximo passo testável rumo a um port estático, não uma conversão completa.

## Testes e builds

```
python3 -m unittest discover -v -s tests
python3 tools/recomp_map.py /caminho/CruisnWorld.n64 --output relatorio.json

g++ -std=c++17 -O2 -Wall -Wextra -Werror tests/test_native_memory.cpp android/app/src/main/cpp/recomp_memory.cpp -o /tmp/cw_test
(cd /tmp && ./cw_test)
```

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
