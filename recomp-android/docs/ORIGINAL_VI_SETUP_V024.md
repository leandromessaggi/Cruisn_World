# Marco v0.24 — NTSC, eventos VI e criação da thread de vídeo

**Status:** teste privado de boot original **aprovado no Linux**, com código C
estaticamente recompilado da ROM americana do usuário. **Não jogável.**

## Falha real identificada no ambiente de teste

O teste anterior inicializava a RAM de diagnóstico com `0xA5` e sem definir
`osTvType` em `0x80000300`. Com esse valor indefinido, o jogo saltava a
seleção de modo de vídeo e seguia apenas parte da preparação de VI.

O novo teste inicializa `osTvType = 1`, como no boot NTSC da ROM USA, e
permite que a função recompilada `func_802FB3B0` execute as operações
observadas até o `osStartThread` da thread de vídeo.

## Comportamento observado e validado

- Transferência inicial de 192 bytes por PI DMA e duas verificações do SP:
  mantidas das versões anteriores.
- Inicialização das três filas de mensagens com layouts e capacidades esperados.
- `osViSetMode`: ponteiro de modo `0x8034C3D0` em memória guest válida.
- `osViBlack`: três pedidos de escurecimento na sequência do boot, sem imagem.
- `osViSetSpecialFeatures`: flags `0x52`.
- `osSetEventMesg`: eventos `4`, `9` e `14`, com mensagens
  `0x29B`, `0x29C` e `0x29D` na fila `0x8035E4D0`.
- `osViSetEvent`: notificação de retrace `0x29A`, intervalo `1`.
- `osCreateThread` e `osStartThread`: criação/início solicitados para a
  thread `id=6`, entrada `0x802FB708`, prioridade `40`.

**Checkpoint:** imediatamente após a terceira chamada original a
`osStartThread`, encerrando a execução deliberadamente com um callback de
teste. A thread `0x802FB708` **não foi executada**.

## Reproduzir com dados privados

No diretório do projeto público, mantendo ROM e fontes gerados exclusivamente
no computador do proprietário:

```bash
python3 recomp-android/tools/verify_private_vi_setup.py \\
  --private /caminho/para/recompilacao-privada \\
  --rom /caminho/para/cruisn-world-usa.z64 \\
  --output relatorio-privado.json
```

O script verifica os SHA-256 da ROM e dos arquivos recompilados esperados.
Não publica nem incorpora bytes da ROM no GitHub.

## O que isso NÃO implementa

Os hooks são instrumentação observacional de **teste limitado**. Não criam
threads reais, não geram eventos VI periódicos, não executam RSP/RDP, não
renderizam, não reproduzem áudio e não devem ser tratados como substitutos
de `N64ModernRuntime`/RT64. Uma build Android jogável depende da integração
desses componentes e da validação de uma corrida completa.
