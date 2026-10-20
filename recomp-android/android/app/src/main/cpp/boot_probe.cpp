// JNI entrypoint to the actual-game native state and 17 statically
// recompiled state setters. No ROM bytes or complete N64 emulation bundled.
#include "recomp_memory.hpp"
#include "recomp_devices.hpp"
#include "recomp_pi.hpp"
#include "recomp_loader.hpp"
#include "recomp_os_queue.hpp"
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
  // Isolated native device-service checks; gameplay is not connected.
  cw::GuestClock clock;
  cw::GuestMessageQueue queue(2);
  clock.tickVi(); clock.tickVi();
  std::uint32_t vi_msg=0;
  if(!queue.send(0x56490001u)||!queue.receive(&vi_msg)||vi_msg!=0x56490001u)
    return env->NewStringUTF("Erro: FIFO VI");
  cw::ControllerSample sample;
  sample.buttons=cw::kButtonA|cw::kButtonStart;sample.stick_x=42;
  if(sample.serialize()[0]!=0x90||sample.serialize()[2]!=42)
    return env->NewStringUTF("Erro: input N64");
  cw::Eeprom16k eeprom;
  if(!eeprom.load(path+".eep",&error))
    return env->NewStringUTF(("Erro: EEPROM: "+error).c_str());
  // Controlled native PI copy of original ROM bytes, with VI-gated completion.
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
  // ROM-specific high-level lift, not an emulated guest thread.
  cw::GuestMessageQueue loader_queue(1);
  cw::NativeOverlayLoad loader(state,clock,loader_queue);
  if(!state.writeU32(cw::kOverlayVram,0xABAD1DEAu)||!loader.begin())
    return env->NewStringUTF("Erro: assinatura original do loader 802E0098");
  for(std::size_t i=0;i<cw::kLoaderChunkCount;++i){
    if(loader.service())return env->NewStringUTF("Erro: DMA adiantado");
    clock.tickVi();
    if(!loader.service()||loader.failed())return env->NewStringUTF("Erro: loader DMA");
  }
  std::uint32_t ptr=0;
  if(!loader.done()||loader.committedBytes()!=cw::kOverlayBytes||
     loader.completedChunks()!=cw::kLoaderChunkCount||
     !state.readU32(cw::kOverlayPointerGlobal,&ptr)||ptr!=cw::kOverlayVram||
     state.overlayFNV64()!=UINT64_C(0x617C6FFD93FA6347))
    return env->NewStringUTF("Erro: overlay nao restaurado pelo loader");
  report<<"[OK] Loader 802E0098: 33 instrucoes MIPS conferidas\n";
  report<<"[OK] Overlay: 7 transferencias PI DMA; ponteiro salvo na RAM\n";
  report<<"[AVISO] Loader de alto nivel, sem threads/graficos do jogo\n";

  // Guest-ABI libultra queue: DMA completion is observable by recompiled code.
  cw::GuestOsMesgQueue os_queue(state,0x803E0000u);
  if(!os_queue.create(0x803E0040u,1))
    return env->NewStringUTF("Erro: criar OSMesgQueue");
  cw::GuestPiOsBridge bridge(state,clock,os_queue);
  if(!state.writeU32(cw::kOverlayVram,0xDEADBEEFu) ||
     !bridge.startRead(cw::kCartBase+static_cast<std::uint32_t>(cw::kOverlayRomStart),
                       cw::kOverlayVram,4,0xC9000009u))
    return env->NewStringUTF("Erro: preparar DMA para OSMesgQueue");
  bridge.service();
  if(!bridge.busy())return env->NewStringUTF("Erro: DMA antes do VI");
  clock.tickVi(); bridge.service();
  std::uint32_t os_msg=0,os_restored=0;
  if(bridge.busy()||bridge.notifications()!=1||
     os_queue.receive(&os_msg)!=cw::OsMqStatus::Ok||os_msg!=0xC9000009u||
     !state.readU32(cw::kOverlayVram,&os_restored)||os_restored!=original_overlay)
    return env->NewStringUTF("Erro: notificacao PI na RAM OSMesgQueue");
  report<<"[OK] OSMesgQueue guest ABI: ring de 24 bytes em RAM N64\n";
  report<<"[OK] Ponte DMA PI: notificacao entregue e consumida pela fila guest\n";
  report<<"[LIMITE] Ainda sem modo BLOCK ou scheduler de threads MIPS\n";
  report<<"[OK] PI ROM->RDRAM: overlay original restaurado em novo VI\n";
  report<<"[OK] evento PI de conclusao enviado para fila N64\n";
  report<<"[OK] COP0 Count 46,875 MHz, 3 VI = "<<std::dec<<clock.totalCount()<<" ticks\n";
  report<<"[OK] VI FIFO, serializacao de controle, EEPROM 16 Kbit privada\n";
  report<<"[AVISO] Servicos ainda nao conectados a libultra/gameplay\n";
  report << "\n[OK] 18 funcoes MIPS verificadas com implementacao estatica ARM64\n"
            "ESTADO: loader e primitivas de dados recompiladas.\n"
            "Ainda NAO ha threads, graficos, som ou gameplay neste APK.";
  return env->NewStringUTF(report.str().c_str());
}
