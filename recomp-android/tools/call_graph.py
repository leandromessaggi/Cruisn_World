#!/usr/bin/env python3
"""Locate actual MIPS JAL callsites for statically translated functions.

Only scans the ROM's known MIPS executable regions. Does not execute MIPS or
extract copyright-controlled assets. Candidate callsites are not proof of
runtime reachability: reaching gameplay needs the full N64 runtime.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from rom_probe import load_rom, EXPECTED_SHA256, EXPECTED_SIZE

SECTIONS = ((0x1000, 0x1038, 0x802AD400),
            (0x1050, 0x84E20, 0x802AD450),
            (0xA7040, 0xBFE90, 0x80050400))

def collect_calls(rom: bytes, targets: set[int]):
    hits = []
    for start, end, vram in SECTIONS:
        for off in range(start, end, 4):
            opcode, = struct.unpack_from('>I', rom, off)
            if opcode >> 26 != 0x03:  # MIPS jal immediate
                continue
            pc = vram + (off - start)
            callee = ((pc + 4) & 0xF0000000) | ((opcode & 0x03FFFFFF) << 2)
            if callee in targets:
                hits.append({'rom_offset':f'0x{off:06X}',
                             'caller_instruction':f'0x{pc:08X}',
                             'target':f'0x{callee:08X}'})
    return hits

def inspect(rom: bytes, catalog):
    if len(rom) != EXPECTED_SIZE or hashlib.sha256(rom).hexdigest() != EXPECTED_SHA256:
        raise ValueError('Requires the supported, exact US ROM')
    targets = {int(fn['vram'], 16) for fn in catalog['functions']}
    calls = collect_calls(rom, targets)
    # MIPS JAL at 802AF6F4 is a verified static C++-recompiled call edge.
    assert {'rom_offset':'0x0032F4','caller_instruction':'0x802AF6F4',
            'target':'0x802AF5F4'} in calls
    return {'source_sha256':EXPECTED_SHA256, 'translated_targets':len(targets),
            'direct_jal_callsites':len(calls), 'calls':calls,
            'warning':'Static JAL candidates; not complete control flow or a runtime.'}

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('rom', help='user owned supported USA ROM / ZIP')
    parser.add_argument('--output', default='reports/direct_call_graph.json')
    args = parser.parse_args()
    catalog = json.loads(Path(__file__).with_name('recompiled_functions.json').read_text())
    rom, _, _ = load_rom(args.rom)
    result = inspect(rom, catalog)
    output = Path(args.output); output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(result['translated_targets'], 'native targets,',result['direct_jal_callsites'], 'JAL callsites found')
