#include "recomp_loader.hpp"
#include <algorithm>
namespace cw {
// All 33 MIPS opcodes of func_802E0098 in the original USA ROM.
const std::array<std::uint32_t,33>& originalLoaderInstructions(){
 static constexpr std::array<std::uint32_t,33> words={{
  0x27BDFFE0u,0xAFBF0014u,0x0C0B8018u,0x00000000u,
  0x3C0E000Cu,0x3C0F000Au,0x25EF7040u,0x25CE12A0u,
  0x01CF3823u,0x00E02025u,0xAFA7001Cu,0x2405003Au,
  0x0C0B890Cu,0x00003025u,0x3C038034u,0x3C048005u,
  0x34840400u,0x24633C2Cu,0xAC620000u,0x10820003u,
  0x8FA7001Cu,0x1000FFFFu,0x00000000u,0x3C05000Au,
  0x24A57040u,0x0C0AB558u,0x00E03025u,0x0C0ABF34u,
  0x00000000u,0x8FBF0014u,0x27BD0020u,0x03E00008u,
  0x00000000u
 }};
 return words;
}
NativeOverlayLoad::NativeOverlayLoad(NativeState& s,const GuestClock& clock,GuestMessageQueue& queue)
 : state_(s),pi_(s,clock,queue),completion_(queue){}
bool NativeOverlayLoad::begin(){
 if(!state_.valid()||active_||failed_||completion_.capacity()==0||completion_.size()!=0)return false;
 const auto& words=originalLoaderInstructions();
 if(!state_.romMatches(kLoaderRomOffset,words.data(),words.size())||
    !state_.canDmaRead(kOverlayRomStart,kOverlayVram,kOverlayBytes))return false;
 // Verified fixed allocation address from the game's own boot flow;
 // original allocator/heap bookkeeping have NOT yet been ported.
 std::uint32_t old=0;
 if(!state_.readU32(kOverlayPointerGlobal,&old)||
    !state_.writeU32(kOverlayPointerGlobal,kOverlayVram))return false;
 active_=true;
 if(!scheduleNext()){
   state_.writeU32(kOverlayPointerGlobal,old);
   active_=false;
   return false;
 }
 return true;
}
bool NativeOverlayLoad::scheduleNext(){
 if(!active_||copied_>=kOverlayBytes)return false;
 const std::size_t n=std::min(kPiChunkBytes,kOverlayBytes-copied_);
 return pi_.startRead(kCartBase+static_cast<std::uint32_t>(kOverlayRomStart+copied_),
                      kOverlayVram+static_cast<std::uint32_t>(copied_),n,kLoaderPiEvent);
}
bool NativeOverlayLoad::service(){
 if(!active_||done_||failed_||!pi_.service())return false;
 std::uint32_t event=0;
 if(!completion_.receive(&event)||event!=kLoaderPiEvent){
   failed_=true;return false;
 }
 copied_+=std::min(kPiChunkBytes,kOverlayBytes-copied_);
 ++completed_;
 if(copied_==kOverlayBytes){
   std::uint64_t expected=0;
   done_=state_.romRangeFNV64(kOverlayRomStart,kOverlayBytes,&expected)&&
         state_.overlayFNV64()==expected;
   failed_=!done_;
   return true;
 }
 if(!scheduleNext())failed_=true;
 return true;
}
} // namespace cw
