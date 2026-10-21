# Cruis'n World — marco v0.19: três etapas originais do boot

**Não é uma versão jogável nem um runtime de threads.** Não inclui a ROM ou o código recompilado dela no GitHub.

## Medição local com arquivos privados gerados da ROM americana

A ferramenta `tools/verify_private_three_stage_boot.py` verifica SHA-256 do arquivo `cpu/funcs_0.c` e compila seu código C real com o `recomp.h` original. O *harness* executa, separadamente e com interrupções delimitadas:

1. `recomp_entrypoint` → `func_802AD450`, que inicializa estado, cria a thread de entrada **`0x802AD4C0`**, id 1 e prioridade 8, e a inicia.
2. `func_802AD4C0` com registradores `SP = stack - 0x10` e `A0 = argumento` dessa thread; ela chama `osCreatePiManager`, cria a thread id 3 em **`0x802AD8C0`**, com prioridade 10, inicia a thread 3, rebaixa a prioridade da thread 1 a zero e estaciona.
3. `func_802AD8C0` é executada apenas até invocar `osCreateMesgQueue`, criando a primeira fila (`0x80360760`, capacidade 1), onde o teste é interrompido.

As funções de interface são C++ nativo conforme o ABI do N64Recomp. A verificação `tests/test_boot_multithread_probe.cpp` usa **somente dados sintéticos** e roda publicamente, também cross-compilada para Android ARM64. O teste privado **roda no Linux**, sem Android NDK; a compilação Android valida exclusivamente os hooks de interface.

O teste privado recusa arquivos gerados de ROM que não correspondam ao hash conhecido. Símbolos de funções não incluídas nesta etapa podem ser preenchidos por *armadilhas de teste*: **se alguma delas for chamada, o teste termina com erro**, em vez de simular sucesso. O modo privado exige os arquivos da geração v0.14.

```bash
python3 recomp-android/tools/verify_private_three_stage_boot.py \
  --private /pasta/privada/geracao-v014 \
  --output /pasta/privada/relatorio-v019.json
```

## Ainda não implementado

- Estado CIC-6106/IPL3/SP para as verificações de proteção do jogo (no teste, o hook inicial é apenas observado).
- Threads MIPS agendadas automaticamente, troca de contexto, espera bloqueante e interrupções.
- Gerenciador de DMA PI real e transferência ROM durante a execução da terceira thread; os eventos acima são observados, não processados pelo runtime.
- Continuação da terceira thread depois da primeira fila; RSP/RDP, RT64/Vulkan, áudio, controles e corrida original.
- APK jogável ou pacote completo do Android NDK, pois a compilação Android cobre apenas a biblioteca de hooks sem o executável do jogo.

Este teste mede o próximo ponto de execução do **jogo original**, não substitui N64ModernRuntime e não deve ser usado como implementação final dos serviços libultra.
