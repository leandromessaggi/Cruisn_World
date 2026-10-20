# Arcade World Racing — Android nativo

Este é um **protótipo original de corrida para Android**, com código Java e renderização pelo Android Canvas. Ele **não depende da ROM, não usa emulador ou Libretro**, e **não é um port fiel** do Cruis'n World original.

A versão anterior do repositório, com núcleo de emulação Nintendo 64, permanece na branch [main](https://github.com/leandromessaggi/Cruisn_World/tree/main).

## Código e APK

- [Código Android nativo](native-android/)
- [Workflow de compilação](.github/workflows/native-apk.yml)
- [Execuções GitHub Actions](https://github.com/leandromessaggi/Cruisn_World/actions/workflows/native-apk.yml)

Para baixar o APK: abra o workflow **Build Native Racing APK**, selecione a execução mais recente com status verde, baixe o artefato **ArcadeWorldRacing-Nativo-APK** e extraia o `app-debug.apk`.

Veja [README do projeto Android](native-android/README.md) para funcionalidades, controles e instruções. A compatibilidade final precisa ser testada em aparelho Android.

## Escopo

O jogo tem quatro pistas originais, adversários, nitro, três voltas, controles touch multitoque e mapeamento de botões físicos. A recriação fiel do game Nintendo 64 dependeria de engenharia reversa substancial, incluindo reconstrução de pistas, modelos, física, áudio e recursos originais, e não está implementada aqui.
