# Arcade World Racing — protótipo Android nativo

Este projeto é **um jogo original de corrida arcade para Android**, desenvolvido em Java com `Activity` + `SurfaceView` + `Canvas`. **Não utiliza emulador, ROM, libretro, navegador ou arquivos do jogo original**. O APK contém código compilado pelo Android Gradle Plugin.

> **Não é a conversão exata de Cruis’n World (Nintendo 64).** Uma ROM inclui código MIPS e dados binários, não o código-fonte do jogo. Produzir um port fiel demandaria reverse engineering extenso de renderização, física, pistas, veículos, áudio e outros sistemas. Este projeto é um primeiro protótipo independente, criado para validar a experiência de corrida totalmente nativa.

## Funcionalidades desenvolvidas

- **4 cenários originais:** Costa Solar, Cidade Neon, Deserto Vermelho e Picos Gelados.
- Estrada curva com elevação e objetos em perspectiva pseudotridimensional, implementada com Canvas 2D.
- **3 voltas**, largada com contagem regressiva, cronômetro, recordes persistentes e checkpoints.
- Física arcade própria: aceleração, frenagem, direção, fora da pista e colisões.
- **9 adversários** com movimento independente e velocidade variável.
- Nitro recarregável.
- Interface widescreen 100% desenhada no Android; HUD de velocidade, progresso, nitro e tempo.
- Controles touch multi-toque para direção, acelerador, freio e nitro.
- Gamepads Android Bluetooth/USB: D-pad, analógico, gatilhos, A/B/X e START.
- Mapeamento de seis ações para botões físicos, salvo no aparelho.
- Pausar, retomar, reiniciar e voltar ao menu.
- Arte de ícone original desenhada para o protótipo.

## Build

1. Instale JDK 17, Android SDK 34 e Gradle 8.4 (ou use GitHub Actions).
2. Execute na raiz: `gradle --no-daemon :app:assembleDebug`.
3. Encontre `app/build/outputs/apk/debug/app-debug.apk` (APK assinado com chave de debug).

No GitHub, na branch `native-android-port`, o arquivo `.github/workflows/native-apk.yml` compila e publica o APK em **Actions → Build Native Racing APK → Artifacts → ArcadeWorldRacing-Nativo-APK**.

## Controles

| Ação | Touch | Gamepad (padrão) |
|---|---|---|
| Virar esquerda/direita | setas | direcional ou analógico |
| Acelerar | GÁS | A ou gatilho R2 |
| Frear | FREIO | B ou gatilho L2 |
| Nitro | NITRO | X |
| Pausar | ícone menu | START ou Voltar |
| Reconfigurar | botão CONTROLES no menu | selecionar a ação e pressionar um botão |

## Organização

- `app/src/main/java/.../RacingActivity.java`: entrada do aplicativo e gerenciamento de ciclo de vida.
- `app/src/main/java/.../RacingView.java`: loop, projeção, desenho, física, IA, HUD e inputs.
- `.github/workflows/native-apk.yml`: automação do build no GitHub.

## Próximos passos para uma recriação fiel

Uma reprodução completa do Cruis’n World exigiria reconstruir os mapas e circuitos reais, extrair/reescrever modelos com autorização, reproduzir física, interface, áudio, modos de jogo e validar comportamento cena por cena. A ROM fornecida não foi incorporada a este repositório público.
