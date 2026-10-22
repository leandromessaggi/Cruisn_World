#include "recomp_vi_events.hpp"
#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_com_cruisn_recomp_PortLabActivity_nativeViProbe(JNIEnv* env, jclass,
                                                      jstring rom_path) {
  if (!rom_path) return env->NewStringUTF("[ERRO] VI: ROM nao selecionada");
  const char* chars=env->GetStringUTFChars(rom_path,nullptr);
  if (!chars) return nullptr; // pending Java OOME
  const std::string path(chars);
  env->ReleaseStringUTFChars(rom_path,chars);
  cw::NativeState state;
  std::string error;
  if (!state.load(path,&error))
    return env->NewStringUTF(("[ERRO] VI: "+error).c_str());
  constexpr std::uint32_t queue_address=0x803E0200u;
  cw::GuestOsMesgQueue queue(state,queue_address);
  cw::GuestCpuContextScheduler contexts(state);
  cw::GuestClock clock;
  cw::GuestViEventBridge vi(state,clock,contexts);
  constexpr std::uint32_t low=0x803D7000u, high=0x803D8000u;
  std::uint32_t received=0;
  if (!queue.create(0x803E0240u,1) ||
      !contexts.createThread(low,20,cw::kMainVram,0x803CF400u) ||
      !contexts.createThread(high,80,cw::kMainVram+0x50u,0x803CF500u) ||
      contexts.dispatch()!=high ||
      contexts.recvGuest(queue_address,0x803E0280u,true)!=cw::ThreadOpStatus::Blocked ||
      contexts.current()!=low || !vi.setRetrace(queue_address,0x29Au,1) ||
      cw::GuestViHostPacer::runTicks(vi,1)!=1 ||
      vi.delivered()!=1 || contexts.current()!=high)
    return env->NewStringUTF("[ERRO] VI: entrega do evento ou prioridade");
  cw::GuestWaitCompletion completion{};
  if (!contexts.takeCompletion(high,&completion) ||
      completion.status!=cw::ThreadOpStatus::Complete ||
      completion.operation!=cw::ThreadWaitKind::Receive ||
      completion.message!=0x29Au || vi.delivered()!=1 || vi.dropped()!=0)
    return env->NewStringUTF("[ERRO] VI: retorno da fila de mensagens");
  std::uint32_t guest_received=0;
  cw::MipsCpuContext resumed{};
  if (!state.readU32(0x803E0280u,&guest_received) || guest_received!=0x29Au ||
      !contexts.getContext(high,&resumed) || resumed.gpr[2]!=0)
    return env->NewStringUTF("[ERRO] VI: destino guest ou retorno MIPS v0");
  // Continue the bounded 60 Hz host-clock diagnostic after the wakeup.
  // The first unconsumed event fills the single-slot queue; subsequent
  // nonblocking VI events must be dropped without overwriting it.
  if (cw::GuestViHostPacer::runTicks(vi,4)!=4 ||
      clock.viCount()!=5 || vi.delivered()!=2 || vi.dropped()!=3 ||
      queue.receive(&received)!=cw::OsMqStatus::Ok || received!=0x29Au)
    return env->NewStringUTF("[ERRO] VI: temporizacao ou fila cheia");
  return env->NewStringUTF(
      "[OK] v0.29: VI host ~60 Hz -> osRecvMesg guest -> prioridade 80\n"
      "[OK] MIPS v0=0, mensagem em RDRAM, 5 ticks, 2 entregas, 3 descartes\n"
      "[LIMITE] Diagnostico: sem interrupcao VI do N64, sem codigo original "
      "em execucao e sem corrida jogavel");
}
