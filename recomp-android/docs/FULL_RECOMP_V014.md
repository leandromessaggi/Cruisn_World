# Marco v0.14 — recompilação integral como geração de código

Esta etapa substitui a estratégia de traduzir funções MIPS isoladas por N64Recomp, usando os patches e o mapa de símbolos da pesquisa navjack/cruisn-world-recomp.

**Resultado confirmado com a ROM fornecida pelo usuário (USA):** N64Recomp gerou 30 arquivos C contendo **1.473 definições de funções RECOMP_FUNC**, e RSPRecomp gerou aspMain.cpp (microcódigo de áudio). Todas as unidades C foram aceitas por GCC e compiladas para objetos Linux x86-64; os objetos foram reunidos em uma biblioteca estática local de aproximadamente 6,7 MB. O código RSP foi gerado, mas ainda não foi compilado nem integrado ao runtime.

**O que está no GitHub:** o workflow de ferramentas Linux sem ROM, seu mapa de símbolos e script para recompilar offline uma ROM do próprio usuário. Código extraído da ROM e a própria ROM **não** foram enviados ao repositório público.

## Repetir a geração local (Linux x86-64)

1. Baixe o artefato CruisnWorld-Pinned-N64Recompiler-Linux na execução da workflow Build pinned N64Recomp (ROM-free).
2. No computador com sua cópia local da ROM, execute:

    python3 recomp-android/tools/prepare_full_recomp.py --rom "Cruis'n World (USA).zip" --toolchain CruisnWorld-Pinned-N64Recompiler-Linux.zip --output private-generated

3. Confira private-generated/report.json. Os arquivos gerados são derivados da ROM, permanecem privados e não devem ser publicados neste repositório.

**Importante:** gerar e compilar código C em Linux não significa que o jogo rode no Android. Ainda faltam N64ModernRuntime ARM64, chamadas libultra e hooks, renderização RT64/Vulkan, áudio RSP integrado, entrada e EEPROM real, inicialização/menu e corrida original. A versão v0.13 continua sendo o APK Android de diagnóstico mais recente.

## Upstream

- https://github.com/navjack/cruisn-recomp
- https://github.com/navjack/cruisn-world-recomp
- https://github.com/navjack/N64Recomp

Licenças de ferramentas: GPL-3.0. Os direitos do jogo permanecem com seus titulares.
