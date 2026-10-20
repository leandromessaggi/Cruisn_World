# Cruis'n World — Android (APK independente)

Este projeto monta um APK Android dedicado a jogar **Cruis'n World** de Nintendo 64 usando LibretroDroid + Mupen64Plus-Next (GLES3) e o frontend de código aberto Ludere.

**Importante:** este repositório é público e NÃO distribui a ROM comercial. Na primeira execução, escolha seu arquivo local `.n64`, `.v64`, `.z64` ou `.zip` com a ROM. O aplicativo armazena a ROM no espaço privado, sem precisar de emulador externo.

## Instalar APK

1. Abra a aba **Actions** deste repositório.
2. Selecione a execução **Build Android APK** com status verde.
3. Em **Artifacts**, baixe **CruisnWorld-arm64-apk**.
4. Extraia e instale **CruisnWorld-arm64.apk** no celular Android ARM64.
5. Na primeira abertura, selecione o arquivo original da ROM.

## Controles e recursos

- Touch: direcional analógico, A, B, C/X, C/Y, Z, L, R, Start, Select.
- Gamepad físico USB/Bluetooth reconhecido pelo Android.
- Botão Voltar ou combinação Start+Select: menu, saves, estados, reset.
- **Mapear botoes fisicos:** escolha ação e pressione o botão para vinculá-lo.
- Opção de manter controles touch visíveis com o gamepad físico.

## Montar APK com ROM incorporada (uso local e privado)

Se você tiver uma cópia da ROM que pode utilizar, coloque-a em `game_rom/CruisnWorld.n64` e execute:

```bash
python3 -m pip install cairosvg
python3 build_game.py --refresh
```

Exige Python 3, Git, JDK 17, Android SDK 33 e acesso a internet para baixar dependências. O script gera `CruisnWorld-arm64.apk`. Não faça commit de arquivos de ROM: `game_rom/` está no `.gitignore`.

## Arquitetura

- `build_game.py`: clona Ludere na revisão fixa `8ec1ca8`, adapta orientação/touch, importa ROM e integra Mupen64Plus-Next GLES3.
- `overlay/ControllerMapping.kt`: remapeamento persistente dos botões físicos.
- `assets/launcher_icon.svg`: arte original do ícone.
- `.github/workflows/build-apk.yml`: pipeline de compilação ARM64.

O Android executa o jogo via **emulação nativa integrada**, não por port do binário original. A compatibilidade e velocidade em dispositivos Android precisam ser confirmadas com testes reais. As dependências são baixadas durante a compilação; o APK não depende de internet durante a partida.

As licenças do frontend/emulador estão em `LICENSE-NOTICE.txt`. A assinatura de release do projeto Ludere é apenas demonstrativa; para distribuição pública use uma assinatura própria.
