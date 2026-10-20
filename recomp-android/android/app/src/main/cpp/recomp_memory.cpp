#include "recomp_memory.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace cw {
namespace {
std::uint32_t be32(const std::uint8_t* p) {
  return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
         (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]);
}

struct Routine {
  std::uint32_t vram;
  std::uint32_t dest;
  std::uint32_t value;
  std::array<std::uint32_t, 4> instructions;
  std::size_t words;
};

// Hand-translated from instructions in the USA game. The jr $ra delay slot
// contains the store, so native code performs that store before returning.
// These are low-level state setters, NOT a driving or gameplay engine.
constexpr Routine kRoutines[] = {
 {0x802AEE9Cu, kFlag1Address, 1u,
  {0x240E0001u, 0x3C018033u, 0x03E00008u, 0xAC2E34C0u}, 4},
 {0x802AEEACu, kFlag1Address, 0u,
  {0x3C018033u, 0x03E00008u, 0xAC2034C0u, 0u}, 3},
 {0x802AF5E4u, kFlag2Address, 1u,
  {0x240E0001u, 0x3C018033u, 0x03E00008u, 0xAC2E34C8u}, 4},
 {0x802AF5F4u, kFlag2Address, 0u,
  {0x3C018033u, 0x03E00008u, 0xAC2034C8u, 0u}, 3},
};

constexpr std::uint8_t kMagic[] = {0x80, 0x37, 0x12, 0x40};
constexpr std::uint8_t kCrcs[] = {0xDF,0xE6,0x11,0x53,0xD7,0x61,0x18,0xE6};
}  // namespace

NativeState::NativeState() : ram_(kRamSize, 0u) {}

bool NativeState::offset(std::uint32_t vram, std::size_t size, std::size_t* out) const {
  if (vram < kRamBase) return false;
  const std::size_t index = static_cast<std::size_t>(vram - kRamBase);
  if (index > ram_.size() || size > ram_.size() - index) return false;
  *out = index;
  return true;
}

bool NativeState::load(const std::string& rom_path, std::string* error) {
  valid_ = false;
  std::ifstream in(rom_path, std::ios::binary | std::ios::ate);
  if (!in || in.tellg() != static_cast<std::streampos>(kRomSize)) {
    if (error) *error = "ROM deve ter exatamente 12 MiB no formato z64";
    return false;
  }
  in.seekg(0);
  // This tool requires the normalized user-owned ROM, and never persists it
  // in the source tree or compiled APK.
  std::vector<std::uint8_t> rom(kRomSize);
  in.read(reinterpret_cast<char*>(rom.data()), static_cast<std::streamsize>(rom.size()));
  if (!in || !std::equal(std::begin(kMagic), std::end(kMagic), rom.begin()) ||
      !std::equal(std::begin(kCrcs), std::end(kCrcs), rom.begin() + 0x10)) {
    if (error) *error = "Cabecalho ou CRCs nao correspondem ao dump suportado";
    return false;
  }
  // Verify all translated instruction bytes against the ACTUAL selected ROM.
  for (const auto& fn : kRoutines) {
    const std::size_t source = kMainRomStart + static_cast<std::size_t>(fn.vram - kMainVram);
    for (std::size_t i = 0; i != fn.words; ++i) {
      if (be32(&rom[source + i * 4]) != fn.instructions[i]) {
        if (error) *error = "Rotina MIPS de pesquisa divergiu do arquivo selecionado";
        return false;
      }
    }
  }
  std::size_t main_offset = 0, bss_offset = 0, overlay_offset = 0;
  if (!offset(kMainVram, kMainRomEnd-kMainRomStart, &main_offset) ||
      !offset(kBssStart, kBssSize, &bss_offset) ||
      !offset(kOverlayVram, kOverlayRomEnd-kOverlayRomStart, &overlay_offset)) {
    if (error) *error = "Mapa de memoria MIPS invalido";
    return false;
  }
  std::fill(ram_.begin(), ram_.end(), 0);
  std::copy(rom.begin() + kMainRomStart, rom.begin() + kMainRomEnd,
            ram_.begin() + main_offset);
  // Actual effect of the boot loop at 0x802AD410, compiled into C++.
  std::fill(ram_.begin() + bss_offset, ram_.begin() + bss_offset + kBssSize, 0);
  // Actual main-thread overlay range and destination from func_802E0098.
  std::copy(rom.begin() + kOverlayRomStart, rom.begin() + kOverlayRomEnd,
            ram_.begin() + overlay_offset);
  valid_ = true;
  return true;
}

bool NativeState::readU32(std::uint32_t vram, std::uint32_t* out) const {
  std::size_t index;
  if (!valid_ || !out || !offset(vram, 4, &index)) return false;
  *out = be32(ram_.data() + index);
  return true;
}

bool NativeState::writeU32(std::uint32_t vram, std::uint32_t value) {
  std::size_t index;
  if (!valid_ || !offset(vram, 4, &index)) return false;
  for (int i = 0; i < 4; ++i) ram_[index + i] = static_cast<std::uint8_t>(value >> (24-8*i));
  return true;
}

bool NativeState::isZero(std::uint32_t address, std::size_t size) const {
  std::size_t index;
  if (!valid_ || !offset(address, size, &index)) return false;
  return std::all_of(ram_.begin()+index, ram_.begin()+index+size,
                     [](std::uint8_t x){ return x == 0; });
}

std::uint64_t NativeState::overlayFNV64() const {
  if (!valid_) return 0;
  std::size_t index;
  if (!offset(kOverlayVram, kOverlayRomEnd-kOverlayRomStart, &index)) return 0;
  std::uint64_t hash = UINT64_C(14695981039346656037);
  for (std::size_t i = 0; i < kOverlayRomEnd-kOverlayRomStart; ++i) {
    hash ^= ram_[index+i];
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

bool NativeState::callRecompiled(std::uint32_t function_vram) {
  if (!valid_) return false;
  for (const auto& f : kRoutines) {
    if (f.vram == function_vram) return writeU32(f.dest, f.value);
  }
  return false;  // Deliberately do not interpret or execute unknown MIPS.
}

std::string NativeState::diagnostics() const {
  std::ostringstream log;
  log << "Cruis'n World US — milestone recompilacao nativa\n\n";
  if (!valid_) return "ROM nao inicializada.\n";
  log << "[OK] ROM mapeada: 0x" << std::uppercase << std::hex << kMainVram
      << " — main 0x" << (kMainRomEnd-kMainRomStart) << " bytes\n";
  log << "[OK] BSS limpa: 0x" << kBssStart << ", " << std::dec
      << kBssSize << " bytes: " << (isZero(kBssStart,kBssSize)?"sim":"nao") << "\n";
  log << std::hex << std::uppercase << "[OK] Overlay real: ROM 0x" << kOverlayRomStart
      << "-0x" << kOverlayRomEnd << " => RAM 0x" << kOverlayVram << "\n";
  log << "[OK] Overlay FNV64 = " << std::setfill('0') << std::setw(16) << overlayFNV64() << "\n";
  const std::array<std::uint32_t,4> calls{{0x802AEE9Cu,0x802AEEACu,0x802AF5E4u,0x802AF5F4u}};
  for (const auto addr : calls) {
    log << "[ARM64] Funcao recompilada 0x" << addr
        << " (setter de memoria; nao gameplay)\n";
  }
  log << "\nProximo: interrupcoes, threads, RSP/RDP, display lists, audio, controles e jogo.\n"
         "Este APK NAO executa a corrida.";
  return log.str();
}
}  // namespace cw
