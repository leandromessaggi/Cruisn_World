#include "../android/app/src/main/cpp/recomp_memory.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <cstdint>
int main(int argc,char** argv){
 if(argc!=2)return 1;
 cw::NativeState s;std::string err;
 if(!s.load(argv[1],&err)){std::cerr<<err<<"\n";return 2;}
 if(!s.isZero(cw::kBssStart,cw::kBssSize))return 3;
 std::uint32_t w;
 if(!s.readU32(cw::kMainVram,&w)||w!=0x3C088035)return 4;
 if(!s.readU32(cw::kOverlayVram,&w)||w!=0x3C0EA000)return 5;
 if(s.overlayFNV64()!=UINT64_C(0x617c6ffd93fa6347))return 6;
 for(auto a: {0x802AEE9Cu,0x802AEEACu,0x802AF5E4u,0x802AF5F4u}){
  if(!s.callRecompiled(a))return 7;
 }
 if(!s.readU32(cw::kFlag1Address,&w)||w!=0)return 8;
 if(!s.readU32(cw::kFlag2Address,&w)||w!=0)return 9;
 std::cout<<"PASSED on actual Cruis'n World USA ROM: mapping, overlay, BSS, 4 statically recompiled routines\n";
 std::cout<<s.diagnostics()<<"\n";
}
