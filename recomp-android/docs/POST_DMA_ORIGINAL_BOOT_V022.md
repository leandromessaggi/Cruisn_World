# Marco v0.22 — execução da thread após primeiro DMA e CIC

**Estado: port nativo ainda não jogável.**

O teste **privado** executa o código C recompilado da ROM USA e vai além
do checkpoint da v0.20. A sequência validada é:

1. `recomp_entrypoint` / `func_802AD450`: configuração inicial e início de thread;
2. `func_802AD4C0`: solicitação do PI Manager e início da segunda thread;
3. `func_802AD8C0`: criação da fila, chamada a `func_802AD560`, transferência
   de 192 bytes da ROM `0xA6F80` para RDRAM `0x803E7AE0`, mensagem de DMA recebida;
4. **Após o DMA**, a mesma rotina chama **duas funções originais**
   `func_802AD870` e `func_802AD898`: elas leem SP IMEM e DMEM e retornam 1
   com o estado CIC-6106 preparado;
5. A execução é interrompida explicitamente **na entrada** da próxima função
   `func_802FB3B0`. O corpo dessa função **não** foi executado.

Não são usadas funções substitutas que sempre retornam sucesso para as
verificações SP: as funções originais são chamadas do `funcs_0.c` gerado pelo
N64Recomp. O ensaio é reproduzido em Linux com uma região de endereços grande
reservada esparsamente por `mmap` para comportar janelas SP/ROM. **Não é o
mapeamento definitivo para Android**, nem um escalonador de threads.

## Teste privado

```sh
python3 recomp-android/tools/verify_private_post_dma_boot.py \
  --private /caminho/para/fontes/gerados \
  --rom /caminho/CruisnWorld_USA.z64 \
  --output /tmp/post_dma_report.json
```

O script recusa ROM de outro SHA-256, recusa código CPU gerado de outra versão,
usa limites de tempo e gera armadilhas que abortam para outros símbolos não
implementados. **Não publique o relatório contendo conteúdo privado, a ROM,
ou o código gerado da ROM em um repositório público.**

O CI público valida somente os hooks ROM-free e a sintaxe do script. Não pode
reproduzir a execução original sem a ROM do proprietário.

## Próximo bloqueio

Integrar as funções na sequência após `0x802FB3B0`, trocar os hooks de DMA
síncronos e a simulação de threads por N64ModernRuntime real, e depois
conectar o renderizador RT64/Vulkan, áudio e controles Android.
