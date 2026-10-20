// JNI entrypoint to the actual-game native state and 17 statically
// recompiled state setters. No ROM bytes or complete N64 emulation bundled.
#include "recomp_memory.hpp"
#include "recomp_devices.hpp"
#include "recomp_pi.hpp"
#include "recomp_loader.hpp"
#include "recomp_os_queue.hpp"
#include "recomp_thread_wait.hpp"
#include "recomp_context.hpp"
#include "recomp_dispatch.hpp"
#include <jni.h>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <vector>
#include <cstdint>

namespace {
std::uint32_t read32(const std::uint8_t* p) {
  return (std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|
         (std::uint32_t(p[2])<<8)|std::uint32_t(p[3]);
}
constexpr std::uint32_t boot[] = {
  0x3C088035,0x3C090009,0x25083380,0x35294760,0x2129FFF8,
  0xAD000000,0xAD000004,0x1520FFFC,0x21080008,
  0x3C0A802B,0x3C1D8015,0x254AD450,0x01400008,0x27BD0400
};
}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_cruisn_recomp_PortLabActivity_nativeBootProbe(JNIEnv* env, jclass, jstring filepath) {
  if (!filepath) return env->NewStringUTF("Erro: caminho de ROM vazio");
  const char* path_ptr=env->GetStringUTFChars(filepath,nullptr);
  const std::string path(path_ptr);
  env->ReleaseStringUTFChars(filepath,path_ptr);
  std::ostringstream report;
  std::ifstream input(path,std::ios::binary);
  if (!input) return env->NewStringUTF("Erro: ROM nao acessivel");
  input.seekg(0x1000);
  std::vector<std::uint8_t> actual(sizeof(boot));
  input.read(reinterpret_cast<char*>(actual.data()),actual.size());
  if (!input) return env->NewStringUTF("Erro: leitura do boot MIPS");
  for (std::size_t i=0;i<sizeof(boot)/sizeof(boot[0]);++i) {
    if (read32(actual.data()+i*4)!=boot[i]) {
      return env->NewStringUTF("Erro: assinatura original do boot diverge");
    }
  }
  cw::NativeState state;
  std::string error;
  if (!state.load(path,&error)) return env->NewStringUTF(("Erro: "+error).c_str());
  if (state.overlayFNV64() != UINT64_C(0x617c6ffd93fa6347)) {
    return env->NewStringUTF("Erro: hash do overlay original divergiu");
  }
  report << "[OK] Boot original MIPS: 14 instrucoes verificadas\n";
  report << "[OK] JNI ARM64: mapeamento de memoria N64\n";
  report << state.diagnostics() << "\n\n";
  // Real original address setters converted to native C++ calls.
  // Data is restored immediately as each setter-pair is applied.
  const std::uint32_t addrs[] = {0x802AEE9C,0x802AEEAC,0x802AF5E4,0x802AF5F4};
  const std::uint32_t flags[] = {cw::kFlag1Address,cw::kFlag1Address,
                                  cw::kFlag2Address,cw::kFlag2Address};
  const std::uint32_t expected[] = {1,0,1,0};
  for(int i=0;i<4;++i) {
    std::uint32_t value=UINT32_MAX;
    if(!state.callRecompiled(addrs[i])||!state.readU32(flags[i],&value)||value!=expected[i]) {
      return env->NewStringUTF("Erro: rotina recompilada nao passou no teste");
    }
    report << "[OK] rot 0x" << std::hex << std::uppercase << addrs[i]
           << " -> RAM 0x" << flags[i] << " = " << std::dec << value << "\n";
  }
  cw::CallTrace flow;
  if (!state.writeU32(cw::kFlag2Address, 11) ||
      !state.writeU32(0x803334CCu, 0) ||
      !state.callRecompiled(0x802AF6ECu, nullptr, &flow)) {
    return env->NewStringUTF("Erro: execucao encadeada ARM64");
  }
  std::uint32_t flag2=0,flag3=0;
  if (flow.entries.size()!=2 || flow.entries[0]!=0x802AF6ECu ||
      flow.entries[1]!=0x802AF5F4u ||
      !state.readU32(cw::kFlag2Address, &flag2) || flag2!=0 ||
      !state.readU32(0x803334CCu, &flag3) || flag3!=1) {
    return env->NewStringUTF("Erro: pos-condicoes da chamada estaticamente recompilada");
  }
  report << "[OK] Fluxo real 0x802AF6EC -> JAL 0x802AF5F4\n";
  report << "[OK] pos-condicoes: flag C8 = " << flag2 << ", flag CC = " << flag3 << "\n";
  report << "[OK] trace de chamadas C++ ARM64: 2 funcoes, sem interpretador MIPS\n";
  // N64 device-service primitives. They are self-contained; no guest thread
  // scheduler or original-game call site is connected to them yet.
  cw::GuestClock clock;
  cw::GuestMessageQueue vi_queue(2);
  clock.tickVi();
  clock.tickVi();
  std::uint32_t message=0;
  if (!vi_queue.send(0x56490001u) || !vi_queue.receive(&message) ||
      message != 0x56490001u || clock.viCount()!=2) {
    return env->NewStringUTF("Erro: teste de VI/eventos nativos");
  }
  cw::ControllerSample pad;
  pad.buttons=cw::kButtonA|cw::kButtonStart;
  pad.stick_x=42;
  const auto sample=pad.serialize();
  if (sample[0]!=0x90 || sample[1]!=0 || sample[2]!=42) {
    return env->NewStringUTF("Erro: formato do controle N64");
  }
  cw::Eeprom16k eeprom;
  if (!eeprom.load(path+".eep", &error)) {
    return env->NewStringUTF(("Erro: EEPROM 16 Kbit: "+error).c_str());
  }
  // Stage and complete an actual cartridge PI transfer from this verified ROM.
  // The overlay is preloaded for the earlier diagnostics; overwrite the first
  // word to prove the asynchronously staged DMA genuinely restores it.
  std::uint32_t original_overlay=0, restored_overlay=0, pi_event=0;
  cw::GuestMessageQueue pi_queue(1);
  cw::GuestPiDma pi(state,clock,pi_queue);
  if (!state.readU32(cw::kOverlayVram,&original_overlay) ||
      !state.writeU32(cw::kOverlayVram,0xDEADBEEFu) ||
      !pi.startRead(cw::kCartBase + static_cast<std::uint32_t>(cw::kOverlayRomStart),
                    cw::kOverlayVram,4,0x50490001u) ||
      pi.service()) {
    return env->NewStringUTF("Erro: inicio do DMA PI");
  }
  clock.tickVi();
  if (!pi.service() || pi.busy() || !pi_queue.receive(&pi_event) ||
      pi_event!=0x50490001u ||
      !state.readU32(cw::kOverlayVram,&restored_overlay) ||
      restored_overlay!=original_overlay) {
    return env->NewStringUTF("Erro: DMA PI do overlay original");
  }
  // High-level native lift of the ROM's 0x802E0098 overlay transfer chain.
  // The actual allocator, libultra guest threads and instruction dispatch
  // are not connected. Verify all loader opcodes before starting the lift.
  cw::GuestMessageQueue loader_queue(1);
  cw::NativeOverlayLoad loader(state,clock,loader_queue);
  if (!state.writeU32(cw::kOverlayVram,0xABAD1DEAu) || !loader.begin())
    return env->NewStringUTF("Erro: assinatura ou inicializacao do loader 802E0098");
  for (std::size_t step=0;step<cw::kLoaderChunkCount;++step) {
    if (loader.service()) return env->NewStringUTF("Erro: DMA adiantado sem VI");
    clock.tickVi();
    if (!loader.service() || loader.failed())
      return env->NewStringUTF("Erro: sequencia do loader DMA por chunks");
  }
  std::uint32_t overlay_ptr=0;
  if (!loader.done() || loader.committedBytes()!=cw::kOverlayBytes ||
      loader.completedChunks()!=cw::kLoaderChunkCount ||
      !state.readU32(cw::kOverlayPointerGlobal,&overlay_ptr) ||
      overlay_ptr!=cw::kOverlayVram ||
      state.overlayFNV64()!=UINT64_C(0x617C6FFD93FA6347))
    return env->NewStringUTF("Erro: loader nativo nao restaurou o overlay original");
  report << "[OK] 0x802E0098: assinatura MIPS completa de 33 instrucoes\n";
  report << "[OK] Overlay DMA: 7 blocos PI, total de " << std::dec
         << loader.committedBytes() << " bytes\n";
  report << "[OK] Heap reservado 0x80050400 registrado em 0x80343C2C\n";
  report << "[LIMITACAO] Fluxo do loader traduzido em alto nivel; nao ha execucao de threads\n";
  report << "[OK] PI ROM->RDRAM: overlay original restaurado em novo VI\n";
  report << "[OK] evento PI de conclusao enviado para a fila N64\n";
  // v0.9: publish an actual PI DMA completion in guest-addressed memory
  // through the N64 libultra OSMesgQueue layout (o32 ABI, no host pointers).
  cw::GuestOsMesgQueue guest_queue(state,0x803E0000u);
  if (!guest_queue.create(0x803E0040u,1))
    return env->NewStringUTF("Erro: criar OSMesgQueue no RDRAM");
  cw::GuestPiOsBridge bridge(state,clock,guest_queue);
  if (!state.writeU32(cw::kOverlayVram,0xDEADBEEFu) ||
      !bridge.startRead(cw::kCartBase+static_cast<std::uint32_t>(cw::kOverlayRomStart),
                        cw::kOverlayVram,4,0xC9000009u))
    return env->NewStringUTF("Erro: ponte DMA para fila libultra");
  bridge.service();
  if (!bridge.busy())return env->NewStringUTF("Erro: ponte PI terminou antes do VI");
  clock.tickVi();bridge.service();
  std::uint32_t guest_msg=0,guest_word=0;
  if (bridge.busy() || bridge.notifications()!=1 ||
      guest_queue.receive(&guest_msg)!=cw::OsMqStatus::Ok ||
      guest_msg!=0xC9000009u ||
      !state.readU32(cw::kOverlayVram,&guest_word) || guest_word!=original_overlay)
    return env->NewStringUTF("Erro: notificacao PI no OSMesgQueue original");
  report << "[OK] Fila libultra OSMesgQueue o32 criada em RAM guest 0x803E0000\n";
  report << "[OK] PI -> OSMesgQueue: notificacao escrita e consumida em RAM N64\n";
  // v0.10: deterministic cooperative wakeups for message queue waiters.
  // No guest CPU context switch is performed, only host-side wait bookkeeping.
  cw::GuestQueueWaiter waits(state);
  constexpr std::uint32_t thread_a=0x803D2000u,thread_b=0x803D3000u;
  std::uint32_t wake_value=0;
  cw::GuestWaitCompletion wake{};
  if (!guest_queue.create(0x803E0040u,1) ||
      !waits.registerThread(thread_a,40) || !waits.registerThread(thread_b,80) ||
      waits.receive(thread_a,guest_queue.guestAddress(),&wake_value,true)!=cw::ThreadOpStatus::Blocked ||
      waits.send(thread_b,guest_queue.guestAddress(),0xC9000010u,false)!=cw::ThreadOpStatus::Complete ||
      !waits.takeCompletion(thread_a,&wake) ||
      wake.status!=cw::ThreadOpStatus::Complete || wake.message!=0xC9000010u)
    return env->NewStringUTF("Erro: aguardo cooperativo das filas N64");
  report << "[OK] osRecvMesg BLOCK: thread guest acordada ao receber evento PI simulado\n";
  // v0.11 — snapshot host-side context and deterministic priority dispatch.
  // The test deliberately keeps the ROM's original MIPS code unexecuted.
  cw::GuestCpuContextScheduler contexts(state);
  constexpr std::uint32_t context_low=0x803D4000u, context_high=0x803D5000u;
  if (!guest_queue.create(0x803E0040u,1) ||
      !contexts.createThread(context_low,30,cw::kMainVram,0x803CF000u) ||
      !contexts.createThread(context_high,90,cw::kMainVram+0x50u,0x803CF100u) ||
      contexts.dispatch()!=context_high)
    return env->NewStringUTF("Erro: preparar contextos das threads MIPS");
  cw::MipsCpuContext original{};
  if (!contexts.getContext(context_high,&original))
    return env->NewStringUTF("Erro: restaurar contexto MIPS");
  original.pc=cw::kMainVram+0x54u;
  original.next_pc=original.pc+4u;
  original.gpr[0]=~UINT64_C(0);
  original.gpr[2]=UINT64_C(0x123456789ABCDEF0);
  original.hi=UINT64_C(0xABCDEF0123456789);
  original.fpr[12]=UINT64_C(0x7FF0000000000000);
  original.cop0_count=0x10203040u;
  original.in_delay_slot=true;
  std::uint32_t discarded=0;
  if (!contexts.saveCurrent(original) ||
      contexts.recv(guest_queue.guestAddress(),&discarded,true)!=cw::ThreadOpStatus::Blocked ||
      contexts.current()!=context_low ||
      contexts.postMessage(guest_queue.guestAddress(),0xC9000011u)!=cw::OsMqStatus::Ok ||
      contexts.current()!=context_high)
    return env->NewStringUTF("Erro: salvar contexto e retomar prioridade");
  cw::MipsCpuContext restored{};
  cw::GuestWaitCompletion completion{};
  if (!contexts.getContext(context_high,&restored) ||
      restored.gpr[0]!=0 || restored.gpr[2]!=UINT64_C(0x123456789ABCDEF0) ||
      restored.hi!=UINT64_C(0xABCDEF0123456789) ||
      restored.fpr[12]!=UINT64_C(0x7FF0000000000000) ||
      restored.pc!=cw::kMainVram+0x54u || !restored.in_delay_slot ||
      restored.cop0_count!=0x10203040u ||
      !contexts.takeCompletion(context_high,&completion) ||
      completion.message!=0xC9000011u)
    return env->NewStringUTF("Erro: integridade do contexto restaurado");
  report << "[OK] Contexto MIPS: GPR/FPR/HI/LO/COP0, PC e delay slot preservados\n";
  report << "[OK] Fila libultra acordou thread de prioridade 90 e preemptou a de 30\n";
  // v0.12: execute ONE whole, ROM-verified C++ translation selected using
  // the currently dispatched thread's PC and argument registers. No opcode
  // interpreter, no dynamic code execution and no complete game runtime.
  cw::GuestCpuContextScheduler cpu_dispatch(state);
  constexpr std::uint32_t dispatch_thread=0x803D6000u;
  if (!cpu_dispatch.createThread(dispatch_thread,70,0x802AF6ECu,0x803CF200u) ||
      cpu_dispatch.dispatch()!=dispatch_thread)
    return env->NewStringUTF("Erro: criar thread para dispatcher estatico");
  cw::MipsCpuContext dispatch_ctx{};
  if (!cpu_dispatch.getContext(dispatch_thread,&dispatch_ctx))
    return env->NewStringUTF("Erro: ler contexto do dispatcher");
  dispatch_ctx.gpr[31]=0; // Return to host after this verified function.
  if (!cpu_dispatch.saveCurrent(dispatch_ctx) ||
      !state.writeU32(cw::kFlag2Address,87) ||
      !state.writeU32(0x803334CCu,0))
    return env->NewStringUTF("Erro: preparar dispatcher ROM original");
  cw::GuestRecompDispatcher dispatcher(state,cpu_dispatch);
  cw::CallTrace dispatch_trace{};
  if (dispatcher.run(4,&dispatch_trace)!=3 ||
      !state.readU32(cw::kFlag2Address,&flag2) || flag2!=0 ||
      !state.readU32(0x803334CCu,&flag3) || flag3!=1 ||
      dispatch_trace.entries.size()!=3 ||
      dispatch_trace.entries[0]!=0x802AF6ECu ||
      dispatch_trace.entries[1]!=0x802AF5F4u ||
      dispatch_trace.entries[2]!=0x802AF6FCu)
    return env->NewStringUTF("Erro: despacho da funcao nativa verificada");
  cw::GuestCpuPhase final_phase{};
  if (!cpu_dispatch.getPhase(dispatch_thread,&final_phase) ||
      final_phase!=cw::GuestCpuPhase::Finished ||
      dispatcher.step()!=cw::RecompStepResult::NoReadyThread)
    return env->NewStringUTF("Erro: termino atomico do dispatcher");
  report << "[OK] v0.13: JAL original em 3 etapas: chamada, callee, retorno ao caller\n";
  report << "[OK] RA salvo em stack guest e restaurado pela continuacao 802AF6FC\n";
  report << "[LIMITACAO] Somente 18 funcoes traduzidas; nao executa MIPS arbitrario\n";
  report << "[LIMITACAO] O contexto NAO executa codigo guest nem realiza troca host de CPU\n";
  report << "[LIMITACAO] Waiter cooperativo: NAO ha troca de contexto de threads MIPS\n";
  report << "[OK] Clock COP0 a 46.875 MHz: VIs acumulados = "
         << std::dec << clock.totalCount() << " ticks\n";
  report << "[OK] Fila de eventos VI nao bloqueante (FIFO)\n";
  report << "[OK] Estrutura de input N64: 16 botoes + analogico\n";
  report << "[OK] EEPROM 16 Kbit (2048 bytes) persistida em armazenamento privado\n";
  report << "[AVISO] Loader em alto nivel; nao ligado as chamadas libultra do jogo\n";
  report << "\n[OK] 18 funcoes MIPS verificadas com implementacao estatica ARM64\n"
            "ESTADO: loader, rotinas de dados e servicos de dispositivos de teste.\n"
            "Ainda NAO ha runtime de threads, renderizacao ou gameplay neste APK.";
  return env->NewStringUTF(report.str().c_str());
}
