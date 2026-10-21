#include "cic6106_seed.hpp"
#include <array>

namespace cw {
namespace {
struct BootWord { std::uint32_t address, value; };
// Discovered in the original ROM's hardware checks. These are register / boot
// state constants, not ROM data. SP DMEM and SP IMEM are separate peripheral
// windows; never write them into ordinary 8 MiB RDRAM.
constexpr std::array<BootWord, 8> kWorldUsState{{
    {0x80000164u, 0x01EC6021u},
    {0x800001BCu, 0x8FBF001Cu},
    {0x80000234u, 0xAD130004u},
    {0x8000028Cu, 0xAD0AFFFCu},
    {0x800002B8u, 0x8941680Cu},
    {0x80000310u, 0x000017D8u},
    {0xA4000000u, 0xFFFFFFFFu}, // SP DMEM
    {0xA4001000u, 0x000017D8u}, // SP IMEM
}};
constexpr std::size_t kRomSize = 12u * 1024u * 1024u;
}
bool seed_world_us_cic6106(const Cic6106Writer& dst,
                           const std::uint8_t* rom, std::size_t rom_size) noexcept {
    if (!dst.write_word || !dst.write_bytes || !rom || rom_size != kRomSize)
        return false;
    // USA NTSC big-endian .z64, game id NCWE and documented checksum.
    constexpr std::array<std::uint8_t, 4> magic{{0x80u,0x37u,0x12u,0x40u}};
    constexpr std::array<std::uint8_t, 4> game{{'N','C','W','E'}};
    constexpr std::array<std::uint8_t, 8> checksum{{0xDF,0xE6,0x11,0x53,0xD7,0x61,0x18,0xE6}};
    for (std::size_t i=0;i<4;++i)
        if (rom[i]!=magic[i] || rom[0x3Bu+i]!=game[i]) return false;
    for (std::size_t i=0;i<8;++i)
        if (rom[0x10u+i]!=checksum[i]) return false;
    for (const auto& word: kWorldUsState)
        if (!dst.write_word(dst.user, word.address, word.value)) return false;
    // The game's IPL3 checks read the 0x90000000 KSEG0 cartridge mapping.
    // The original port's runtime maps the first 0x1000 bytes of ROM there.
    return dst.write_bytes(dst.user, 0x90000000u, rom, 0x1000u);
}
} // namespace cw
