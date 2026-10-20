#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "recomp_pi.hpp"
namespace cw {
constexpr std::uint32_t kLoaderVram=0x802E0098u;
constexpr std::size_t kLoaderRomOffset=kMainRomStart+(kLoaderVram-kMainVram);
constexpr std::uint32_t kOverlayPointerGlobal=0x80343C2Cu;
constexpr std::size_t kPiChunkBytes=0x4000u;
constexpr std::uint32_t kLoaderPiEvent=0x50494C44u;
constexpr std::size_t kOverlayBytes=kOverlayRomEnd-kOverlayRomStart;
constexpr std::size_t kLoaderChunkCount=(kOverlayBytes+kPiChunkBytes-1)/kPiChunkBytes;
const std::array<std::uint32_t,33>& originalLoaderInstructions();
class NativeOverlayLoad final {
public:
  NativeOverlayLoad(NativeState& state,const GuestClock& clock,GuestMessageQueue& completion);
  bool begin();
  bool service();
  bool done() const{return done_;}
  bool busy() const{return active_&&!done_;}
  bool failed() const{return failed_;}
  std::size_t completedChunks() const{return completed_;}
  std::size_t committedBytes() const{return copied_;}
private:
  bool scheduleNext();
  NativeState& state_;
  GuestPiDma pi_;
  GuestMessageQueue& completion_;
  bool active_=false,done_=false,failed_=false;
  std::size_t completed_=0,copied_=0;
};
} // namespace cw
