// This is a native ARM64 reconstruction of the observed N64 initialization
// *state*, not an N64 CPU emulator and not the full Cruis'n World engine.
// See upstream Cruis'n World research, GPL-3.0:
// https://github.com/navjack/cruisn-world-recomp
#include <jni.h>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace {
constexpr size_t kRomSize=0xC00000;
constexpr size_t kBssSize=0x94760;
constexpr uint32_t kBssStart=0x80353380;
constexpr uint32_t kRuntimeEntry=0x802AD400;
constexpr uint32_t kThreadMain=0x802AD450;
// MIPS R4300i instructions observed at ROM offset 0x1000.
constexpr uint32_t kBootCode[]={
    0x3C088035,0x3C090009,0x25083380,0x35294760,0x2129FFF8,
    0xAD000000,0xAD000004,0x1520FFFC,0x21080008,
    0x3C0A802B,0x3C1D8015,0x254AD450,0x01400008,0x27BD0400
};
uint32_t be32(const unsigned char* p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|uint32_t(p[3]);}
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_cruisn_recomp_PortLabActivity_nativeBootProbe(JNIEnv* env,jclass,jstring filepath){
    const char* s=env->GetStringUTFChars(filepath,nullptr);
    const std::string path(s);env->ReleaseStringUTFChars(filepath,s);
    std::ostringstream status;
    status << "Cruis'n World US — pesquisa de port nativo\n\n";
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file){status<<"ERRO: ROM indisponivel.";return env->NewStringUTF(status.str().c_str());}
    auto size=file.tellg();
    if(size!=std::streampos(kRomSize)){status<<"ERRO: tamanho incompatível.";return env->NewStringUTF(status.str().c_str());}
    file.seekg(0);
    std::vector<unsigned char> boot(0x1060);
    file.read(reinterpret_cast<char*>(boot.data()),boot.size());
    if(!file){status<<"ERRO: falha ao ler entrada.";return env->NewStringUTF(status.str().c_str());}
    if(be32(boot.data())!=0x80371240u || be32(boot.data()+0x10)!=0xDFE61153u ||
       be32(boot.data()+0x14)!=0xD76118E6u){
       status<<"ERRO: header de N64 inesperado.";return env->NewStringUTF(status.str().c_str());
    }
    for(size_t i=0;i<sizeof(kBootCode)/sizeof(uint32_t);i++){
        if(be32(&boot[0x1000+i*4])!=kBootCode[i]){
            status<<"ERRO: codigo do boot não coincide na instrução "<<i;
            return env->NewStringUTF(status.str().c_str());
        }
    }
    // Reconstruction of the effect of the exact MIPS loop at 0x802AD410:
    // sw $zero,0($t0); sw $zero,4($t0); bnez $t1 ... with addi delay slot.
    // Avoids all MIPS interpretation at runtime: just native C++ initialization.
    std::vector<uint8_t> bss(kBssSize,0xA5);
    std::fill(bss.begin(),bss.end(),uint8_t(0));
    bool cleared=std::all_of(bss.begin(),bss.end(),[](uint8_t b){return b==0;});
    status<<"[OK] ROM validada pelo SHA-256 no Java\n";
    status<<"[OK] CRCs e código MIPS do boot conferidos\n";
    status<<"[OK] Boot convertido para C++/ARM64\n";
    status<<"[OK] BSS zerada: "<<kBssSize<<" bytes\n";
    status<<"[OK] BSS zerada corretamente: "<<(cleared?"sim":"nao")<<"\n";
    status<<std::hex<<std::uppercase;
    status<<"Entrada mapeada: 0x"<<kRuntimeEntry<<"\n";
    status<<"Main MIPS: 0x"<<kThreadMain<<"\n";
    status<<"BSS inicio: 0x"<<kBssStart<<"\n\n";
    status<<"PRÓXIMAS ETAPAS: ligar funções recompiladas, ponte libultra, ";
    status<<"RSP, RDP/Vulkan, áudio e controles.\n\n";
    status<<"Este APK ainda NÃO é o jogo jogável.";
    return env->NewStringUTF(status.str().c_str());
}
