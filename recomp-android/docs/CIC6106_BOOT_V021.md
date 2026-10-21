# Marco v0.21 — Cruis'n World (USA), estado de boot CIC-6106

O port nativo permanece **não jogável**.

O novo módulo `runtime/cic6106_seed.hpp/.cpp` reproduz oito palavras da
RAM/DMEM/IMEM esperadas pelo jogo após o boot CIC-6106, e disponibiliza a
primeira página (`0x1000` bytes) da ROM na janela guest `0x90000000`.
O módulo recebe callbacks de escrita: **não** mapeia a memória física e
**não** emula CIC, PI, DMA, proteção de código ou escalonador. O runtime futuro
deve validar e fornecer mapeamentos RDRAM, SP e cartucho separadamente.

Validação privada de **duas rotinas originais recompiladas**:
`func_802AD870` (SP IMEM) e `func_802AD898` (SP DMEM). Ambas retornam 1 com
estado correto e 0 quando a palavra esperada é alterada, conforme verificado
com o arquivo ROM USA de SHA-256
`5320594232f0c492a672464832425351bd06497c95fb243745e2493bc5df9c2c`.
Esses testes usam um *mmap* Linux reservado esparsamente como artifício de teste;
**não deve ser copiado para o runtime Android**. Não foram executadas outras
rotinas da thread nesse ensaio.

O módulo público valida o tamanho de 12 MiB, a ordem dos bytes z64, a ID
`NCWE` e os dois CRCs conhecidos do cabeçalho antes de escrever. O script
privado valida também o SHA-256 integral. Se falhar uma escrita, retorna
`false`, sem resultado artificial de sucesso. A inicialização parcial após erro
é responsabilidade do chamador descartar.

## Teste público, sem ROM

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror \
  -I recomp-android/runtime \
  recomp-android/runtime/cic6106_seed.cpp \
  recomp-android/tests/test_cic6106_seed.cpp -o /tmp/cw-cic6106
/tmp/cw-cic6106
```

## Teste privado, com ROM do proprietário

```sh
python3 recomp-android/tools/verify_private_cic6106.py \
  --private /caminho/para/generated \
  --rom /caminho/CruisnWorld_USA_normalizada.z64 \
  --output /tmp/private_cic_report.json
```

O CI público compila somente o módulo ROM-free com NDK. A chamada de proteção
real, o `N64ModernRuntime`, a renderização RT64/Vulkan, o áudio RSP, EEPROM,
threads e corrida ainda precisam ser conectados em Android.

Referência de engenharia: `navjack/cruisn-world-recomp`,
`native/cruisn_world_boot.cpp`.
