# Marco v0.16 — código recompilado real em objetos AArch64 (privado)

**Resultado com a ROM USA do próprio usuário:** 30 unidades C N64Recomp compiladas com Clang 17, `--target=aarch64-linux-android26`, `-ffreestanding`, para 30 arquivos ELF64 AArch64. `llvm-ar` agrupou os objetos em uma biblioteca estática de aproximadamente 3,4 MB. O ELF contém 1.473 definições de funções. Todos os objetos e os fontes ROM-derivados permanecem fora do GitHub público.

## Limites do teste

O ambiente de execução local **não possui Android NDK**. O teste usa um conjunto mínimo e isolado de *declarações ABI* para `math.h`, `fenv.h`, `assert.h` e `stdlib.h`. Essas declarações **não implementam funções**, não simulam o runtime, nem permitem afirmar que o binário está ligado a Bionic ou é um APK Android funcional. `--target` identifica a arquitetura/intenção da compilação; a verificação final de ABI e a ligação em `libcw_game.so` continuam pendentes. O teste real com NDK deve ser feito com `tools/build_android_cpu.py` sobre os fontes privados.

O compilador Clang detectou uma dependência da declaração `cruisn_world_seed_boot_state` e da função matemática `sqrtf` que precisaram estar presentes no conjunto de cabeçalhos. Por isso o script público do NDK passou a incluir **obrigatoriamente** `cpu/recomp_decls.h` e ativar `-Werror=implicit-function-declaration`. O teste sintético do GitHub não substitui a compilação com a ROM verdadeira.

## Auditoria de símbolos

`tools/audit_arm64_cpu.py` verifica o ELF de cada objeto e classifica os símbolos externos entre a lista de 49 *hooks* `*_recomp`/específicos do título e dependências de infraestrutura (14 outros símbolos). Resultado atual: **63 símbolos não resolvidos** após remover referências internas já cobertas por outros objetos. Esses símbolos NÃO devem ser preenchidos com stubs silenciosos que simplesmente retornam sucesso; o runtime real precisa implementá-los e ser validado com o jogo.

### Uso local

```sh
python3 recomp-android/tools/build_android_cpu.py \
  --input CruisnWorld_ARM64_Recomp_Private_BuildKit_v0_15.zip \
  --ndk "$ANDROID_NDK_HOME" --output private-out --keep_objects

python3 recomp-android/tools/audit_arm64_cpu.py \
  --objects private-out/objects \
  --declarations PRIVATE_EXTRACTED/cpu/recomp_decls.h \
  --readelf "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf" \
  --expected-functions 1473 --output private-out/abi_audit.json
```

**Dependências seguintes:** 49 hooks de libultra/título mais 14 auxiliares/bibliotecas; vincular N64ModernRuntime, RSP de áudio e RT64/Vulkan; inicialização CIC-6106 e EEPROM; validar menu/corrida em dispositivo Android. O estado continua não jogável. Não publicar os objetos/código derivados da ROM nem a ROM em repositório público.
